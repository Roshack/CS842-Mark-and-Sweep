/*
 * Allocation functions
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

#define _BSD_SOURCE /* for MAP_ANON */
#define _DARWIN_C_SOURCE /* for MAP_ANON on OS X */

/* for standards info */
#if defined(unix) || defined(__unix) || defined(__unix__) || \
    (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#if _POSIX_VERSION
#include <sys/mman.h>
#endif

/* REMOVE THIS FOR SUBMISSION! */
//#include <gc.h>
/* --- */

#include "ggggc/gc.h"
#include "ggggc-internals.h"

#ifdef __cplusplus
extern "C" {
#endif

/* figure out which allocator to use */
#if defined(GGGGC_USE_MALLOC)
#define GGGGC_ALLOCATOR_MALLOC 1
#include "allocate-malloc.c"

#elif _POSIX_ADVISORY_INFO >= 200112L
#define GGGGC_ALLOCATOR_POSIX_MEMALIGN 1
#include "allocate-malign.c"

#elif defined(MAP_ANON)
#define GGGGC_ALLOCATOR_MMAP 1
#include "allocate-mmap.c"

#elif defined(_WIN32)
#define GGGGC_ALLOCATOR_VIRTUALALLOC 1
#include "allocate-win-valloc.c"

#else
#warning GGGGC: No allocator available other than malloc!
#define GGGGC_ALLOCATOR_MALLOC 1
#include "allocate-malloc.c"

#endif

void ggggc_useFree(void * x)
{
    struct GGGGC_Header *hdr = x;
    struct GGGGC_Pool *pool = (struct GGGGC_Pool *) (GGGGC_POOL_OUTER_MASK & (long unsigned int) x);
    //printf("pool of %lx is %lx\r\n", (long unsigned int) x, (long unsigned int) pool);
    ggc_size_t size = hdr->descriptor__ptr->size;
    ggc_size_t startWord = ggggc_wordsIntoPool(x);
    ggc_size_t endWord = startWord + hdr->descriptor__ptr->size;
    ggc_size_t iter = startWord;
    ggc_size_t ind;
    ggc_size_t rem;
    ggc_size_t mask;
    //printf("using free for object of size %ld at %lx\r\n", hdr->descriptor__ptr->size, (long unsigned int) x);
    while (iter < endWord) {
        ind = iter/GGGGC_BITS_PER_WORD;
        rem = iter%GGGGC_BITS_PER_WORD;
        mask = 1L << rem;
        //printf("freebits before using mask %lx\r\n", pool->freeBits[ind]);
        pool->freeBits[ind] = pool->freeBits[ind]|mask;
        //printf("freebits after using mask %lx\r\n", pool->freeBits[ind]);
        iter++;
    }
}

void * ggggc_findFree(ggc_size_t size)
{
    ggc_size_t index = (ggggc_curPool->firstFree)/GGGGC_BITS_PER_WORD;
    ggc_size_t foundStart = 0;
    ggc_size_t streak = 0;
    ggc_size_t firstFound = 0;
    for (index = (ggggc_curPool->firstFree)/GGGGC_BITS_PER_WORD; index < GGGGC_FREEBIT_ARRAY_SIZE; index++) {
        //printf("index is %ld and it looks like %lx\r\n", index, ggggc_curPool->freeBits[index]);
        ggc_size_t bitIter = 1;
        ggc_size_t bitCount = 0;
        for (bitCount = 0; bitCount < GGGGC_BITS_PER_WORD; bitCount++) {
            ggc_size_t checker= index*GGGGC_BITS_PER_WORD + bitCount;
            ggc_size_t *check = ggggc_curPool->start + checker;
            if (check >= ggggc_curPool->end) {
                return NULL;
            }
            bitIter = 1 << bitCount;
            ggc_size_t current = index*GGGGC_BITS_PER_WORD + bitCount;
            if (!(ggggc_curPool->freeBits[index] & bitIter)) {
                if(!firstFound && index*GGGGC_BITS_PER_WORD + bitCount > ggggc_curPool->firstFree) {
                    ggggc_curPool->firstFree= index*GGGGC_BITS_PER_WORD + bitCount;
                }
                if (current == foundStart+streak) {
                    // If we're still on a streak keep it up
                    streak++;
                    if (streak == size) {
                        return (void *) (ggggc_curPool->start + foundStart);
                    }
                } else {
                    streak = 1;
                    foundStart = current;
                }
            }
        }
    }
    return NULL;
}

/* pools which are freely available */
static struct GGGGC_Pool *freePoolsHead, *freePoolsTail;

/* allocate and initialize a pool */
static struct GGGGC_Pool *newPool(int mustSucceed)
{
    struct GGGGC_Pool *ret;

    ret = NULL;

    /* try to reuse a pool */
    if (freePoolsHead) {
        if (freePoolsHead) {
            ret = freePoolsHead;
            freePoolsHead = freePoolsHead->next;
            if (!freePoolsHead) freePoolsTail = NULL;
        }
    }

    /* otherwise, allocate one */
    if (!ret) ret = (struct GGGGC_Pool *) allocPool(mustSucceed);

    if (!ret) return NULL;

    /* set it up */
    ret->next = NULL;
    ret->free = ret->start;
    ret->end = (ggc_size_t *) ((unsigned char *) ret + GGGGC_POOL_BYTES);
    int i = 0;
    for (i = 0; i < GGGGC_FREEBIT_ARRAY_SIZE; i++) {
        ret->freeBits[i] = GGGGC_MAX_WORD;
    }
    ret->currentFreeMax = GGGGC_FREEMAX_INIT;
    return ret;
}

/* heuristically expand a generation if it has too many survivors */
void ggggc_expandGeneration(struct GGGGC_Pool *pool)
{
    ggc_size_t space, survivors, poolCt;

    if (!pool) return;

    /* first figure out how much space was used */
    space = 0;
    survivors = 0;
    poolCt = 0;
    while (1) {
        space += pool->end - pool->start;
        survivors += pool->survivors;
        pool->survivors = 0;
        poolCt++;
        if (!pool->next) break;
        pool = pool->next;
    }

    /* now decide if it's too much */
    if (survivors > space/2) {
        /* allocate more */
        ggc_size_t i;
        for (i = 0; i < poolCt; i++) {
            pool->next = newPool(0);
            pool = pool->next;
            if (!pool) break;
        }
    }
}

/* free a generation (used when a thread exits) */
void ggggc_freeGeneration(struct GGGGC_Pool *pool)
{
    if (!pool) return;
    if (freePoolsHead) {
        freePoolsTail->next = pool;
    } else {
        freePoolsHead = pool;
    }
    while (pool->next) pool = pool->next;
    freePoolsTail = pool;
}

/* Function when allocating an object to zero out all
   it's internal space past the header */
void ggggc_zero_object(struct GGGGC_Header *hdr)
{
    ggc_size_t size = hdr->descriptor__ptr->size - GGGGC_WORD_SIZEOF(*hdr);
    ggc_size_t i = 0;
    ggc_size_t * iter = ((ggc_size_t *) hdr) + 1;
    //printf("Zeroing object at %lx starting at %lx\r\n", (long unsigned int) hdr, (long unsigned int) iter);
    while (i < size) {
        iter[0] = 0;
        //printf("For object %lx wiping %lx\r\n", (long unsigned int) hdr, (long unsigned int) iter);
        iter = iter + 1;
        i++;
    }
    return;
}

/* allocate an object */
void *ggggc_malloc(struct GGGGC_Descriptor *descriptor)
{
    /* Yield at allocation, if it decides to collect we have more space! */
    ggggc_yield();
    void* userPtr = NULL;
    struct GGGGC_Header header;
    extern ggc_size_t ggggc_poolCount;
    extern int ggggc_forceCollect;
    header.descriptor__ptr = descriptor;
    /* Check if curPool is set... if not we probably have no pools yet...
       and if we do have pools already we're in trouble cuz we lost our pointers
       to them so... EEP. */
    if (!ggggc_curPool) {
        ggggc_poolCount = 1;
        ggggc_forceCollect = 0;
        ggggc_curPool = ggggc_poolList = newPool(1);
    }
    /* Check if there are any free objects if there are try to find a suitable one 
       Max word is used to denote when the pool has been swept recently so that
       we know that we don't know the max size it can currently take. */
    if ((ggggc_curPool->currentFreeMax != GGGGC_FREEMAX_INIT) && (ggggc_curPool->currentFreeMax == GGGGC_MAX_WORD || ggggc_curPool->currentFreeMax > descriptor->size )) {
        userPtr = ggggc_findFree(descriptor->size);
        if (!userPtr) {
            ggggc_curPool->currentFreeMax = descriptor->size;
        }
    }

    /* If there are no suitable free objects allocate at the end of the pool */
    if (!userPtr) {
        ggc_size_t size = descriptor->size;
        if (ggggc_curPool->free + size >= ggggc_curPool->end) {
            /* If the object too big for our current pool get a new one */
            /* This should be changed to iterating through the pools later
               to check if there is a pool with enough space */
            if (ggggc_curPool->next) {
                // if we're not on last pool let's go to the next pool before maknig new one
                ggggc_curPool = ggggc_curPool->next;
                //printf("recurisvely calling malloc...\r\n");
                return ggggc_malloc(descriptor);
            } else {
            }
            struct GGGGC_Pool *temp = newPool(1);
            // Force a collection when we need to allocate a new pool.
            ggggc_forceCollect = 1;
            ggggc_poolCount++;
            ggggc_curPool->next = temp;
            ggggc_curPool = temp;
        }
        userPtr = (ggggc_curPool->free);
        ggggc_curPool->free += size;
    } else {
        //printf("Found free object %lx\r\n", (long unsigned int) userPtr);
        ((struct GGGGC_Header *) userPtr)[0] = header;
    }

    //printf("User ptr allocated at: %lx\r\n", (long unsigned int) userPtr);
    ((struct GGGGC_Header *) userPtr)[0] = header;
    ggggc_useFree(userPtr);
    ggggc_zero_object((struct GGGGC_Header*) userPtr);
    return userPtr;
}

struct GGGGC_Array {
    struct GGGGC_Header header;
    ggc_size_t length;
};

/* allocate a pointer array (size is in words) */
void *ggggc_mallocPointerArray(ggc_size_t sz)
{
    struct GGGGC_Descriptor *descriptor = ggggc_allocateDescriptorPA(sz + 1 + sizeof(struct GGGGC_Header)/sizeof(ggc_size_t));
    struct GGGGC_Array *ret = (struct GGGGC_Array *) ggggc_malloc(descriptor);
    ret->length = sz;
    return ret;
}

/* allocate a data array */
void *ggggc_mallocDataArray(ggc_size_t nmemb, ggc_size_t size)
{
    ggc_size_t sz = ((nmemb*size)+sizeof(ggc_size_t)-1)/sizeof(ggc_size_t);
    struct GGGGC_Descriptor *descriptor = ggggc_allocateDescriptorDA(sz + 1 + sizeof(struct GGGGC_Header)/sizeof(ggc_size_t));
    struct GGGGC_Array *ret = (struct GGGGC_Array *) ggggc_malloc(descriptor);
    ret->length = nmemb;
    return ret;
}

/* allocate a descriptor-descriptor for a descriptor of the given size */
struct GGGGC_Descriptor *ggggc_allocateDescriptorDescriptor(ggc_size_t size)
{
    struct GGGGC_Descriptor tmpDescriptor, *ret;
    ggc_size_t ddSize;

    /* need one description bit for every word in the object */
    ddSize = GGGGC_WORD_SIZEOF(struct GGGGC_Descriptor) + GGGGC_DESCRIPTOR_WORDS_REQ(size);

    /* check if we already have a descriptor */
    if (ggggc_descriptorDescriptors[size])
        return ggggc_descriptorDescriptors[size];

    /* otherwise, need to allocate one. First lock the space */
    if (ggggc_descriptorDescriptors[size]) {
        return ggggc_descriptorDescriptors[size];
    }

    /* now make a temporary descriptor to describe the descriptor descriptor */
    tmpDescriptor.header.descriptor__ptr = NULL;
    tmpDescriptor.size = ddSize;
    tmpDescriptor.pointers[0] = GGGGC_DESCRIPTOR_DESCRIPTION;

    /* allocate the descriptor descriptor */
    ret = (struct GGGGC_Descriptor *) ggggc_malloc(&tmpDescriptor);

    /* make it correct */
    ret->size = size;
    ret->pointers[0] = GGGGC_DESCRIPTOR_DESCRIPTION;

    /* put it in the list */
    ggggc_descriptorDescriptors[size] = ret;
    GGC_PUSH_1(ggggc_descriptorDescriptors[size]);
    GGC_GLOBALIZE();

    /* and give it a proper descriptor */
    ret->header.descriptor__ptr = ggggc_allocateDescriptorDescriptor(ddSize);
    return ret;
}

/* allocate a descriptor for an object of the given size in words with the
 * given pointer layout */
struct GGGGC_Descriptor *ggggc_allocateDescriptor(ggc_size_t size, ggc_size_t pointers)
{
    ggc_size_t pointersA[1];
    pointersA[0] = pointers;
    return ggggc_allocateDescriptorL(size, pointersA);
}

/* descriptor allocator when more than one word is required to describe the
 * pointers */
struct GGGGC_Descriptor *ggggc_allocateDescriptorL(ggc_size_t size, const ggc_size_t *pointers)
{
    struct GGGGC_Descriptor *dd, *ret;
    ggc_size_t dPWords, dSize;

    /* the size of the descriptor */
    if (pointers)
        dPWords = GGGGC_DESCRIPTOR_WORDS_REQ(size);
    else
        dPWords = 1;
    dSize = GGGGC_WORD_SIZEOF(struct GGGGC_Descriptor) + dPWords;

    /* get a descriptor-descriptor for the descriptor we're about to allocate */
    dd = ggggc_allocateDescriptorDescriptor(dSize);

    /* use that to allocate the descriptor */
    ret = (struct GGGGC_Descriptor *) ggggc_malloc(dd);
    ret->size = size;
    /* and set it up */
    if (pointers) {
        memcpy(ret->pointers, pointers, sizeof(ggc_size_t) * dPWords);
        ret->pointers[0] |= 1; /* first word is always the descriptor pointer */
    } else {
        ret->pointers[0] = 0;
    }

    return ret;
}

/* descriptor allocator for pointer arrays */
struct GGGGC_Descriptor *ggggc_allocateDescriptorPA(ggc_size_t size)
{
    ggc_size_t *pointers;
    ggc_size_t dPWords, i;

    /* fill our pointer-words with 1s */
    dPWords = GGGGC_DESCRIPTOR_WORDS_REQ(size);
    pointers = (ggc_size_t *) alloca(sizeof(ggc_size_t) * dPWords);
    for (i = 0; i < dPWords; i++) pointers[i] = (ggc_size_t) -1;

    /* get rid of non-pointers */
    pointers[0] &= ~0x4;

    /* and allocate */
    return ggggc_allocateDescriptorL(size, pointers);
}

/* descriptor allocator for data arrays */
struct GGGGC_Descriptor *ggggc_allocateDescriptorDA(ggc_size_t size)
{
    /* and allocate */
    return ggggc_allocateDescriptorL(size, NULL);
}

/* allocate a descriptor from a descriptor slot */
struct GGGGC_Descriptor *ggggc_allocateDescriptorSlot(struct GGGGC_DescriptorSlot *slot)
{
    if (slot->descriptor) return slot->descriptor;
    if (slot->descriptor) {
        return slot->descriptor;
    }

    slot->descriptor = ggggc_allocateDescriptor(slot->size, slot->pointers);

    /* make the slot descriptor a root */
    GGC_PUSH_1(slot->descriptor);
    GGC_GLOBALIZE();

    return slot->descriptor;
}

/* and a combined malloc/allocslot */
void *ggggc_mallocSlot(struct GGGGC_DescriptorSlot *slot)
{
    return ggggc_malloc(ggggc_allocateDescriptorSlot(slot));
}

#ifdef __cplusplus
}
#endif
