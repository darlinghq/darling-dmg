#ifndef HFSBTREENODE_H
#define HFSBTREENODE_H
#include "hfsplus.h"
#include "be.h"
#include <iostream>
#include <vector>
#include <memory>
#include <stdexcept>
#include "Reader.h"

class HFSBTreeNode
{
public:
	HFSBTreeNode()
	: m_descriptor(nullptr), m_nodeSize(0)
	{
	}
	
	HFSBTreeNode(const std::vector<uint8_t>& descriptorData, uint16_t nodeSize)
	: m_descriptorData(descriptorData), m_nodeSize(nodeSize)
	{
		initFromBuffer();
	}
	
	HFSBTreeNode(std::shared_ptr<Reader> treeReader, uint32_t nodeIndex, uint16_t nodeSize)
	: m_nodeSize(nodeSize)
	{
		m_descriptorData.resize(nodeSize);
		
		if (treeReader->read(&m_descriptorData[0], nodeSize, nodeSize*nodeIndex) < nodeSize)
			throw std::runtime_error("Short read of BTree node");
		
		initFromBuffer();
	}
	
	HFSBTreeNode(const HFSBTreeNode& that)
	{
		*this = that;
	}
	
	HFSBTreeNode& operator=(const HFSBTreeNode& that)
	{
		m_descriptorData = that.m_descriptorData;
		m_nodeSize = that.m_nodeSize;
		initFromBuffer();
		return *this;
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

	template <typename KeyType> class RecordIterator : public std::iterator<std::random_access_iterator_tag, KeyType>
	{
	public:
		typedef typename std::iterator<std::random_access_iterator_tag, KeyType>::difference_type difference_type;

		RecordIterator(const HFSBTreeNode* node, int index) : m_node(node), m_index(index)
		{
		}
		KeyType* operator*()
		{
			return m_node->getRecordKey<KeyType>(m_index);
		}
		RecordIterator& operator++()
		{
			m_index++;
			return *this;
		}

		difference_type operator-(const RecordIterator& that)
		{
			return m_index - that.m_index;
		}

		RecordIterator& operator+=(const difference_type& n)
		{
			m_index += n;
			return *this;
		}

		RecordIterator& operator-=(const difference_type& n)
		{
			m_index -= n;
			return *this;
		}

		bool operator!=(const RecordIterator& that)
		{
			return m_index != that.m_index;
		}
		bool operator==(const RecordIterator& that)
		{
			return m_index == that.m_index;
		}
		int index() const
		{
			return m_index;
		}
	private:
		const HFSBTreeNode* m_node;
		int m_index;
	};

	template <typename KeyType> RecordIterator<KeyType> begin() const
	{
		return RecordIterator<KeyType>(this, 0);
	}

	template <typename KeyType> RecordIterator<KeyType> end() const
	{
		return RecordIterator<KeyType>(this, recordCount());
	}
	
private:
	char* descPtr() const
	{
		return reinterpret_cast<char*>(m_descriptor);
	}
	void initFromBuffer()
	{
		m_descriptor = reinterpret_cast<BTNodeDescriptor*>(&m_descriptorData[0]);
		m_firstRecordOffset = reinterpret_cast<uint16_t*>(descPtr() + m_nodeSize - sizeof(uint16_t));
	}
private:
	mutable BTNodeDescriptor* m_descriptor;
	std::vector<uint8_t> m_descriptorData;
	uint16_t m_nodeSize;
	uint16_t* m_firstRecordOffset;
};

#endif
