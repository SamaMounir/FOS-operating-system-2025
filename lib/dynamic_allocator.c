/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
//==================================
//==================================
// [1] GET PAGE VA:
//==================================
__inline__ uint32 to_page_va(struct PageInfoElement *ptrPageInfo) {
	if (ptrPageInfo < &pageBlockInfoArr[0]
			|| ptrPageInfo >= &pageBlockInfoArr[DYN_ALLOC_MAX_SIZE / PAGE_SIZE])
		panic("to_page_va called with invalid pageInfoPtr");
	//Get start VA of the page from the corresponding Page Info pointer
	int idxInPageInfoArr = (ptrPageInfo - pageBlockInfoArr);
	return dynAllocStart + (idxInPageInfoArr << PGSHIFT);
}

//==================================
// [2] GET PAGE INFO OF PAGE VA:
//==================================
__inline__ struct PageInfoElement * to_page_info(uint32 va) {
	int idxInPageInfoArr = (va - dynAllocStart) >> PGSHIFT;
	if (idxInPageInfoArr < 0|| idxInPageInfoArr >= DYN_ALLOC_MAX_SIZE/PAGE_SIZE)
		panic("to_page_info called with invalid pa");
	return &pageBlockInfoArr[idxInPageInfoArr];
}

uint32 Othman_next_pow2(uint32 n) {
	int result = 0;
	uint32 value = 1;
	while (value < n) {
		value <<= 1;
		result++;
	}
	return value;
}

uint32 indexes(uint32 n) {
	int result = 0;
	uint32 value = DYN_ALLOC_MIN_BLOCK_SIZE;
	while (value < n) {
		value <<= 1;
		result++;
	}
	return result;
}
//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
bool is_initialized = 0;
void initialize_dynamic_allocator(uint32 daStart, uint32 daEnd) {
	/*Othman & Enas [PROJECT'25]*/
	{
		assert(daEnd <= daStart + DYN_ALLOC_MAX_SIZE);
		is_initialized = 1;
	}
	dynAllocStart = daStart;
	dynAllocEnd = daEnd;
	LIST_INIT(&freePagesList);

	for (int i = 0; i < (LOG2_MAX_SIZE - LOG2_MIN_SIZE + 1); i++) {
		LIST_INIT(&freeBlockLists[i]);
	}
	for (int i = 0; i < (dynAllocEnd - dynAllocStart) / PAGE_SIZE; i++) {
		pageBlockInfoArr[i].block_size = 0;
		pageBlockInfoArr[i].num_of_free_blocks = 0;
		LIST_INSERT_TAIL(&freePagesList, &pageBlockInfoArr[i]);
	}
}

//===========================
// 2) ALLOCATE BLOCK:
//===========================
void *alloc_block(uint32 size) {
	/*Othman & Enas [PROJECT'25]*/
	assert(size <= DYN_ALLOC_MAX_BLOCK_SIZE);

	uint32 realSize = Othman_next_pow2(size); //0 1 2 3 4 5 6 7 8
	if (realSize < DYN_ALLOC_MIN_BLOCK_SIZE)
		realSize = DYN_ALLOC_MIN_BLOCK_SIZE;

	int index = indexes(realSize);
	struct BlockElement *block = NULL;
	// Case 1
	if (LIST_SIZE(&freeBlockLists[index]) != 0) {
		block = LIST_FIRST(&freeBlockLists[index]);
		to_page_info((uint32) block)->num_of_free_blocks--;
		LIST_REMOVE(&freeBlockLists[index], block);
	}

	// Case 2
	else if (LIST_SIZE(&freePagesList) != 0) {
		struct PageInfoElement *freepage = LIST_FIRST(&freePagesList);
		uint32 page_va = to_page_va(freepage);

		int getpage = get_page((void *) page_va);
		int page_idx = (page_va - dynAllocStart) >> PGSHIFT;

		freepage->block_size = (uint16) realSize;
		freepage->num_of_free_blocks = (1 << PGSHIFT) / realSize;

		uint32 cur_va = page_va;
		for (int i = 0; i < PAGE_SIZE / realSize; ++i) {
			struct BlockElement *blk = (void*) (to_page_va(freepage)
					+ (i * realSize));
			LIST_INSERT_TAIL(&freeBlockLists[index], blk);

		}
		LIST_REMOVE(&freePagesList, freepage);
		block = LIST_FIRST(&freeBlockLists[index]);
		to_page_info((uint32) block)->num_of_free_blocks--;
		LIST_REMOVE(&freeBlockLists[index], block);
		//Case 3
	} else {
		int cur_index = index + 1;
		bool found = 0;
		while (cur_index <= (LOG2_MAX_SIZE - LOG2_MIN_SIZE)) {
			if (LIST_SIZE(&freeBlockLists[cur_index]) != 0) {
				block = LIST_FIRST(&freeBlockLists[cur_index]);
				LIST_REMOVE(&freeBlockLists[cur_index], block);
				uint32 page_va = ROUNDDOWN((uint32 )block, PAGE_SIZE);
				int page_idx = (page_va - dynAllocStart) >> PGSHIFT;
				to_page_info((uint32) block)->num_of_free_blocks--;
				found = 1;
				break;
			}
			cur_index++;
		}
		if (found == 0) {
			panic("no memory");
		}
	}

	return block;
}

//===========================
// [3] GET BLOCK SIZE:
//===========================
__inline__ uint32 get_block_size(void *va) {
	/*Othman & Enas [PROJECT'25]*/
	uint32 page_va = ROUNDDOWN((uint32 )va, PAGE_SIZE);
	return to_page_info((uint32) page_va)->block_size;
}

//===========================
// [4] FREE BLOCK:
//===========================
void free_block(void *va) {
	/*Othman & Enas [PROJECT'25]*/
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		assert((uint32 )va >= dynAllocStart && (uint32 )va < dynAllocEnd);
	}
	uint32 page_va = ROUNDDOWN((uint32 )va, PAGE_SIZE);
	struct PageInfoElement *page = to_page_info(page_va);
	uint32 size = page->block_size;
	int index = indexes(size);
	uint16 total_number_of_blocks = PAGE_SIZE / size;

	struct BlockElement *block = (struct BlockElement *) va;
	LIST_INSERT_TAIL(&freeBlockLists[index], block);
	page->num_of_free_blocks++;

	if (page->num_of_free_blocks == total_number_of_blocks) {
		uint32 base = page_va;
		for (int i = 0; i < total_number_of_blocks; i++) {
			struct BlockElement* blk_va = (void*) (base + i * size);
			LIST_REMOVE(&freeBlockLists[index], blk_va);
		}
		return_page((void *) page_va);
		page->block_size = 0;
		page->num_of_free_blocks = 0;
		LIST_INSERT_TAIL(&freePagesList, page);
	}
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//===========================
// [1] REALLOCATE BLOCK:
//===========================
void *realloc_block(void* va, uint32 new_size) {

	struct BlockElement *block;
	if (va == NULL) {
		alloc_block(new_size);
	} else if (new_size == 0) {
		free_block(va);
	} else {
		uint32 size = get_block_size(va);
		block = alloc_block(new_size);
		memcpy(block, va, size);
		free_block(va);
	}
	return block;

}
