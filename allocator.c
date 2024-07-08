/**
 * @file
 *
 * Explores memory management at the C runtime level.
 *
 * To use (one specific command):
 * LD_PRELOAD=$(pwd)/allocator.so command
 * ('command' will run with your allocator)
 *
 * To use (all following commands):
 * export LD_PRELOAD=$(pwd)/allocator.so
 * (Everything after this point will use your custom allocator -- be careful!)
 */

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>

#include "allocator.h"
#include "logger.h"

#define ALIGN_SIZE 8
#define BLOCK_ALIGN 4

static struct mem_block *g_head = NULL; /*!< Start (head) of our linked list */
static struct mem_block *g_tail = NULL; /*!< End (tail) of our linked list */

static unsigned long g_allocations = 0; /*!< Allocation counter */
static unsigned long g_regions = 0; /*!< regions counter */
static unsigned long g_splits = 0;/*number of blocks split for naming purposes*/

pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER; /*< Mutex for protecting the linked list */

/**
 * Given a free block, this function will split it into two pieces and update
 * the linked list.
 *
 * @param block the block to split
 * @param size new size of the first block after the split is complete,
 * including header sizes. The size of the second block will be the original
 * block's size minus this parameter.
 *
 * @return address of the resulting second block (the original address will be
 * unchanged) or NULL if the block cannot be split.
 */
struct mem_block *split_block(struct mem_block *block, size_t size)
{   
    size_t min_sz = sizeof(struct mem_block) + BLOCK_ALIGN;

    if(size < min_sz){
        return NULL;
    }
    if(block->free == false){
        return NULL;
    }
    size_t rm_sz = block->size - size;
    if(rm_sz < 104){
        return NULL;
    }
    
    struct mem_block *new_block = (void *) block + size;
    LOG("Splitting block: %p\n", new_block);
    if(block == g_tail){
        block->next = new_block;
        new_block->prev = block;
        new_block->next = NULL;
        g_tail = new_block;
    }
    else{
        new_block->next = block->next;
        block->next->prev = new_block;
        new_block->prev = block;
        block->next = new_block;
    }

    snprintf(new_block->name, 32, "Split block %lu", g_splits++);
    new_block->size = rm_sz;
    new_block->free = true;
    new_block->region_id = block->region_id;
    block->size = size;
    LOG("returning from split_block: %p\n", new_block);
    return new_block;
}

/**
 * Given a free block, this function attempts to merge it with neighboring
 * blocks --- both the previous and next neighbors --- and update the linked
 * list accordingly.
 *
 * @param block the block to merge
 *
 * @return address of the merged block or NULL if the block cannot be merged.
 */
struct mem_block *merge_block(struct mem_block *block)
{

    if(block->next != NULL){
        if(block->next->free == true && block->next->region_id == block->region_id){//if next and block are in same region
            // LOG("2 blocks down (block->next->next): %p\n", block->next->next);
            block->size = block->size + block->next->size;
            // LOG("merging block->next + block = %zu\n", block->size);
            // LOG("merge block: %p\n", block);
            if(block->next == g_tail){
                g_tail = block;
                block->next->prev = NULL;
                block->next = NULL;
            }
            else{
                // LOG("before pointing to block->next->next...: %p\n", block->next);
                block->next = block->next->next;
                block->next->prev = block;
                // LOG("after: block->next: %p\n", block->next);
            }
        }
    }
    if(block->prev != NULL){
        if(block->prev->free == true && block->prev->region_id == block->region_id){//if prev and block are in same region
            // LOG("2 blocks down from previous block(block->next): %p\n", block->next);
            block->prev->size = block->prev->size + block->size;
            // LOG("merging block prev + block = %zu\n", block->prev->size);
            if(block == g_tail){
                g_tail = block->prev;
                block->prev->next = NULL;
                block->prev = NULL;
            }
            else{
                // LOG("before pointing to prev block to block->next...: %p\n", block->prev->next);
                block->prev->next = block->next;
                block->prev->next->prev = block->prev;
                // LOG("after: block->prev->next: %p\n", block->prev->next);
            }    
        }
    }
    if(block == g_head && block == g_tail){//if it was only block in memory unmap
        g_head = NULL;
        g_tail = NULL;
        if(munmap(block, block->size) == -1){
            perror("munmap");
            return NULL;
        }
    }
    else if((block->next != NULL && block->next->region_id != block->region_id) && (block->prev != NULL && block->prev->region_id != block->region_id)){//prev & next are in diff regions
        block->prev->next = block->next;
        block->next->prev = block->prev;
        if(munmap(block, block->size) == -1){
            perror("munmap");
            return NULL;
        }
    }
    else if(block->prev != NULL && block == g_tail && block->prev->region_id != block->region_id){//if block is tail and only block in region
        g_tail = block->prev;
        block->prev->next = NULL;
        block->prev = NULL;
        if(munmap(block, block->size) == -1){
            perror("munmap");
            return NULL;
        }
    }
    else if(block->next != NULL && block == g_head && block->next->region_id != block->region_id){//if block is head and only block in region
        g_head = block->next;
        block->next->prev = NULL;
        block->next = NULL;
        if(munmap(block, block->size) == -1){
            perror("munmap");
            return NULL;
        }
    }
    return block;
}

/**
 * Given a block size (header + data), locate a suitable location using the
 * first fit free space management algorithm.
 *
 * @param size size of the block (header + data)
 */
void *first_fit(size_t size)
{
    struct mem_block *current = g_head;
    while(current != NULL){
        if(size <= current->size && current->free == true){
            LOG("First fit: current name = %s\n", current->name);
            return current;
        }
        // LOG("First fit: current name = %s\n", current->name);
        // LOG("First fit: current is free = %d\n", current->free);
        // LOG("First fit: current size = %zu\n", current->size);
        current = current->next;
    }
    return NULL;
}

/**
 * Given a block size (header + data), locate a suitable location using the
 * worst fit free space management algorithm. If there are ties (i.e., you find
 * multiple worst fit candidates with the same size), use the first candidate
 * found.
 *
 * @param size size of the block (header + data)
 */
void *worst_fit(size_t size)
{
    struct mem_block *current = g_head;
    struct mem_block *worst = NULL;
    ssize_t worst_size = INT_MIN;
    while(current != NULL){
        if(size <= current->size && current->free == true){
            ssize_t diff = (ssize_t) current->size - size;
            if(diff > worst_size){
                worst = current;
                worst_size = diff;
            }
        }
        current = current->next;
    }
    return worst;
}

/**
 * Given a block size (header + data), locate a suitable location using the
 * best fit free space management algorithm. If there are ties (i.e., you find
 * multiple best fit candidates with the same size), use the first candidate
 * found.
 *
 * @param size size of the block (header + data)
 */
void *best_fit(size_t size)
{
    struct mem_block *current = g_head;
    struct mem_block *best = NULL;
    size_t best_size = INT_MAX;
    while(current != NULL){
        if(size <= current->size && current->free == true){
            ssize_t diff = (ssize_t) current->size - size;
            if(diff < best_size){
                best = current;
                best_size = diff;
            }
        }
        current = current->next;
    }
    return best;
}

void *reuse(size_t size)
{
    char *algo = getenv("ALLOCATOR_ALGORITHM");
    if(algo == NULL){
        algo = "first_fit";
    }

    void *reused_block = NULL;

    if (strcmp(algo, "first_fit") == 0) {
        reused_block = first_fit(size);
    } else if (strcmp(algo, "best_fit") == 0) {
        reused_block = best_fit(size);
    } else if (strcmp(algo, "worst_fit") == 0) {
        reused_block = worst_fit(size);
    }

    if(reused_block != NULL){
        split_block(reused_block, size);      
    }
    return reused_block;
}

void *malloc_name(size_t size, char *name){
    void *alloc = malloc(size);
    if(alloc == NULL){
        return NULL;
    }
    struct mem_block *new_block = (struct mem_block *) alloc - 1;
    strcpy(new_block->name, name);
    LOG("Created name block: %s\n", name);
    return alloc;
}

void *malloc(size_t size)
{
    pthread_mutex_lock(&alloc_mutex);
    static const int prot_flags = PROT_READ | PROT_WRITE;
    static const int map_flags = MAP_PRIVATE | MAP_ANONYMOUS;
    size_t total_size = size + sizeof(struct mem_block);
    size_t aligned_size = total_size;
    if(aligned_size % ALIGN_SIZE != 0){
        aligned_size = aligned_size + ALIGN_SIZE - (total_size % ALIGN_SIZE);
    }
    LOG("allocation request; size = %zu, total = %zu, aligned = %zu\n", size, total_size, aligned_size);
    
    char *scrabble = getenv("ALLOCATOR_SCRIBBLE");
    bool scribbles = false;
    if (scrabble != NULL && (atoi(scrabble) == 1)) {
        scribbles = true;
    }

    struct mem_block *reused_block = reuse(aligned_size);
    if(reused_block != NULL){
        reused_block->free = false;
        if (scribbles) {
            memset(reused_block + 1, 0xAA, size);
        }
        pthread_mutex_unlock(&alloc_mutex);
        return reused_block + 1;
    }

    int page_size = getpagesize();
    size_t num_pages = aligned_size / page_size;
    if (aligned_size % page_size != 0){
        num_pages++;
    }

    size_t region_size = num_pages * page_size;
    LOG("New region; size = %zu\n", region_size);
    
    struct mem_block *new_block = mmap(NULL, region_size, prot_flags, map_flags, -1, 0);
    
    if (new_block == MAP_FAILED) {
        perror("mmap");
        pthread_mutex_unlock(&alloc_mutex);
        return NULL;
    }

    snprintf(new_block->name, 32, "Allocation %lu", g_allocations++);
    new_block->region_id = g_regions++;

    if(g_head == NULL && g_tail == NULL){
        g_head = new_block;
        g_tail = g_head;
        new_block->prev = NULL;
    }
    else{
        g_tail->next = new_block;
        new_block->prev = g_tail;
        g_tail = new_block;
    }

    new_block->free = true;
    new_block->size = region_size;
    new_block->next = NULL;
    split_block(new_block, aligned_size);
    new_block->free = false;

    LOG("New allocation %p (data = %p)\n", new_block, new_block + 1);
    if (scribbles) {
        memset(new_block + 1, 0xAA, size);
    }
    pthread_mutex_unlock(&alloc_mutex);
    return new_block + 1;
}

void free(void *ptr)
{
    pthread_mutex_lock(&alloc_mutex);
    if (ptr == NULL) {
        /* Freeing a NULL pointer does nothing */
        pthread_mutex_unlock(&alloc_mutex);
        return;
    }
    struct mem_block *block = (struct mem_block *) ptr - 1;
    LOG("Free request; address = %p, size = %zu\n", ptr, block->size);

    block->free = true;
    merge_block(block);
    pthread_mutex_unlock(&alloc_mutex);
    // LOG("Block size: %zu\n", block->size);
}

void *calloc(size_t nmemb, size_t size)
{
    void *mem_block = malloc(nmemb * size);
    LOG("Writing 0 over memory block at %p\n", mem_block);
    memset(mem_block, 0, nmemb * size);
    return mem_block;
}

void *realloc(void *ptr, size_t size)
{
    LOG("Rellocation request; address = %p, new size = %zu\n", ptr, size);
    if (ptr == NULL) {
        /* If the pointer is NULL, then we simply malloc a new block */
        return malloc(size);
    }

    if (size == 0) {
        /* Realloc to 0 is often the same as freeing the memory block... But the
         * C standard doesn't require this. We will free the block and return
         * NULL here. */
        free(ptr);
        return NULL;
    }
    struct mem_block *block = (struct mem_block *) ptr - 1;
    
    block = malloc(size);
    memcpy(block, ptr, size);
    
    return block;
}

/**
 * print_memory
 *
 * Prints out the current memory state, including both the regions and blocks.
 * Entries are printed in order, so there is an implied link from the topmost
 * entry to the next, and so on.
 */
void print_memory(void)
{
    puts("-- Current Memory State --");
    struct mem_block *current_block = g_head;
    struct mem_block *current_region = g_head;

    while(current_block != NULL){
        if(current_block->region_id != current_region->region_id || current_block == g_head){
            printf("[REGION] %lu] %p\n", current_block->region_id, current_block);
            current_region = current_block;
        }
        printf("  [BLOCK] %p-%p \'%s' %zu [%s]\n", current_block, (char *) current_block + current_block->size, current_block->name, current_block->size, current_block->free ? "FREE" : "USED");
        current_block = current_block->next;
    }
    
}

