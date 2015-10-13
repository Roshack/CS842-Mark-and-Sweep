/*
 * Header for internal GGGGC functions
 *
 * Copyright (c) 2014 Gregor Richards
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

#ifndef GGGGC_INTERNALS_H
#define GGGGC_INTERNALS_H 1

#include "ggggc/gc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* sweeeeeeeeep */
void ggggc_sweep();

/* Pass a pointer to an object to check if it's marked
   returns 1 if marked 0 if not */
long unsigned int ggggc_isMarked(void * x);

/* Given a pointer to a header returns that headers 
   descriptor__ptr after masking the possibly set mark bit */
void * ggggc_cleanMark(void *x);

/* Mark an individual object passed as a void pointer to its header. 
   Used for cleanliness */
void ggggc_markObject(void *x);

/* Unmark an individual object passed as a void pointer to its header */
void ggggc_unmarkObject(void *x);

/* Start the mark phase of mark and sweep */
void ggggc_mark();

/* Do actual marking recursion for non stack objects */
void ggggc_markHelper();

/* run a collection */
void ggggc_collect();

/* the pools are thread-local */
extern struct GGGGC_Pool *ggggc_poolList;

/* the current allocation pool */
extern struct GGGGC_Pool *ggggc_curPool;

/* descriptor descriptors */
extern struct GGGGC_Descriptor *ggggc_descriptorDescriptors[GGGGC_WORDS_PER_POOL/GGGGC_BITS_PER_WORD+sizeof(struct GGGGC_Descriptor)];

#ifdef __cplusplus
}
#endif

#endif
