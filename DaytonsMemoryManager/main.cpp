#include "MemoryManager.h"
#include <string>
#include <iostream>
#include <chrono>
#include <thread>

using namespace std;

class TestClass { MEM_USE char memory[MB * 1] = { 0 }; int val = 0; string name; public: void Test() { val++; name = "John Doe"; } string GetName() { return name; }};

int main()
{
	//initialize the memory manager with 10mb data block limit
	MemoryManager::Initialize(10 * MB, true);

	TestClass* testFour;
	MemoryPointer<TestClass> testFive;

	//tests
	{
		//size integrity checks
		cout << "Size of MemoryPointer: " << sizeof(MemoryPointer<void>) << endl;
		cout << "Size of MemoryNodeBlock: " << sizeof(MemoryNodeBlock) << endl;
		cout << "Size of MemoryNode: " << sizeof(MemoryNode) << endl;
		cout << "Size of ObjectDataBlock: " << sizeof(ObjectDataBlock) << endl;

		//2MB allocation test (should insert into first block without issue)
		MemoryPointer<char> testOne = MEM_CREATE<char>(MEM_NEW(2 * MB) char*);

		//20MB allocation test (above the block size so will automatically create a new block)
		MemoryPointer<char> testTwo = MEM_CREATE<char>(MEM_NEW(20 * MB) char*);

		//reassigning 1 smart pointer to another. One went out of scope and should deallocate
		testTwo = testOne;

		//test the MEM_USE macro with a class about 1MB in space. This should allocate back onto the first block
		MemoryPointer<TestClass> testThree = MEM_CREATE<TestClass>(new TestClass());

		testFour = testThree.Get();

		//test the pointer
		testFour->Test();

		//ensure data is correct
		cout << "Test four name: " << testThree->GetName() << endl;

		//last test. We'll initialize a value here but we'll assign it to a different smart pointer out of scope. Once this leaves scope the data shouldnt be deleted.
		MemoryPointer<TestClass> testSix = MEM_CREATE<TestClass>(new TestClass());
		testSix->Test();
		testFive = testSix;

	}//once scope is left all other memory EXCEPT testSix should automatically deallocate


	//UNDEFINED BEHAVIOR. testFour pointer was assigned from a value inside a smart pointer and that smart pointer has been deallocated now so the memory has changed. Depending on deallocation and where the 
	//pointer existed in the data block it could be 0s, it could also be random memory of a different object... don't do this. Use the smart pointers.
	cout << "UNDEFINED BEHAVIOR TEST NAME: " << testFour->GetName() << endl;

	//proper way to do it. testFive is a Smart Pointer and still has a proper reference to data so the data wasnt deleted.
	cout << "PROPER TEST NAME: " << testFive->GetName() << endl;

	//check fragmentation (data thats currently inaccessible by memory iterators because its been deallocated sloppily)
	//If you run this program with auto-defrag enabled this should be 0%. Otherwise it will likely spit something out if you've deallocated data at some point.
	cout << "FRAGMENTATION: " << MemoryManager::GetFragmentationCount() << "%" << endl;

	//display memory count
	cout << "Total memory in block storage: " << MemoryManager::GetTotalMemoryUsed() << " bytes" << endl;

	//shutdown the memory manager which will automatically deallocate all data. If memory was still pointing here there will be undefined behavior so do this at the end of program execution.
	MemoryManager::Shutdown();

	//show memory count after shutdown
	cout << "Total memory in block storage: " << MemoryManager::GetTotalMemoryUsed() << " bytes" << endl;

	//debugger should update with a smaller allocation stamp
	system("PAUSE");
	return 0;
}