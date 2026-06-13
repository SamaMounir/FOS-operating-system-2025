#include <inc/lib.h>

#define Uheap_Pages_count ((USER_HEAP_MAX - USER_HEAP_START) / PAGE_SIZE)
#define va_to_index(va) (((uint32)(va) - USER_HEAP_START) / PAGE_SIZE)
static uint32 userAllocSizes[Uheap_Pages_count];
static uint32 pages[Uheap_Pages_count];

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//==============================================
// [1] INITIALIZE USER HEAP:
//==============================================
int __firstTimeFlag = 1;
void uheap_init()
{
	if(__firstTimeFlag)
	{
		initialize_dynamic_allocator(USER_HEAP_START, USER_HEAP_START + DYN_ALLOC_MAX_SIZE);
		uheapPlaceStrategy = sys_get_uheap_strategy();
		uheapPageAllocStart = dynAllocEnd + PAGE_SIZE;
		uheapPageAllocBreak = uheapPageAllocStart;

		__firstTimeFlag = 0;
	}
}

static uint32 user_customfit(uint32 numPages) {
    uint32 first_exact_va = 0;
    uint32 worst_va = 0;
    uint32 worst_size = 0;

    uint32 curr_va = uheapPageAllocStart;
    while (curr_va < uheapPageAllocBreak) {
        uint32 index = va_to_index(curr_va);

        if (userAllocSizes[index] != 0) {
            // Occupied block, skip it
            uint32 usedSpace_size = userAllocSizes[index];
            uint32 usedSpace_pages = ROUNDUP(usedSpace_size,PAGE_SIZE) / PAGE_SIZE;
            curr_va += usedSpace_pages * PAGE_SIZE;
            continue;
        }

        // Free Space, count size
        uint32 freeSpace_va = curr_va;
        uint32 freeSpace_size = 0;
        uint32 temp_va = curr_va;
        uint32 temp_index = index;
        while (temp_va < uheapPageAllocBreak && userAllocSizes[temp_index] == 0) {
            freeSpace_size++;
            temp_va += PAGE_SIZE;
            temp_index++;
        }

        // Check for exact fit
        if (freeSpace_size == numPages) {
            if (first_exact_va == 0) {
                first_exact_va = freeSpace_va;
            }
        }

        // Update worst fit if better
        if (freeSpace_size >= numPages && freeSpace_size > worst_size) {
            worst_size = freeSpace_size;
            worst_va = freeSpace_va;
        }

        // Move to next
        curr_va = temp_va;
    }
    // Prefer exact if found
	if (first_exact_va != 0) {
		return first_exact_va;
    } else if (worst_size >= numPages) {
    	return worst_va;
    }
	return 0;
}


//==============================================
// [2] GET A PAGE FROM THE KERNEL FOR DA:
//==============================================
int get_page(void* va)
{
	int ret = __sys_allocate_page(ROUNDDOWN(va, PAGE_SIZE), PERM_USER|PERM_WRITEABLE|PERM_UHPAGE);
	if (ret < 0)
		panic("get_page() in user: failed to allocate page from the kernel");
	return 0;
}

//==============================================
// [3] RETURN A PAGE FROM THE DA TO KERNEL:
//==============================================
void return_page(void* va)
{
	int ret = __sys_unmap_frame(ROUNDDOWN((uint32)va, PAGE_SIZE));
	if (ret < 0)
		panic("return_page() in user: failed to return a page to the kernel");
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=================================
// [1] ALLOCATE SPACE IN USER HEAP:
//=================================
void* malloc(uint32 size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	if (size == 0) return NULL ;
	//==============================================================
	/*Jana [PROJECT'25.IM#2] USER HEAP*/
	//TODO: [PROJECT'25.IM#2] USER HEAP - #1 malloc
	#if USE_KHEAP

	uheap_init();
	uint32 numPages = ROUNDUP(size,PAGE_SIZE) / PAGE_SIZE;
	uint32 neededBytes = numPages * PAGE_SIZE;

	//check valid size or not
	if (size == 0 || (neededBytes > USER_HEAP_MAX - uheapPageAllocBreak))
		return NULL;

	//check block alloc or not
	if (size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
		return alloc_block(size);
	}

	//2) Page allocator for larger sizes
	// 2.1) Try custom fit (search EXACT -> WORST) mn el kmalloc 3ady
	uint32 allocVA = user_customfit(numPages);

	if (allocVA == 0) {
		// Ensure no overflow (already checked above)
		if (uheapPageAllocBreak + neededBytes > USER_HEAP_MAX) {
			return NULL; // Out of heap space
		}
		allocVA = uheapPageAllocBreak;
		uheapPageAllocBreak += neededBytes;
	}

	//syscall for kernel to mark the va with neededbytes for real alloc if fault happened

	uint32 pageidx = va_to_index(allocVA); //converting the va(ely tl3to fady mn custom fit) to index
	userAllocSizes[pageidx] = size; //marks requested sizes in bytes

	for (uint32 temp_page_va = allocVA; temp_page_va < allocVA + neededBytes;temp_page_va += PAGE_SIZE) {
		uint32 temp_idx = va_to_index(temp_page_va);
		pages[temp_idx] = 1;
	}

	sys_allocate_user_mem(allocVA, neededBytes);
	return (void*) allocVA;

	#else
		panic("KERNEL HEAP is OFF! malloc() needs USE_KHEAP to be 1");
	#endif
	//Your code is here
	//Comment the following line
	//panic("malloc() is not implemented yet...!!");
}

//=================================
// [2] FREE SPACE FROM USER HEAP:
//=================================
void free(void* virtual_address)
{	/*Jana [PROJECT'25.IM#2] USER HEAP*/
	//TODO: [PROJECT'25.IM#2] USER HEAP - #3 free
	//Your code is here
	#if USE_KHEAP

	uint32 mycurrentva = (uint32) virtual_address;
	uint32 myindex = va_to_index(mycurrentva);//change va to index to get its size
	uint32 mysize = userAllocSizes[myindex];
	uint32 neededBytes = ROUNDUP(mysize, PAGE_SIZE);

	//uint32 idx_brk = va_to_index(uheapPageAllocBreak);

	//check for null
	if (mycurrentva == 0)
		return;

	//check va inside user heap or not
	if (mycurrentva < USER_HEAP_START
		|| mycurrentva >= USER_HEAP_MAX
		|| mycurrentva >= uheapPageAllocBreak
		|| mycurrentva + mysize > USER_HEAP_MAX
		|| mycurrentva + mysize > uheapPageAllocBreak) {
		panic("* free: invalid VA outside User heap\n");
		return;
	}

	//double free
	if (mysize == 0) {
		return ;
	}

	// 1) Small allocations -> Block Allocator
	if (mysize <= DYN_ALLOC_MAX_BLOCK_SIZE) {
		free_block(virtual_address);
		return;
	}
	sys_free_user_mem(mycurrentva, neededBytes);
	userAllocSizes[myindex] = 0;
	for(uint32 temp_page_va = mycurrentva ; temp_page_va<mycurrentva+neededBytes ; temp_page_va+=PAGE_SIZE){
		uint32 temp_idx = va_to_index(temp_page_va);
		pages[temp_idx] = 0;
	}
	while (uheapPageAllocBreak > uheapPageAllocStart) {
		uint32 last_va  = uheapPageAllocBreak - PAGE_SIZE;
		uint32 idx_brk = va_to_index(last_va);

		if(pages[idx_brk] == 0){
			uheapPageAllocBreak -= PAGE_SIZE;
		}
		else{
			break;
		}
	}

	#else
		panic("KERNEL HEAP is OFF! free() needs USE_KHEAP to be 1");
	#endif
	//Comment the following line
	//panic("free() is not implemented yet...!!");
}

//=================================
// [3] ALLOCATE SHARED VARIABLE:
//=================================


void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	//==============================================================
	uheap_init();
	/*Sohila [PROJECT'25.IM#3] SHARED MEMORY*/
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #2 smalloc
	//Your code is here
	#if USE_KHEAP

	uint32 numPages = ROUNDUP(size,PAGE_SIZE) / PAGE_SIZE;
	uint32 neededBytes = numPages * PAGE_SIZE;

	//check valid size or not
	if (size == 0 || (neededBytes > USER_HEAP_MAX - uheapPageAllocBreak))
		return NULL;
	//2) Page allocator for larger sizes
	// 2.1) Try custom fit (search EXACT -> WORST) mn el kmalloc 3ady
	uint32 allocVA = user_customfit(numPages);

	if (allocVA == 0) {
		// Ensure no overflow (already checked above)
	    if (uheapPageAllocBreak + neededBytes > USER_HEAP_MAX) {
	    	return NULL; // Out of heap space
	    }
	    allocVA = uheapPageAllocBreak;
	    uheapPageAllocBreak += neededBytes;
	}

	//syscall for kernel to mark the va with neededbytes for real alloc if fault happened

	uint32 pageidx = va_to_index(allocVA); //converting the va(ely tl3to fady mn custom fit) to index
	userAllocSizes[pageidx] = size; //marks requested sizes in bytes

	for (uint32 temp_page_va = allocVA; temp_page_va < allocVA + neededBytes;
		temp_page_va += PAGE_SIZE) {
	    uint32 temp_idx = va_to_index(temp_page_va);
	    pages[temp_idx] = 1;
	}

	int res = sys_create_shared_object(sharedVarName, size, isWritable, (void*) allocVA);
	if (res < 0) {
		return NULL;
	}
	return (void*) allocVA;

	#else
		panic("KERNEL HEAP is OFF! smalloc() needs USE_KHEAP to be 1");
	#endif
	//Comment the following line
	//panic("smalloc() is not implemented yet...!!");
}

//========================================
// [4] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================
	/*Sohila [PROJECT'25.IM#3] SHARED MEMORY*/
	//TODO: [PROJECT'25.IM#3] SHARED MEMORY - #4 sget
	//Your code is here
	#if USE_KHEAP

	int shr_size = sys_size_of_shared_object(ownerEnvID, sharedVarName);
	if (shr_size < 0) {
		return NULL;
	}
	uint32 roundedSize = ROUNDUP(shr_size, PAGE_SIZE);
	uint32 numofpages = roundedSize / PAGE_SIZE;
	uint32 ALLOCva = user_customfit(numofpages);
	if (ALLOCva == 0) {
		if (uheapPageAllocBreak + (numofpages * PAGE_SIZE) >= USER_HEAP_MAX) {
			return NULL; // Out of heap space
		}
		ALLOCva = uheapPageAllocBreak;
	    uheapPageAllocBreak += (numofpages * PAGE_SIZE);
	}
	uint32 pageidx = va_to_index(ALLOCva); //converting the va(ely tl3to fady mn custom fit) to index
	userAllocSizes[pageidx] = shr_size; //marks requested sizes in bytes

	for (uint32 temp_page_va = ALLOCva; temp_page_va < ALLOCva + (numofpages * PAGE_SIZE); temp_page_va +=PAGE_SIZE) {
		uint32 temp_idx = va_to_index(temp_page_va);
		pages[temp_idx] = 1;
	}

	uint32 retofget = sys_get_shared_object(ownerEnvID, sharedVarName, (void*) ALLOCva);
	if (retofget < 0) {
		return NULL;
	}

	return (void*) ALLOCva;

	#else
		panic("KERNEL HEAP is OFF! sget() needs USE_KHEAP to be 1");
	#endif
	//Comment the following line
	//panic("sget() is not implemented yet...!!");
}


//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	uheap_init();
	//==============================================================
	panic("realloc() is not implemented yet...!!");
}


//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_delete_shared_object(...); which switches to the kernel mode,
//	calls delete_shared_object(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the delete_shared_object() function is empty, make sure to implement it.
void sfree(void* virtual_address)
{
	//TODO: [PROJECT'25.BONUS#5] EXIT #2 - sfree
	//Your code is here
	//Comment the following line
	panic("sfree() is not implemented yet...!!");

	//	1) you should find the ID of the shared variable at the given address
	//	2) you need to call sys_freeSharedObject()
}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//
