#ifndef HFSFORK_H
#define HFSFORK_H
#include "hfsplus.h"
#include "HFSVolume.h"
#include <vector>

class HFSVolume;

class HFSFork : public Reader
{
public:
	HFSFork(HFSVolume* vol, const HFSPlusForkData& fork, HFSCatalogNodeID cnid = kHFSNullID, bool resourceFork = false);
	int32_t read(void* buf, int32_t count, uint64_t offset) override;
	uint64_t size() const;
private:
	HFSVolume* m_volume;
	const HFSPlusForkData& m_fork;
	std::vector<const HFSPlusExtentDescriptor*> m_extents;

	HFSCatalogNodeID m_cnid;
	bool m_resourceFork;
};

#endif
