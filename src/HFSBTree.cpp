#include "HFSBTree.h"
#include <stdexcept>
#include "be.h"
#include "hfsplus.h"
#include "unichar.h"
#include <iostream>
#include <cstring>
#include <set>
#include <algorithm>
#include "HFSBTreeNode.h"
#include "CacheZone.h"

HFSBTree::HFSBTree(std::shared_ptr<HFSFork> fork, CacheZone* zone, const char* cacheTag)
: m_fork(fork)
{
	BTNodeDescriptor desc0;
	
	//std::cout << "Tree size: " << fork->length() << std::endl;
	
	m_reader.reset(new CachedReader(m_fork, zone, cacheTag));
	
	if (m_reader->read(&desc0, sizeof(desc0), 0) != sizeof(desc0))
		throw std::runtime_error("Failed to read BTNodeDescriptor zero");
	
	if (desc0.kind != NodeKind::kBTHeaderNode)
		throw std::runtime_error("Wrong kind of BTree header");
	
	if (m_reader->read(&m_header, sizeof(m_header), sizeof(desc0)) != sizeof(m_header))
		throw std::runtime_error("Failed to read BTHeaderRec");
	
	//std::cout << "leaf records: " << be(m_header.leafRecords) << std::endl;
	//std::cout << "node size: " << be(m_header.nodeSize) << std::endl;
	//std::cout << "first leaf node: " << be(m_header.firstLeafNode) << std::endl;
	//std::cout << "last leaf node: " << be(m_header.lastLeafNode) << std::endl;
	
	/*if (m_header.rootNode)
	{
		walkTree(be(m_header.rootNode));
	}*/
}

HFSBTreeNode HFSBTree::findLeafNode(const Key* indexKey, KeyComparator comp, bool wildcard)
{
	return traverseTree(be(m_header.rootNode), indexKey, comp, wildcard);
}

std::vector<HFSBTreeNode> HFSBTree::findLeafNodes(const Key* indexKey, KeyComparator comp)
{
	std::vector<HFSBTreeNode> rv;
	std::set<uint32_t> uniqLink; // for broken filesystems
	HFSBTreeNode current = findLeafNode(indexKey, comp, true);

	if (current.isInvalid())
		return rv;
	
	rv.push_back(current);

	while (current.forwardLink() != 0)
	{
		Key* key;
		
		if (uniqLink.find(current.forwardLink()) != uniqLink.end())
		{
			std::cerr << "WARNING: forward link loop detected!\n";
			break;
		}
		else
			uniqLink.insert(current.forwardLink());

		//std::cout << "Testing node " << current.forwardLink() << std::endl;
		current = HFSBTreeNode(m_reader, current.forwardLink(), current.nodeSize());
		
		key = current.getKey<Key>(); // TODO: or the key of the first record?

		if (comp(key, indexKey) > 0)
			break;

		rv.push_back(current);
	}

	return rv;
}

HFSBTreeNode HFSBTree::traverseTree(int nodeIndex, const Key* indexKey, KeyComparator comp, bool wildcard)
{
	//std::cout << "Examining node " << nodeIndex << std::endl;
	HFSBTreeNode node(m_reader, nodeIndex, be(m_header.nodeSize));

	switch (node.kind())
	{
		case NodeKind::kBTIndexNode:
		{
			int position;
			uint32_t* childIndex;

			if (wildcard)
			{
				auto it = std::lower_bound(node.begin<Key>(), node.end<Key>(), indexKey, [=](const Key* keyA, const Key* keyB) {
					return comp(keyA, keyB) < 0;
				});

				position = it.index() - 1;
			}
			else
			{
				auto it = std::upper_bound(node.begin<Key>(), node.end<Key>(), indexKey, [=](const Key* keyA, const Key* keyB) {
					return comp(keyA, keyB) < 0;
				});

				position = it.index() - 1;
			}
			
			if (position < 0)
				position = 0;
			
			// recurse down
			childIndex = node.getRecordData<uint32_t>(position);
			
			return traverseTree(be(*childIndex), indexKey, comp, wildcard);
		}
		case NodeKind::kBTLeafNode:
		{
			return node;
		}
		case NodeKind::kBTHeaderNode:
		case NodeKind::kBTMapNode:
			break;
		default:
			std::cerr << "Invalid node kind! Kind: " << int(node.kind()) << std::endl;
			
	}

	return HFSBTreeNode();
}

/*
void HFSBTree::walkTree(int nodeIndex)
{
	BTNodeDescriptor* desc;
	uint32_t offset = be(m_header.nodeSize)*nodeIndex;
	uint16_t* firstRecordOffset;
	
	desc = reinterpret_cast<BTNodeDescriptor*>(m_tree + be(m_header.nodeSize)*nodeIndex);
	firstRecordOffset = reinterpret_cast<uint16_t*>(m_tree + be(m_header.nodeSize)*(nodeIndex+1) - sizeof(uint16_t));
	
	switch (desc->kind)
	{
		case NodeKind::kBTLeafNode:
		{
			HFSPlusCatalogKey* key = reinterpret_cast<HFSPlusCatalogKey*>(((char*) desc) + sizeof(BTNodeDescriptor));
			std::cout << "LeafNode " << nodeIndex << " is a leaf node: " << UnicharToString(key->nodeName) << std::endl;
			std::cout << "LeafSibling: " << be(desc->fLink) << std::endl;
			std::cout << "LeafRecords: " << be(desc->numRecords) << std::endl;
			
			for (long i = 0; i < be(desc->numRecords); i++)
			{
				uint16_t recordOffset = be(*(firstRecordOffset-i));
				HFSPlusCatalogKey* recordKey = reinterpret_cast<HFSPlusCatalogKey*>(((char*) desc) + recordOffset);
				HFSPlusCatalogFile* record;
				RecordType recType;
				
				std::cout << "LeafRecordKey: " << UnicharToString(recordKey->nodeName) << " - parent: " << be(recordKey->parentID) << std::endl;
				record = reinterpret_cast<HFSPlusCatalogFile*>(((char*) recordKey) + be(recordKey->keyLength) + sizeof(recordKey->keyLength));
				recType = RecordType(be(uint16_t(record->recordType)));
				
				switch (recType)
				{
					case RecordType::kHFSPlusFolderRecord:
					{
						HFSPlusCatalogFolder* file = (HFSPlusCatalogFolder*) record;
						std::cout << "\tFolder: ID " << be(file->folderID) << std::endl;
						break;
					}
					case RecordType::kHFSPlusFileRecord:
					{
						HFSPlusCatalogFile* file = (HFSPlusCatalogFile*) record;
						std::cout << "\tFile: ID " << be(file->fileID) << std::endl;
						break;
					}
					case RecordType::kHFSPlusFolderThreadRecord:
					{
						HFSPlusCatalogThread* thread = (HFSPlusCatalogThread*) record;
						std::cout << "\tA folder named " << UnicharToString(thread->nodeName) << " with CNID " << be(recordKey->parentID) << " has parent CNID " << be(thread->parentID) << std::endl;
						break;
					}
					case RecordType::kHFSPlusFileThreadRecord:
					{
						HFSPlusCatalogThread* thread = (HFSPlusCatalogThread*) record;
						std::cout << "\tA file named " << UnicharToString(thread->nodeName) << " with CNID " << be(recordKey->parentID) << " has parent CNID " << be(thread->parentID) << std::endl;
						break;
					}
					default:
					{
						std::cout << "\tunknown record type: " << be(uint16_t(record->recordType)) << std::endl;
					}
				}
			}
			break;
		}
		case NodeKind::kBTIndexNode:
		{
			std::cout << "Node " << nodeIndex << " is an index node with " << be(desc->numRecords) << " records\n";
			//std::cout << "Sibling: " << be(desc->fLink) << std::endl;
			
			for (long i = 0; i < be(desc->numRecords); i++)
			{
				uint16_t recordOffset = be(*(firstRecordOffset-i));
				HFSPlusCatalogKey* record = reinterpret_cast<HFSPlusCatalogKey*>(((char*) desc) + recordOffset);
				uint16_t keyLen = be(record->keyLength); // TODO:  kBTVariableIndexKeysMask
				uint32_t childNodeIndex;
				
				std::cout << "Record " << i << ", key len:" << keyLen << std::endl;
				std::cout << "Index key " << be(record->parentID) << std::endl;
				
				childNodeIndex = be(*(uint32_t*) (((char*)record)+2+keyLen) );
				std::cout << "Child node index: " << childNodeIndex << std::endl;
				walkTree(childNodeIndex);
			}
			break;
		}
		case NodeKind::kBTHeaderNode:
			std::cout << "Node " << nodeIndex << " is a header node\n";
			break;
		case NodeKind::kBTMapNode:
			std::cout << "Node " << nodeIndex << " is a map node\n";
			break;
	}
}*/


