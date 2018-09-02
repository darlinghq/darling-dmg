#include "DMGDisk.h"
#include <stdexcept>
#include "be.h"
#include <iostream>
#include <cstring>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <memory>
#include <sstream>
#include "DMGPartition.h"
#include "AppleDisk.h"
#include "GPTDisk.h"
#include "CachedReader.h"
#include "SubReader.h"
#include "exceptions.h"

static uint64_t findKolyBlock(std::shared_ptr<Reader> reader)
{
	if (reader->length() < 1024)
		throw io_error("File too small");

	char buf[1024];
	reader->read(buf, sizeof(buf), reader->length() - 1024);
	for (uint32_t i = 0 ; i < 1024 - sizeof(UDIFResourceFile) + 1 ; i++) {
		if (strcmp("koly", buf+i) == 0)
			return reader->length() - 1024 + i;
	}
	return 0;
}

DMGDisk::DMGDisk(std::shared_ptr<Reader> reader)
	: m_reader(reader), m_zone(40000)
{
	uint64_t offset = findKolyBlock(m_reader);

	if (offset == 0)
		throw io_error("Cannot find koly block at the end");

	if (m_reader->read(&m_udif, sizeof(m_udif), offset) != sizeof(m_udif))
		throw io_error("Cannot read the KOLY block");

	if (be(m_udif.fUDIFSignature) != UDIF_SIGNATURE)
		throw io_error("Invalid KOLY block signature");
	
	loadKoly(m_udif);
}

DMGDisk::~DMGDisk()
{
	xmlFreeDoc(m_kolyXML);
}

bool DMGDisk::isDMG(std::shared_ptr<Reader> reader)
{
	uint64_t offset = findKolyBlock(reader);
	return offset != 0;
}

void DMGDisk::loadKoly(const UDIFResourceFile& koly)
{
	std::unique_ptr<char[]> xmlData;
	xmlXPathContextPtr xpathContext;
	xmlXPathObjectPtr xpathObj;
	uint64_t offset, length;
	bool simpleWayOK = false;

	offset = be(koly.fUDIFXMLOffset);
	length = be(koly.fUDIFXMLLength);

	xmlData.reset(new char[length]);
	m_reader->read(xmlData.get(), length, offset);

	m_kolyXML = xmlParseMemory(xmlData.get(), length);

//#if 0 // Asian copies of OS X put crap UTF characters into XML data making type/name parsing unreliable
	xpathContext = xmlXPathNewContext(m_kolyXML);

	// select all partition dictionaries with partition ID >= 0
	xpathObj = xmlXPathEvalExpression((const xmlChar*) "/plist/dict/key[text()='resource-fork']/following-sibling::dict[1]/key[text()='blkx']"
			"/following-sibling::array[1]/dict[key[text()='ID']/following-sibling::string[text() >= 0]]", xpathContext);

	if (xpathObj && xpathObj->nodesetval)
		simpleWayOK = loadPartitionElements(xpathContext, xpathObj->nodesetval);
	
	xmlXPathFreeObject(xpathObj);
	xmlXPathFreeContext(xpathContext);
//#else
	
	if (!simpleWayOK)
	{
		std::shared_ptr<Reader> rm1, r1;
		PartitionedDisk* pdisk;

		rm1 = readerForKolyBlock(-1);

		if (rm1)
		{
			if (AppleDisk::isAppleDisk(rm1))
			{
				r1 = readerForKolyBlock(0); // TODO: this is not always partition 0
				pdisk = new AppleDisk(rm1, r1);
			}
			else if (GPTDisk::isGPTDisk(rm1))
			{
				r1 = readerForKolyBlock(1);
				pdisk = new GPTDisk(rm1, r1);
			}
			else
				throw function_not_implemented_error("Unknown partition table type");

			m_partitions = pdisk->partitions();

			delete pdisk;
		}	
	}
//#endif
}

bool DMGDisk::loadPartitionElements(xmlXPathContextPtr xpathContext, xmlNodeSetPtr nodes)
{
	for (int i = 0; i < nodes->nodeNr; i++)
	{
		xmlXPathObjectPtr xpathObj;
		Partition part;
		BLKXTable* table;

		if (nodes->nodeTab[i]->type != XML_ELEMENT_NODE)
			continue;

		xpathContext->node = nodes->nodeTab[i];

		xpathObj = xmlXPathEvalExpression((const xmlChar*) "string(key[text()='CFName']/following-sibling::string)", xpathContext);
		
		if (!xpathObj || !xpathObj->stringval)
			xpathObj = xmlXPathEvalExpression((const xmlChar*) "string(key[text()='Name']/following-sibling::string)", xpathContext);

		if (!xpathObj || !xpathObj->stringval)
			throw io_error("Invalid XML data, partition Name key not found");
		
		table = loadBLKXTableForPartition(i);
		
		if (table)
		{
			part.offset = be(table->firstSectorNumber) * 512;
			part.size = be(table->sectorCount) * 512;
		}

		if (!parseNameAndType((const char*) xpathObj->stringval, part.name, part.type) && m_partitions.empty())
			return false;
		m_partitions.push_back(part);

		xmlXPathFreeObject(xpathObj);
		//delete table;
	}
	
	return true;
}

bool DMGDisk::parseNameAndType(const std::string& nameAndType, std::string& name, std::string& type)
{
	// Format: "Apple (Apple_partition_map : 1)"
	size_t paren = nameAndType.find('(');
	size_t colon, space;

	if (paren == std::string::npos)
		return false;

	name = nameAndType.substr(0, paren-1);
	colon = nameAndType.find(':', paren);

	if (colon == std::string::npos)
		return false;

	type = nameAndType.substr(paren+1, (colon - paren) - 1);
	space = type.rfind(' ');
	
	if (space != std::string::npos && space == type.length()-1)
		type.resize(type.length() - 1); // remove space at the end
	
	return true;
}

BLKXTable* DMGDisk::loadBLKXTableForPartition(int index)
{
	xmlXPathContextPtr xpathContext;
	xmlXPathObjectPtr xpathObj;
	char expr[300];
	BLKXTable* rv = nullptr;

	sprintf(expr, "string(/plist/dict/key[text()='resource-fork']/following-sibling::dict[1]/key[text()='blkx']"
		"/following-sibling::array[1]/dict[key[text()='ID']/following-sibling::string[text() = %d]]/key[text()='Data']/following-sibling::data)", index);

	xpathContext = xmlXPathNewContext(m_kolyXML);
	xpathObj = xmlXPathEvalExpression((const xmlChar*) expr, xpathContext);

	if (xpathObj && xpathObj->stringval && *xpathObj->stringval)
	{
		// load data from base64
		std::vector<uint8_t> data;
		
		base64Decode((char*)xpathObj->stringval, data);
		rv = static_cast<BLKXTable*>(operator new(data.size()));
		
		memcpy(rv, &data[0], data.size());
	}

	xmlXPathFreeObject(xpathObj);
	xmlXPathFreeContext(xpathContext);
	
	return rv;
}

bool DMGDisk::base64Decode(const std::string& input, std::vector<uint8_t>& output)
{
	BIO *b64, *bmem;
	std::unique_ptr<char[]> buffer(new char[input.length()]);
	int rd;

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new_mem_buf((void*) input.c_str(), input.length());
	bmem = BIO_push(b64, bmem);
	//BIO_set_flags(bmem, BIO_FLAGS_BASE64_NO_NL);
	
	rd = BIO_read(bmem, buffer.get(), input.length());
	
	if (rd > 0)
		output.assign(buffer.get(), buffer.get()+rd);

	BIO_free_all(bmem);
	return rd >= 0;
}

std::shared_ptr<Reader> DMGDisk::readerForPartition(int index)
{
	for (int i = -1;; i++)
	{
		BLKXTable* table = loadBLKXTableForPartition(i);
		
		if (!table)
			continue;
		
		if (be(table->firstSectorNumber)*512 == m_partitions[index].offset)
		{
			std::stringstream partName;
			uint64_t l = m_reader->length();
			uint32_t data_offset = be(m_udif.fUDIFDataForkOffset);

			partName << "part-" << index;

			if (data_offset) {
				std::shared_ptr<Reader> r(new SubReader(m_reader,
					data_offset,
					m_reader->length() - data_offset));

				return std::shared_ptr<Reader>(
						new CachedReader(std::shared_ptr<Reader>(new DMGPartition(r, table)), &m_zone, partName.str())
						);
			} else {
				return std::shared_ptr<Reader>(
						new CachedReader(std::shared_ptr<Reader>(new DMGPartition(m_reader, table)), &m_zone, partName.str())
						);
			}
		}
		
		delete table;
	}
	
	return nullptr;
}

std::shared_ptr<Reader> DMGDisk::readerForKolyBlock(int index)
{
	BLKXTable* table = loadBLKXTableForPartition(index);
	if (!table)
		return nullptr;
	return std::shared_ptr<Reader>(new DMGPartition(m_reader, table));
}

