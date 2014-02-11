#ifndef RESOURCE_FORK_H
#define RESOURCE_FORK_H
#include "Reader.h"
#include "rsrc.h"
#include <map>
#include <memory>
#include <stdint.h>

class ResourceFork
{
public:
	ResourceFork(std::shared_ptr<Reader> reader);
	
	std::shared_ptr<Reader> getResource(uint32_t resourceType, uint16_t id);
private:
	void loadResources();
private:
	std::shared_ptr<Reader> m_reader;
	
	struct Resource
	{
		uint32_t type;
		uint16_t id;
	};
	struct ResourceLocation
	{
		uint64_t offset;
		uint32_t length;
	};
	friend bool operator<(const ResourceFork::Resource& t, const ResourceFork::Resource& that);
	
	std::map<Resource, ResourceLocation> m_resources;
};

#endif

