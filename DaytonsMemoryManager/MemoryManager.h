#pragma once
#include <malloc.h>
#include <vector>
#include <assert.h>

#define KB 1024
#define MB KB * KB
#define GB MB * MB

//shortcut macro for creating a new object with MemoryManager Allocation
#define MEM_NEW(__SIZE) new (MEM_ALLOCATE(__SIZE))

//shortcut macro for allocating space
#define MEM_ALLOCATE(__SIZE) MemoryManager::AllocateMemory(__SIZE)

//shortcut macro for deleting allocated memory
#define MEM_DEL(__ptr) MemoryManager::DeallocateMemory(__ptr);

//Define this on any classes that will be managed by this memory manager to automatically convert the new/delete operators to use this MemoryManager instead.
#define MEM_USE public: void * operator new(size_t size) { void* p = MEM_ALLOCATE(size); return p; } void operator delete(void * p) { MemoryManager::DeallocateMemory(p); } private: 

//if you want your memory pointers to automatically adapt to deletes and potential movement of memory to avoid fragmenting issues, use this.
#define MEM_CREATE(__PTR) MemoryManager::GetDataReference(__PTR)

struct MemoryNode;
struct ObjectDataBlock;
struct MemoryNodeBlock;

//this block keeps all objects the user cares about in storage here. Each block keeps a vector of each object it has in its data allocation, a data block with an iterator and limit, and pointers to front and back
//this is functionally a doubly-linked list of memory blocks.
struct ObjectDataBlock
{
	ObjectDataBlock(size_t BlockSize, unsigned char* Memory) : m_MemoryBlock(Memory), m_BlockSize(BlockSize), m_MemoryIterator(0), m_Previous(nullptr) {}
	~ObjectDataBlock()
	{
		if (m_Previous != nullptr)
			delete m_Previous;
		if (m_MemoryBlock != nullptr)
			free(m_MemoryBlock);
	}

	unsigned char* m_MemoryBlock = nullptr;
	std::vector<MemoryNode*> m_Objects;
	size_t m_MemoryIterator = 0;
	size_t m_BlockSize = 0;
	ObjectDataBlock * m_Previous = nullptr;
	ObjectDataBlock * m_Next = nullptr;
};

//this block keeps track of ALL allocations by the user. This is a singly linked list of blocks that don't keep external track of nodes. After each node is made it will be left here until program
//shutdown (the user memory leaks 32 bytes per allocation until I'm less lazy and fix this up or create a different allocation method that is less safe but doesn't use node memory management).
struct MemoryNodeBlock
{
	MemoryNodeBlock(unsigned char* Memory) : m_MemoryBlock(Memory) {}
	~MemoryNodeBlock()
	{
		if (m_Previous != nullptr)
			delete m_Previous;
		if (m_MemoryBlock != nullptr)
			free(m_MemoryBlock);
	}

	unsigned char* m_MemoryBlock = nullptr;
	unsigned int m_NodeCounter = 0;
	size_t m_NodeMax = 100;
	MemoryNodeBlock * m_Previous = nullptr;
};

//each allocation on the memory manager will have a node that keeps track of all its data. this effectively increases the allocation of each object by 32 bytes.
struct MemoryNode
{
	unsigned char* m_Object;
	ObjectDataBlock* m_Block;
	size_t m_SizeOfAllocation;
	size_t m_IteratorBeforeAllocation;
};

//Dayton's first Memory Manager.
//--------
//HOW TO USE:
//--------
// - Any class using this header should use the MEM_USE macro after their class declaration to automatically override the new/delete operators to use this manager instead.
// - To create an instance of your class, you'll want to use the MEM_CREATE macro. This will return a pointer-to-pointer of your object.
// - Example: Myclass** class = MEM_CREATE(new MyClass());
// - Warning: If you want to manage memory without using the MEM_USE macro you'll need to manually request allocations and deletes or else you'll leak memory or potentially mess with your object pointer
// during deletions later.
//--------
//EXAMPLE USAGES:
//--------
// - MyClass* class = new MyClass(); //creating an object that IS using the MEM_USE macro. It looks like nothing special happened. Not defrag safe.
// - delete class; //deleting an object that IS using the MEM_USE macro. Again... nothing special here.
// - MyClass** class = MEM_CREATE(new MyClass()); //similar to before but this version is safe for defragmentation.
// - delete *class; //Make sure you de-reference the variable before deleting if you're using defrag-safe ptp.
// - MyClass** class = MEM_CREATE(MEM_NEW(sizeof(MyClass)) MyClass()); //to create a defrag-safe object that isnt using the MEM_USE macro
// - MEM_DEL(*class);// to delete an object that isnt using the MEM_USE macro
//--------
class MemoryManager
{

private:
	static size_t m_BlockSize;
	static unsigned int m_BlockCount;
	static struct ObjectDataBlock* m_CurrentBlock;
	static struct MemoryNodeBlock* m_CurrentNode;
	static bool m_EnableDefragmentation;

	//Internal recursive check to find if there are any blocks that can fit a particular allocation.
	static inline ObjectDataBlock* CheckForAvailableBlockSpace(ObjectDataBlock* block, size_t size, bool CheckPrev = true, bool CheckNext = true)
	{
		if (block == nullptr)
			return nullptr;

		ObjectDataBlock* prev = block->m_Previous;
		ObjectDataBlock* next = block->m_Next;

		//check the previous node if it has space
		if (prev != nullptr && CheckPrev)
		{
			if (prev->m_BlockSize > prev->m_MemoryIterator + size)
				return prev;
		}

		//check the next node if it has space
		if (next != nullptr && CheckNext)
		{
			if (next->m_BlockSize > next->m_MemoryIterator + size)
				return next;
		}

		//recursively check the nodes before previous
		ObjectDataBlock* previousRecursiveCheck = CheckForAvailableBlockSpace(prev, size, true, false);
		if (previousRecursiveCheck != nullptr)
			return previousRecursiveCheck;

		//recursively check the nodes after next
		ObjectDataBlock* nextRecursiveCheck = CheckForAvailableBlockSpace(prev, size, false, true);
		if (previousRecursiveCheck != nullptr)
			return previousRecursiveCheck;

		//no nodes were found that could hold the size request.
		return nullptr;
	}

	//Get the definitive head of our data block.
	static inline ObjectDataBlock* GetHead()
	{
		ObjectDataBlock* head = m_CurrentBlock;
		while (head->m_Next != nullptr)
			head = head->m_Next;

		return head;
	}

public:
	//Initialize the memory manager. Requires a size for the blocks as well as an optional choice on enabling or disabling defragmentation.
	//In general defragmentation isnt needed but if you're constantly deleting and creating new objects then you'll slowly waste space so defragmentation helps to alleviate that at a cost
	//of some CPU power.
	static inline void Initialize(size_t SizeOfBlocks, bool EnableDefrag = true)
	{
		m_EnableDefragmentation = true;
		m_BlockSize = SizeOfBlocks;
		unsigned char* memory = (unsigned char*)malloc(m_BlockSize);
		m_CurrentBlock = new ObjectDataBlock(m_BlockSize, &memory[0]);

		memory = (unsigned char*)malloc(sizeof(MemoryNode) * 100);
		m_CurrentNode = new MemoryNodeBlock(&memory[0]);

		m_BlockCount = 1;
	}

	//Disable the Memory Manager.
	static inline void Shutdown()
	{
		delete GetHead();
		delete m_CurrentNode;
		m_BlockCount = 0;
	}

	//Allocate a chunk of memory for something. Will also create new blocks if an allocation is too big to fit.
	static inline void* AllocateMemory(size_t size)
	{
		MemoryNode* node;

		//get our node block to write our memory to
		if (m_CurrentNode->m_NodeCounter >= 100)
		{
			unsigned char* memory = (unsigned char*)malloc(sizeof(MemoryNode) * 100 + sizeof(MemoryNodeBlock));
			MemoryNodeBlock* newNodeBlock = new(memory) MemoryNodeBlock(&memory[0 + sizeof(MemoryNodeBlock)]);
			newNodeBlock->m_Previous = m_CurrentNode;
			m_CurrentNode = newNodeBlock;
		}

		//create our node
		node = new(&m_CurrentNode->m_MemoryBlock[m_CurrentNode->m_NodeCounter * sizeof(MemoryNodeBlock)]) MemoryNode();
		m_CurrentNode->m_NodeCounter++;

		//the location on a data block we'll be alloting memory to
		size_t memToWriteTo;

		//can the current block not support this allocation? find one that can
		if (m_CurrentBlock->m_BlockSize < m_CurrentBlock->m_MemoryIterator + size)
		{
			//check the heads and tails if theres a block that can fit this allocation
			ObjectDataBlock* blockTest = CheckForAvailableBlockSpace(m_CurrentBlock, size);

			//if there isnt, create a new block that can fit the size or use default size
			if (blockTest == nullptr)
			{
				if (m_CurrentBlock->m_Next != nullptr)
					m_CurrentBlock = GetHead();

				size_t blockSize = m_BlockSize > size ? m_BlockSize : size;

				unsigned char* memory = (unsigned char*)malloc(blockSize + sizeof(ObjectDataBlock));
				ObjectDataBlock * newBlock = new ObjectDataBlock(blockSize, &memory[0]);
				
				newBlock->m_Previous = m_CurrentBlock;
				m_CurrentBlock->m_Next = newBlock;
				m_CurrentBlock = newBlock;
			}
			//there was a block that could fit it so use that.
			else
				m_CurrentBlock = blockTest;
		}

		//set our memory address to write to and move the iterator forward an equal amount
		memToWriteTo = m_CurrentBlock->m_MemoryIterator;
		m_CurrentBlock->m_MemoryIterator += size;

		//the memory address alloted
		unsigned char* p = &m_CurrentBlock->m_MemoryBlock[memToWriteTo];

		//fill node data
		node->m_Object = p;
		node->m_IteratorBeforeAllocation = memToWriteTo;
		node->m_SizeOfAllocation = size;
		node->m_Block = m_CurrentBlock;

		//add node to the data block
		m_CurrentBlock->m_Objects.push_back(node);

		return (void*)node->m_Object;
	}

	//Release memory back into the block. If defragmentation is enabled it will also adjust the memory to push all free space to the end of the block.
	static inline void DeallocateMemory(void* p)
	{
		//check which block this memory belongs to
		for (auto a = GetHead(); a != nullptr; a = a->m_Previous)
		{
			for (auto it = a->m_Objects.begin(); it != a->m_Objects.end(); ++it)
			{
				//found the node that this memory belongs to
				if (p == (*it)->m_Object)
				{
					//this is the node we're removing from the object list
					auto itToRemove = it;

					//if defragmentation is enabled then we need to adjust everything ahead of the deleted object to push them all back
					if (m_EnableDefragmentation)
					{
						//figure out how many bytes we're retrieving
						size_t allocBefore = (*it)->m_IteratorBeforeAllocation;
						size_t allocAfter = allocBefore + (*it)->m_SizeOfAllocation;
						size_t allocCurrent = (*it)->m_Block->m_MemoryIterator;

						//move all the data in front of the allocattion back equal to the amount of bytes we're retrieving
						memmove(&(*it)->m_Block->m_MemoryBlock[allocBefore], &(*it)->m_Block->m_MemoryBlock[allocAfter], allocCurrent - allocAfter);
						(*it)->m_Block->m_MemoryIterator -= (*it)->m_SizeOfAllocation;

						//move the iterator forward and adjust all other memory nodes accordingly
						it++;

						//have every node move their memory reference and allocater back an equal amount of bytes to the allocation so they're still pointing in the right place
						for (it; it != a->m_Objects.end(); ++it)
							if ((*it)->m_IteratorBeforeAllocation > allocBefore)
							{
								(*it)->m_IteratorBeforeAllocation -= (*itToRemove)->m_SizeOfAllocation;
								((*it)->m_Object) = &a->m_MemoryBlock[(*it)->m_IteratorBeforeAllocation];
							}
					}
		
					//null out the old object and remove it from the managed objects on the block
					(*itToRemove)->m_Object = nullptr;
					a->m_Objects.erase(itToRemove);
					return;
				}
			}
		}


		assert(false);
		return;
	}

	//Returns a reference to the internal pointer used by the memory manager.
	//If defragmentation is enabled. USE THIS OR RISK LOSIG MEMORY!
	static inline unsigned char** GetDataReference(void* p)
	{
		for (ObjectDataBlock* a = m_CurrentBlock; a != nullptr; a = m_CurrentBlock->m_Previous)
		{
			for (unsigned int i = 0; i < a->m_Objects.size(); i++)
				if (a->m_Objects[i]->m_Object == p)
					return &a->m_Objects[i]->m_Object;
		}


		assert(false);
		return 0;
	}
};

__declspec(selectany) unsigned int MemoryManager::m_BlockCount;

__declspec(selectany) ObjectDataBlock* MemoryManager::m_CurrentBlock;

__declspec(selectany) struct MemoryNodeBlock* MemoryManager::m_CurrentNode;

__declspec(selectany) bool MemoryManager::m_EnableDefragmentation;

__declspec(selectany) size_t MemoryManager::m_BlockSize;
