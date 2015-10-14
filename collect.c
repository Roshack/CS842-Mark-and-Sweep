/*
 * The collector
 *
 * Copyright (c) 2014, 2015 Gregor Richards
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "ggggc/gc.h"
#include "ggggc-internals.h"

#ifdef __cplusplus
extern "C" {
#endif


ggc_size_t ggggc_wordsIntoPool(void *x)
{
    struct GGGGC_Pool *pool = (struct GGGGC_Pool *) (GGGGC_POOL_OUTER_MASK & (ggc_size_t) x);
    return ((ggc_size_t) x - (ggc_size_t) pool->start)/sizeof(ggc_size_t);
}

void ggggc_freeObject(void *x)
{
    struct GGGGC_Header *hdr = x;
    struct GGGGC_Pool *pool = (struct GGGGC_Pool *) (GGGGC_POOL_OUTER_MASK & (long unsigned int) x);
    ggc_size_t size = hdr->descriptor__ptr->size;
    ggc_size_t startWord = ggggc_wordsIntoPool(x);
    ggc_size_t endWord = startWord + hdr->descriptor__ptr->size - 1;
    ggc_size_t startIndex = startWord/GGGGC_BITS_PER_WORD;
    ggc_size_t endIndex = endWord/GGGGC_BITS_PER_WORD;
    int startRem = startWord % GGGGC_BITS_PER_WORD;
    int endRem = endWord % GGGGC_BITS_PER_WORD;
    ggc_size_t full = 0xFFFFFFFFFFFFFFFF;
    if (startIndex == endIndex) {
        int i = 0;
        for (i = 0; i < hdr->descriptor__ptr->size; i++) {
            ggc_size_t T = 1L << (startRem+i);
            full = full^T;
        }
        pool->freeBits[startIndex] = pool->freeBits[startIndex]&full;
    } else {
        ggc_size_t diff = endIndex - startIndex;
        int i = 0;
        for (i = 0; i < diff; i++) {
            full = 0xFFFFFFFFFFFFFFFF;
            if (i!=0 && i!=(diff-1)) {
                // If our object's size is big and it's location made it so
                // the end index isn't just start index + 1 then the inside indices
                // just need to be entirely freed!
                full = 0;
            } else if (i == 0) {
                int j = startRem;
                for (j = startRem; j < GGGGC_BITS_PER_WORD; j++) {
                    ggc_size_t T = 1L << j;
                    full = full^T;
                }
            } else {
                int j = 0;
                // using <= cuz we made it so the endRem is actaully hte last word
                // that is in use, not the word we go up to.
                for (j = 0; j <= endRem; j++) {
                    ggc_size_t T = 1L << j;
                    full = full^T;
                }
            }
            pool->freeBits[startIndex + i] = pool->freeBits[startIndex + i] & full;
        }
    }  
}

struct StackLL 
{
    void *data;
    struct StackLL *next;
};

struct StackLL * markStack;

void StackLL_Init()
{
    markStack = (struct StackLL *)malloc(sizeof(struct StackLL));
    markStack->data = NULL;
    markStack->next = NULL;
}

void StackLL_Push(void *x)
{
    struct StackLL *new = (struct StackLL *)malloc(sizeof(struct StackLL));
    new->data = x;
    new->next = markStack;
    markStack = new;
}

void * StackLL_Pop()
{
    void * x = markStack->data;
    struct StackLL *old = markStack;
    if (x) {
        markStack = markStack->next;
        free(old);
    }
    return x;
}

void StackLL_Clean()
{
    while(markStack->next) {
        StackLL_Pop();
    }
    free(markStack);
}

extern int ggggc_forceCollect;


long unsigned int ggggc_isMarked(void * x)
{  
    struct GGGGC_Pool *pool = (struct GGGGC_Pool *) (GGGGC_POOL_OUTER_MASK & (long unsigned int) x);
    ggc_size_t startWord = ggggc_wordsIntoPool(x);
    ggc_size_t ind = startWord/GGGGC_BITS_PER_WORD;
    ggc_size_t rem = startWord % GGGGC_BITS_PER_WORD;
    ggc_size_t mask = 1L << rem;
    return ((pool->freeBits[ind]) & mask);
    //return (long unsigned int) ((struct GGGGC_Header *) x)->descriptor__ptr & 1l;
}

void ggggc_markObject(void *x)
{
    ggggc_useFree(x);
    /*
    struct GGGGC_Header *header = (struct GGGGC_Header *) x;
    header->descriptor__ptr = (struct GGGGC_Descriptor *) 
                              ((long unsigned int) header->descriptor__ptr | 1l );
    */
}
void ggggc_unmarkObject(void *x)
{
    struct GGGGC_Header *header = (struct GGGGC_Header *) x;
    /* probably shouldn't hardcode constant for max long -1 but oh welllllll */
    header->descriptor__ptr = (struct GGGGC_Descriptor *) 
                              ((long unsigned int) header->descriptor__ptr & 0xFFFFFFFFFFFFFFFE );
}

void * ggggc_cleanMark(void *x)
{
   struct GGGGC_Header * header = (struct GGGGC_Header *) x;
   return (struct GGGGC_Descriptor *) ((long unsigned int) header->descriptor__ptr & 0xFFFFFFFFFFFFFFFE );
}


void ggggc_mark()
{
    struct GGGGC_Pool *poolIter =  ggggc_poolList;
    //printf("pooliter is %lx\r\n", (long unsigned int) poolIter);
    while (poolIter) {
        ggc_size_t * iter = poolIter->start;
        poolIter->currentFreeMax = GGGGC_MAX_WORD;
        poolIter->firstFree = 0;
        int i = 0;
        for (i = 0; i < GGGGC_FREEBIT_ARRAY_SIZE; i++) {
            poolIter->freeBits[i] = 0;
        }
        poolIter = poolIter->next;
    }
    struct GGGGC_PointerStack *stack_iter;
    int x = 0;
    while (x < 2) {
        // If it's our first time through loop go through pointerstack if not pointerstack globals
        stack_iter = x == 0 ? ggggc_pointerStack : ggggc_pointerStackGlobals;
        while(stack_iter) {
            struct GGGGC_Header *** ptrptr = (struct GGGGC_Header ***) stack_iter->pointers;
            if (**ptrptr) {
                /* Here header is the pointer to the header of the object we're currently looking at
                   the reference in the stack for (given by stack_iter), so we can mark it by
                   updating header->descriptor_ptr */
                struct GGGGC_Header *header= **ptrptr;
                /* Check if this object is already marked, the first object off the stack never will be,
                   but after recursing down the first one future ones could be */
                if (!ggggc_isMarked((void*) header)) {
                    //fprintf(stderr,"First found root %lx\r\n", (long unsigned int) header);
                    StackLL_Push((void *) header);
                    ggggc_markHelper();
                }
            }
            stack_iter = stack_iter->next;
        }
        x++;
    }
}

void ggggc_markHelper()
{
    // Pop off our mark stack...
    void * x = StackLL_Pop();
    while(x)
    {
        if (ggggc_isMarked(x)) {
            x = StackLL_Pop();
            continue;
        }
        // Get the descriptor for this object by dereferencing the cleaned descriptor ptr
        //printf("Marking object %lx of size %ld\r\n", (long unsigned int) x, ((struct GGGGC_Header *) x)->descriptor__ptr->size);
        struct GGGGC_Descriptor *descriptor = (struct GGGGC_Descriptor *) ggggc_cleanMark(x);
        ggggc_markObject(x);
        //printf("Trying to read the descriptor of an object at %lx and that descriptor is %lx\r\n", (long unsigned int) x, (long unsigned int) (((struct GGGGC_Header *) x)->descriptor__ptr));
        if (descriptor->pointers[0]&1) {
            long unsigned int bitIter = 1;
            int z = 0;
            while (z < descriptor->size) {
                if (descriptor->pointers[0] & bitIter) {
                    /* so we found a pointer in our object so check it out */
                    void * newPtr = (void *) (((ggc_size_t *) x)+z);
                    struct GGGGC_Header **newHeader = (struct GGGGC_Header **) newPtr;
                    if (*newHeader) {
                        struct GGGGC_Header * next = *newHeader;
                        if (!z) {
                            // If z is 0 this is our descriptor ptr and we need to clean it first.
                            next = (struct GGGGC_Header *) descriptor;
                        }
                        //fprintf(stderr,"Object at %lx points to %lx in its %d word\r\n", (long unsigned int) x, (long unsigned int) next, z);
                        StackLL_Push((void *) next);
                    }
                }
                z++;
                bitIter = bitIter << 1;
            }
        } else {
            // It only has it's descriptor ptr in it, this is still necessary to check.
            StackLL_Push((void *) descriptor);
        }
        x = StackLL_Pop();
    }
}


void ggggc_sweep()
{
    struct GGGGC_Pool *poolIter =  ggggc_poolList;
    //printf("pooliter is %lx\r\n", (long unsigned int) poolIter);
    while (poolIter) {
        ggc_size_t * iter = poolIter->start;
        poolIter->currentFreeMax = GGGGC_MAX_WORD;
        poolIter->firstFree = 0;
        int i = 0;
        for (i = 0; i < GGGGC_FREEBIT_ARRAY_SIZE; i++) {
            poolIter->freeBits[i] = GGGGC_MAX_WORD;
        }
        while (iter < poolIter->free && iter) {
            /* if object is not marked add to free list... */
            struct GGGGC_Descriptor *desc = ggggc_cleanMark((void *) iter);
            size_t size;
            size = desc->size;
            if (ggggc_isMarked(iter)) {
                ggggc_unmarkObject(iter); 
            } else {
                // Should put it on the freelist if it's not reachable! duh.
                // Right now putting each object we find at the START Of the freelist... maybe not
                // the best but oh well. Easily solved by adding a variable to keep track of
                // where we are in the free list.
                // Turns out after some testing doing it this way is WAYYYYYYYYYYY faster for
                // the bench test program so.... yeah gonna keep doing it this way...
                ggggc_freeObject(iter);
                //struct GGGGC_FreeObject *newFree = (struct GGGGC_FreeObject *) iter;

                //printf("iter is %lx\r\n", (long unsigned int) iter);
                //printf("newfree Next is %lx\r\n", (long unsigned int) &newFree->next);

                //newFree->next = poolIter->freeList;
                //poolIter->freeList = newFree;

                /*
                if (oldFree) {
                    oldFree->next = newFree;
                    oldFree = newFree;
                } else {
                    newFree->next = NULL;
                    poolIter->freeList = newFree;
                    oldFree = newFree;
                }*/
                //printf("Free object found at %lx\r\n", (long unsigned int) newFree);
            }
            iter = iter + size;
        }
        poolIter = poolIter->next; 
    }
}

/* run a collection */
void ggggc_collect()
{
    //printf("running mark\r\n");
    StackLL_Init();
    ggggc_mark();
    StackLL_Clean();
    //printf("Finished mark\r\n");
    //ggggc_sweep();
    // If we've ran a collection we need to reset the curpool.
    //printf("completed sweep\r\n");
    ggggc_forceCollect = 0;
    ggggc_curPool = ggggc_poolList;
}


/* explicitly yield to the collector */
int ggggc_yield()
{
    /* FILLME */
    if (ggggc_forceCollect) {
        ggggc_collect();
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
