/****************************************************************************
 *
 * Copyright 2016 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
/****************************************************************************
 * mm/mm_heap/mm_free.c
 *
 *   Copyright (C) 2007, 2009, 2013-2014 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>

#include <assert.h>
#include <debug.h>

#include <tinyara/mm/mm.h>

#ifdef CONFIG_DEBUG_MM_HEAPINFO
#include  <tinyara/sched.h>
#endif
/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mm_free
 *
 * Description:
 *   Returns a chunk of memory to the list of free nodes,  merging with
 *   adjacent free chunks if possible.
 *
 ****************************************************************************/
void mm_free(FAR struct mm_heap_s *heap, FAR void *mem)
{
	FAR struct mm_freenode_s *node;
	FAR struct mm_freenode_s *prev;
	FAR struct mm_freenode_s *next;
#ifdef CONFIG_DEBUG_MM_HEAPINFO
	struct mm_allocnode_s *alloc_node;
#endif

	mvdbg("Freeing %p\n", mem);

	/* Protect against attempts to free a NULL reference */

	if (!mem) {
		return;
	}

	/* We need to hold the MM semaphore while we muck with the
	 * nodelist.
	 */

	mm_takesemaphore(heap);

	/* Map the memory chunk into a free node */

	node = (FAR struct mm_freenode_s *)((char *)mem - SIZEOF_MM_ALLOCNODE);
#ifdef CONFIG_DEBUG_MM_HEAPINFO
	alloc_node = (struct mm_allocnode_s *)node;

	if ((alloc_node->preceding & MM_ALLOC_BIT) != 0) {
		heapinfo_subtract_size(alloc_node->pid, alloc_node->size);
		heapinfo_update_total_size(heap, ((-1) * alloc_node->size));
	}
#endif
	node->preceding &= ~MM_ALLOC_BIT;

	/* Check if the following node is free and, if so, merge it */

	next = (FAR struct mm_freenode_s *)((char *)node + node->size);
	if ((next->preceding & MM_ALLOC_BIT) == 0) {
		FAR struct mm_allocnode_s *andbeyond;

		/* Get the node following the next node (which will
		 * become the new next node). We know that we can never
		 * index past the tail chunk because it is always allocated.
		 */

		andbeyond = (FAR struct mm_allocnode_s *)((char *)next + next->size);

		/* Remove the next node.  There must be a predecessor,
		 * but there may not be a successor node.
		 */

		DEBUGASSERT(next->blink);
		next->blink->flink = next->flink;
		if (next->flink) {
			next->flink->blink = next->blink;
		}

		/* Then merge the two chunks */

		node->size          += next->size;
		andbeyond->preceding = node->size | (andbeyond->preceding & MM_ALLOC_BIT);
		next                 = (FAR struct mm_freenode_s *)andbeyond;
	}

	/* Check if the preceding node is also free and, if so, merge
	 * it with this node
	 */

	prev = (FAR struct mm_freenode_s *)((char *)node - node->preceding);
	if ((prev->preceding & MM_ALLOC_BIT) == 0) {
		/* Remove the node.  There must be a predecessor, but there may
		 * not be a successor node.
		 */

		DEBUGASSERT(prev->blink);
		prev->blink->flink = prev->flink;
		if (prev->flink) {
			prev->flink->blink = prev->blink;
		}

		/* Then merge the two chunks */

		prev->size     += node->size;
		next->preceding = prev->size | (next->preceding & MM_ALLOC_BIT);
		node            = prev;
	}

	/* Add the merged node to the nodelist */

	mm_addfreechunk(heap, node);
	mm_givesemaphore(heap);
}
