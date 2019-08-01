#include "MemoryManager.h"

int main()
{
	MemoryManager::Initialize(10 * MB, true);

	//2MB allocation test (should insert into first block without issue)
	char** testOne = (char**)MEM_CREATE(MEM_NEW(2 * MB) char*);

	//20MB allocation test (above the block size)
	char** testTwo = (char**)MEM_CREATE(MEM_NEW(20 * MB) char*);

	//test the MEM_USE macro with a class about 1MB in space
	class TestClass { MEM_USE char memory[MB * 1]; };
	TestClass** testThree = (TestClass**)MEM_CREATE(new TestClass());

	
	MEM_DEL(*testOne);
	MEM_DEL(*testTwo);
	delete* testThree;

	MemoryManager::Shutdown();
	return 0;
}