#include "DMGDisk.h"
#include <stdexcept>
#include "be.h"
#include <iostream>
#include <cstring>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <memory>
#include "DMGPartition.h"

DMGDisk::DMGDisk(Reader* reader)
	: m_reader(reader)
{
	uint64_t offset = m_reader->length();
	UDIFResourceFile udif;

	if (offset < 512)
		throw std::runtime_error("File to small to be a DMG");

	offset -= 512;

	if (m_reader->read(&udif, sizeof(udif), offset) != sizeof(udif))
		throw std::runtime_error("Cannot read the KOLY block");

	if (be(udif.fUDIFSignature) != UDIF_SIGNATURE)
		throw std::runtime_error("Invalid KOLY block signature");
	
	loadKoly(udif);
}

DMGDisk::~DMGDisk()
{
	xmlFreeDoc(m_kolyXML);
}

void DMGDisk::loadKoly(const UDIFResourceFile& koly)
{
	std::unique_ptr<char[]> xmlData;
	xmlXPathContextPtr xpathContext;
	xmlXPathObjectPtr xpathObj;
	uint64_t offset, length;

	offset = be(koly.fUDIFXMLOffset);
	length = be(koly.fUDIFXMLLength);

	xmlData.reset(new char[length]);
	m_reader->read(xmlData.get(), length, offset);

	m_kolyXML = xmlParseMemory(xmlData.get(), length);

	xpathContext = xmlXPathNewContext(m_kolyXML);

	// select all partition dictionaries with partition ID >= 0
	xpathObj = xmlXPathEvalExpression((const xmlChar*) "/plist/dict/key[text()='resource-fork']/following-sibling::dict[1]/key[text()='blkx']"
			"/following-sibling::array[1]/dict[key[text()='ID']/following-sibling::string[text() >= 0]]", xpathContext);

	if (xpathObj && xpathObj->nodesetval)
		loadPartitionElements(xpathContext, xpathObj->nodesetval);

	xmlXPathFreeObject(xpathObj);
	xmlXPathFreeContext(xpathContext);
}

void DMGDisk::loadPartitionElements(xmlXPathContextPtr xpathContext, xmlNodeSetPtr nodes)
{
	for (int i = 0; i < nodes->nodeNr; i++)
	{
		xmlXPathObjectPtr xpathObj;
		Partition part;
		BLKXTable* table;

		if (nodes->nodeTab[i]->type != XML_ELEMENT_NODE)
			continue;

		xpathContext->node = nodes->nodeTab[i];

		xpathObj = xmlXPathEvalExpression((const xmlChar*) "string(key[text()='Name']/following-sibling::string)", xpathContext);

		if (!xpathObj || !xpathObj->stringval)
			throw std::runtime_error("Invalid XML data, partition Name key not found");
		
		table = loadBLKXTableForPartition(i);
		
		if (table)
		{
			part.offset = be(table->firstSectorNumber) * 512;
			part.size = be(table->sectorCount) * 512;
		}

		parseNameAndType((const char*) xpathObj->stringval, part.name, part.type);
		m_partitions.push_back(part);

		xmlXPathFreeObject(xpathObj);
		//delete table;
	}
}

bool DMGDisk::parseNameAndType(const std::string& nameAndType, std::string& name, std::string& type)
{
	// Format: "Apple (Apple_partition_map : 1)"
	size_t paren = nameAndType.find('(');
	size_t colon;

	if (paren == std::string::npos)
		return false;

	name = nameAndType.substr(0, paren-1);
	colon = nameAndType.find(':', paren);

	if (colon == std::string::npos)
		return false;

	type = nameAndType.substr(paren+1, (colon - paren) - 2);
	return true;
}

BLKXTable* DMGDisk::loadBLKXTableForPartition(unsigned int index)
{
	xmlXPathContextPtr xpathContext;
	xmlXPathObjectPtr xpathObj;
	char expr[300];
	BLKXTable* rv = nullptr;

	sprintf(expr, "string(/plist/dict/key[text()='resource-fork']/following-sibling::dict[1]/key[text()='blkx']"
		"/following-sibling::array[1]/dict[key[text()='ID']/following-sibling::string[text() = %u]]/key[text()='Data']/following-sibling::data)", index);

	xpathContext = xmlXPathNewContext(m_kolyXML);
	xpathObj = xmlXPathEvalExpression((const xmlChar*) expr, xpathContext);

	if (xpathObj && xpathObj->stringval)
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
	char buffer[512];
	int rd;

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new_mem_buf((void*) input.c_str(), input.length());
	bmem = BIO_push(b64, bmem);

	do
	{
		int rd = BIO_read(bmem, buffer, sizeof(buffer));
		if (rd > 0)
			output.insert(output.end(), buffer, &buffer[rd]);
	}
	while (rd > 0);

	BIO_free_all(bmem);
	return rd >= 0;
}

Reader* DMGDisk::readerForPartition(unsigned int index)
{
	BLKXTable* table = loadBLKXTableForPartition(index);
	if (!table)
		return nullptr;
	
	return new DMGPartition(m_reader, table);
}

