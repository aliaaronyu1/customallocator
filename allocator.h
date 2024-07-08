/**
 * @file
 *
 * Function prototypes and globals for our memory allocator implementation.
 */

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>
#include <stdbool.h>

/* -- Helper functions -- */
/**
 * Split_block takes in a free memory block and splits the block into two blocks according to size
 * @param block the free block that is getting split
 * @param size the size the block should get limited to. 
 * This is the new size of block. The other block is now a free block
 * 
 * @return returns the second block that is now free memory block. Returns NULL if block can't be split
 * 
 */
struct mem_block *split_block(struct mem_block *block, size_t size);

/**
 * merge_block takes in a free memory block merges with it's neighbor blocks, prev and next.
 * @param block the free block will merge with next and prev
 * 
 * @return return the block that was merged or NULL if it couldn't merge
 * 
 */
struct mem_block *merge_block(struct mem_block *block);

/**
 * reuse determines which free space management algorithm to use for reusing a block of free memory
 * @param size the alligned size to set the reused block to 
 * 
 * @return returns pointer to block being reused
 * 
 */
void *reuse(size_t size);

/**
 * first_fit free space memory algorithm determines the first suitable block to use
 * @param size the alligned size to compare with linked list blocks
 * 
 * @return returns pointer to first suitable block or NULL is there is no suitable block
 * 
 */
void *first_fit(size_t size);

/**
 * worst_fit free space memory algorithm determines the worst suitable block to use
 * @param size the alligned size to compare with linked list blocks
 * 
 * @return returns pointer to worst block or NULL if no suitable block
 * 
 */
void *worst_fit(size_t size);

/**
 * best_fit free space memory algorithm determines the best suitable block to use
 * @param size the alligned size to compare with linked list blocks
 * 
 * @return returns pointer best suitable block or NULL if no suitable block
 * 
 */
void *best_fit(size_t size);

/**
 * print_memory prints the linked list of memory with regions and blocks
 */
void print_memory(void);

/* -- C Memory API functions -- */
/**
 * malloc_name does a malloc but also assigns pointer a name from struct for debugging
 * @param size size to malloc
 * @param name name of the allocation that is assigned to struct member variable
 * 
 * @return returns pointer best suitable block or NULL if no suitable block
 */
void *malloc_name(size_t size, char *name);

/**
 * malloc allocates memory. requests memory from kernel and updates linked list 
 * @param size size to malloc
 * 
 * @return pointer of newly created block or reused block 
 */
void *malloc(size_t size);

/**
 * free will free the allocated memory that is requested
 * @param ptr requested void pointer to free
 * 
 */
void free(void *ptr);

/**
 * calloc allocates memory like malloc but also initializes all memory to zero
 * @param nmemb number of the certain data type
 * @param size size of the data type. will get multiplied to nmemb before mallocing
 * 
 * @return pointer of new block calloc'd 
 */
void *calloc(size_t nmemb, size_t size);

/**
 * realloc allocates memory like malloc but also resizes if the requested size if bigger. Free's if size is 0 and is unchanged if the size is less than ptr->size
 * @param ptr pointer to resize
 * @param size size to know how size to resize to
 * 
 * @return pointer of new block that was resized, freed or unchanged ptr
 */
void *realloc(void *ptr, size_t size);

/* -- Data Structures -- */

/**
 * Defines metadata structure for both memory 'regions' and 'blocks.' This
 * structure is prefixed before each allocation's data area.
 */
/**
 * @struct mem_block for each block of memory in a region. It is implemented as a doubly linked list 
 * @var name name of memory block. Mainly used for debugging purposes
 * @var size size of the block of memory. Program will split memory from free blocks to the requested size
 * @var free whether the block is free. Used when splitting and merging.
 * @var region_id The region of each memory block. Each region was mmap'd when there were no more reusable memory in the previous region
 * @var next points to the next block in the linked list
 * @var prev points to previous block in linked list since it is doubly linked
 * @var padding creates filler space so that the header is equal to 100 bytes
 * 
 */
struct mem_block {
    /**
     * The name of this memory block. If the user doesn't specify a name for the
     * block, it should be auto-generated based on the allocation ID. The format
     * should be 'Allocation X' where X is the allocation ID.
     */
    char name[32];

    /** Size of the block */
    size_t size;

    /** Whether or not this block is free */
    bool free;

    /**
     * The region this block belongs to.
     */
    unsigned long region_id;

    /** Next block in the chain */
    struct mem_block *next;

    /** Previous block in the chain */
    struct mem_block *prev;

    /**
     * "Padding" to make the total size of this struct 100 bytes. This serves no
     * purpose other than to make memory address calculations easier. If you
     * add members to the struct, you should adjust the padding to compensate
     * and keep the total size at 100 bytes; test cases and tooling will assume
     * a 100-byte header.
     */
    char padding[35];
} __attribute__((packed));

#endif
