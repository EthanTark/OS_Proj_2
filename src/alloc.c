#include "alloc.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define ALIGNMENT 16 /**< The alignment of the memory blocks */
//This comment is in here for github reason
static free_block *HEAD = NULL; /**< Pointer to the first element of the free list */

/**
 * Split a free block into two blocks
 *
 * @param block The block to split
 * @param size The size of the first new split block
 * @return A pointer to the first block or NULL if the block cannot be split
 */
void *split(free_block *block, int size) {
    if((block->size < size + sizeof(free_block))) {
        return NULL;
    }

    void *split_pnt = (char *)block + size + sizeof(free_block);
    free_block *new_block = (free_block *) split_pnt;

    new_block->size = block->size - size - sizeof(free_block);
    new_block->next = block->next;

    block->size = size;

    return block;
}

/**
 * Find the previous neighbor of a block
 *
 * @param block The block to find the previous neighbor of
 * @return A pointer to the previous neighbor or NULL if there is none
 */
free_block *find_prev(free_block *block) {
    free_block *curr = HEAD;
    while(curr != NULL) {
        char *next = (char *)curr + curr->size + sizeof(free_block);
        if(next == (char *)block)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * Find the next neighbor of a block
 *
 * @param block The block to find the next neighbor of
 * @return A pointer to the next neighbor or NULL if there is none
 */
free_block *find_next(free_block *block) {
    char *block_end = (char*)block + block->size + sizeof(free_block);
    free_block *curr = HEAD;

    while(curr != NULL) {
        if((char *)curr == block_end)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * Remove a block from the free list
 *
 * @param block The block to remove
 */
void remove_free_block(free_block *block) {
    free_block *curr = HEAD;
    if(curr == block) {
        HEAD = block->next;
        return;
    }
    while(curr != NULL) {
        if(curr->next == block) {
            curr->next = block->next;
            return;
        }
        curr = curr->next;
    }
}

/**
 * Coalesce neighboring free blocks
 *
 * @param block The block to coalesce
 * @return A pointer to the first block of the coalesced blocks
 */
void *coalesce(free_block *block) {
    if (block == NULL) {
        return NULL;
    }

    free_block *prev = find_prev(block);
    free_block *next = find_next(block);

    // Coalesce with previous block if it is contiguous.
    if (prev != NULL) {
        char *end_of_prev = (char *)prev + prev->size + sizeof(free_block);
        if (end_of_prev == (char *)block) {
            prev->size += block->size + sizeof(free_block);

            // Ensure prev->next is updated to skip over 'block', only if 'block' is directly next to 'prev'.
            if (prev->next == block) {
                prev->next = block->next;
            }
            block = prev; // Update block to point to the new coalesced block.
        }
    }

    // Coalesce with next block if it is contiguous.
    if (next != NULL) {
        char *end_of_block = (char *)block + block->size + sizeof(free_block);
        if (end_of_block == (char *)next) {
            block->size += next->size + sizeof(free_block);

            // Ensure block->next is updated to skip over 'next'.
            block->next = next->next;
        }
    }

    return block;
}

/**
 * Call sbrk to get memory from the OS
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the allocated memory
 */
void *do_alloc(size_t size) {
    void* p = sbrk(0); //Grab point in memory somewhere with pointer at it
    intptr_t align = (intptr_t)p&(ALIGNMENT-1); // get address if pointer p and find last digit
    intptr_t ag = (align == 0)?0:ALIGNMENT - align; // Compare last digit of p with alignment

    void *mem = sbrk(size + ag + sizeof(header)); // Grab memory big enough for size, alignment, and header size
    
    if (mem == (void *)-1) return NULL; // sbrk fails return NULL
    void* head = (void*)((intptr_t)mem + ag); // pointer for head
    header *block = (header*)head; //casting for header type
    block->size = size; // grab size of the block where pointer is pointing
    block->magic = 0x01234567; // Magic number for error checking
    return (void *)((char *)head + sizeof(header)); // return the proper size of the memory space
}

/**
 * Allocates memory for the end user
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the requested block of memory
 */
void *tumalloc(size_t size) {

    if (HEAD == NULL){ // Check if there is any blocks
        void* ptr = do_alloc(size); // allocate for new blocks
        return ptr; // return pointer for new block
    }
    else{
        free_block* curr = HEAD; //pointer for traversal starting at the head 
        while (curr != NULL){ // while loop until end of linked list
            if (size+sizeof(header) <= curr -> size){ // Check for fitting block
                void *ptr = split(curr, size+sizeof(header)); //Split block for better size
                if(ptr == NULL) return NULL;
                remove_free_block(ptr); // remove other block
                header* data = (header*)ptr; // casting for header
                data -> size = size; // size grab
                data -> magic = 0x01234567; // majic number checking
                return ptr + sizeof(header); // return proper space for the memory //return ptr* + sizeof(header);
            }
            curr = curr->next; // next block in chain
        }
           void* ptr = do_alloc(size); //NULL so allocate space
        return ptr; // Return the new space
    }   
}

/**
 * Allocates and initializes a list of elements for the end user
 *
 * @param num How many elements to allocate
 * @param size The size of each element
 * @return A pointer to the requested block of initialized memory
 */
void *tucalloc(size_t num, size_t size) {
    void *ptr = tumalloc(size*num); //Grab new point of memory for set size
    if (ptr == NULL) { // error checking
        return NULL; // Return end
    }
    memset(ptr, 0, size*num); // set in the memory value of 0
    return ptr; // return pointer with updated values
}

/**
 * Reallocates a chunk of memory with a bigger size
 *
 * @param ptr A pointer to an already allocated piece of memory
 * @param new_size The new requested size to allocate
 * @return A new pointer containing the contents of ptr, but with the new_size
 */
void *turealloc(void *ptr, size_t new_size) {
    void* updated = tumalloc(new_size); //updated location for difference of size
    if(updated == NULL) //if updated size is NULL problem
        return NULL;
    
    header* hdr = (header*)(ptr - sizeof(header)); //hdr with eh point and size of header
    if(hdr -> magic != 0x01234567){ //if hdr location no magic number memory error
        printf("turealloc magic error");
        abort();
    }
    memcpy(updated, ptr, hdr -> size); // set values of memory to different place
    tufree(ptr); //free up memory
    return updated; // send back updated memory
}

/**
 * Removes used chunk of memory and returns it to the free list
 *
 * @param ptr Pointer to the allocated piece of memory
 */
void tufree(void *ptr) {
    header* hdr = (header*)(ptr - sizeof(header)); //getting hder location
    if(hdr -> magic != 0x01234567){// magic number check
        //printf("Freeing pointer: %p | header: %p | size: %lu | magic: 0x%x\n", ptr, hdr, hdr->size, hdr->magic);
        printf("tufree magic error");
        abort();
    }
    free_block* blk = (free_block*)hdr; // casting to free_block
    blk -> size = hdr ->size;
    blk -> next = HEAD; // Fixing pointer for block
    HEAD = coalesce(blk); // Combine blocks to sow up space

}

