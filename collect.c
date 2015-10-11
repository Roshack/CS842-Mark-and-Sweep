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

extern int ggggc_forceCollect;

long unsigned int ggggc_isMarked(void * x)
{  
    return (long unsigned int) ((struct GGGGC_Header *) x)->descriptor__ptr & 1l;
}

void ggggc_markObject(void *x)
{
    struct GGGGC_Header *header = (struct GGGGC_Header *) x;
    header->descriptor__ptr = (struct GGGGC_Descriptor *) 
                              ((long unsigned int) header->descriptor__ptr | 1l );
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
                    printf("First found root %lx\r\n", (long unsigned int) header);
                    ggggc_markHelper((void *) header);
                }
            }
            stack_iter = stack_iter->next;
        }
        x++;
    }
}

void ggggc_markHelper(void * x)
{
    // If this guy is marked already run away before you segfault doing other stuff to it.
    if (ggggc_isMarked(x)) {
        return;
    }
    // Get the descriptor for this object by dereferencing the cleaned descriptor ptr   
    struct GGGGC_Descriptor *descriptor = (struct GGGGC_Descriptor *) ggggc_cleanMark(x);
    ggggc_markObject(x);
    if (descriptor->pointers[0]&1) {
        long unsigned int bitIter = 1;
        int z = 0;
        while (z < 63) {
            if (descriptor->pointers[0] & bitIter) {
                /* so we found a pointer in our object so check it out */
                void * newPtr = x+((z)*sizeof(void*));
                struct GGGGC_Header **newHeader = (struct GGGGC_Header **) newPtr;
                if (*newHeader) {
                    struct GGGGC_Header * next = *newHeader;
                    if (!z) {
                        // If z is 0 this is our descriptor ptr and we need to clean it first.
                        next = (struct GGGGC_Header *) descriptor;
                    }
                    //printf("Object at %lx points to %lx in its %d word\r\n", (long unsigned int) x, (long unsigned int) next, z);
                    ggggc_markHelper((void *) next);
                }
            }
            z++;
            bitIter = bitIter << 1;
        }
    }
}


void ggggc_sweep()
{
    struct GGGGC_Pool *poolIter =  ggggc_poolList;
    printf("pooliter is %lx\r\n", (long unsigned int) poolIter);
    while (poolIter) {
        ggc_size_t * iter = poolIter->start;
        poolIter->freeList = NULL;
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
                struct GGGGC_FreeObject *newFree = (struct GGGGC_FreeObject *) iter;
                newFree->next = poolIter->freeList;
                poolIter->freeList = newFree;
            }
            iter = iter + size;
        }
        poolIter = poolIter->next; 
    }
}

/* run a collection */
void ggggc_collect()
{
    /* FILLME */
    printf("running mark\r\n");
    ggggc_mark();
    printf("running sweep\r\n");
    ggggc_sweep();
    // If we've ran a collection we need to reset the curpool.
    ggggc_forceCollect = 0;
    ggggc_curPool = ggggc_poolList;
}


/* explicitly yield to the collector */
int ggggc_yield()
{
    /* FILLME */
    if (ggggc_forceCollect) {
        printf("Forcecollect is %d\r\n", ggggc_forceCollect);
        ggggc_collect();
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
