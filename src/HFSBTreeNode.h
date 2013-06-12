#ifndef HFSBTREENODE_H
#define HFSBTREENODE_H
#include "hfsplus.h"
#include "be.h"
#include <iostream>

class HFSBTreeNode
{
public:
	HFSBTreeNode()
	: m_descriptor(nullptr), m_nodeSize(0)
	{
	}
	
	HFSBTreeNode(BTNodeDescriptor* descriptor, uint16_t nodeSize)
	: m_descriptor(descriptor), m_nodeSize(nodeSize)
	{
		m_firstRecordOffset = reinterpret_cast<uint16_t*>(descPtr() + m_nodeSize - sizeof(uint16_t));
	}
	
	HFSBTreeNode(void* tree, uint32_t nodeIndex, uint16_t nodeSize)
	: m_nodeSize(nodeSize)
	{
		m_descriptor = reinterpret_cast<BTNodeDescriptor*>(((char*)tree) + nodeSize*nodeIndex);
		//std::cout << "Node descriptor for node " << nodeIndex << " is at " << nodeDescriptor << ", size " << nodeSize << std::endl;
		m_firstRecordOffset = reinterpret_cast<uint16_t*>(descPtr() + m_nodeSize - sizeof(uint16_t));
	}
	
	NodeKind kind() const
	{
		return m_descriptor->kind;
	}
	
	uint16_t recordCount() const
	{
		return be(m_descriptor->numRecords);
	}
	
	BTNodeDescriptor* descriptor() const
	{
		return m_descriptor;
	}
	
	uint16_t nodeSize() const
	{
		return m_nodeSize;
	}
	
	bool isInvalid() const
	{
		return !m_descriptor;
	}
	
	uint32_t forwardLink() const
	{
		return be(m_descriptor->fLink);
	}
	
	template<typename KeyType> KeyType* getKey() const
	{
		return reinterpret_cast<KeyType*>(descPtr() + sizeof(BTNodeDescriptor));
	}
	
	template<typename KeyType> KeyType* getRecordKey(uint16_t recordIndex) const
	{
		uint16_t recordOffset = be(*(m_firstRecordOffset - recordIndex));
		
		return reinterpret_cast<KeyType*>(descPtr() + recordOffset);
	}
	
	template<typename DataType> DataType* getRecordData(uint16_t recordIndex) const
	{
		uint16_t* keyLength = getRecordKey<uint16_t>(recordIndex);
		char* keyPtr = reinterpret_cast<char*>(keyLength);
		
		return reinterpret_cast<DataType*>(keyPtr + be(*keyLength) + sizeof(uint16_t));
	}
	
private:
	char* descPtr() const
	{
		return reinterpret_cast<char*>(m_descriptor);
	}
private:
	mutable BTNodeDescriptor* m_descriptor;
	uint16_t m_nodeSize;
	uint16_t* m_firstRecordOffset;
};

#endif
