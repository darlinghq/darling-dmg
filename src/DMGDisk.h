#ifndef DMGDISK_H
#define DMGDISK_H
#include "PartitionedDisk.h"
#include "Reader.h"
#include "dmg.h"
#include "CacheZone.h"
#include <libxml/parser.h>
#include <libxml/xpath.h>

class DMGDisk : public PartitionedDisk
{
public:
	DMGDisk(std::shared_ptr<Reader>reader);
	~DMGDisk();

	virtual const std::vector<Partition>& partitions() const override { return m_partitions; }
	virtual std::shared_ptr<Reader> readerForPartition(int index) override;

	static bool isDMG(std::shared_ptr<Reader> reader);
private:
	void loadKoly(const UDIFResourceFile& koly);
	bool loadPartitionElements(xmlXPathContextPtr xpathContext, xmlNodeSetPtr nodes);
	static bool parseNameAndType(const std::string& nameAndType, std::string& name, std::string& type);
	static bool base64Decode(const std::string& input, std::vector<uint8_t>& output);
	BLKXTable* loadBLKXTableForPartition(int index);
	std::shared_ptr<Reader> readerForKolyBlock(int index);
private:
	std::shared_ptr<Reader> m_reader;
	std::vector<Partition> m_partitions;
	UDIFResourceFile m_udif;
	xmlDocPtr m_kolyXML;
	std::shared_ptr<CacheZone> m_zone;
};

#endif

