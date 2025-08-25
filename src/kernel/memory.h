/* Memory Management for Aquinas OS
 * 
 * Simple bump allocator implementation.
 * The allocator starts at a fixed address after the kernel and bumps
 * a pointer forward for each allocation. Very fast but no individual
 * deallocation - only bulk free.
 */

#ifndef MEMORY_H
#define MEMORY_H

/* Define size_t if not already defined */
#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

/* Define NULL if not already defined */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* Initialize the memory allocator.
 * Sets up the heap region starting after the kernel. */
void init_memory(void);

/* Allocate memory of given size.
 * Returns pointer to allocated memory or NULL if out of memory.
 * Memory is NOT zeroed. */
void* malloc(size_t size);

/* Allocate and zero memory.
 * Returns pointer to zeroed memory or NULL if out of memory. */
void* calloc(size_t count, size_t size);

/* Free is a no-op in bump allocator.
 * Provided for compatibility but does nothing. */
void free(void* ptr);

/* Reset the entire heap.
 * Frees all allocations at once by resetting the bump pointer. */
void reset_heap(void);

/* Get current heap usage statistics */
size_t get_heap_used(void);
size_t get_heap_size(void);
size_t get_heap_free(void);

/* Memory copy and set functions (since we don't have libc) */
void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);

#endif /* MEMORY_H */