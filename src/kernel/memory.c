/* Memory Management Implementation
 *
 * DESIGN
 * ------
 * This is a bump allocator (linear allocator) - the simplest form of
 * memory allocation. It maintains a single pointer that "bumps" forward
 * with each allocation. 
 * 
 * Advantages:
 * - Extremely fast allocation (just increment a pointer)
 * - No fragmentation within allocated region
 * - Minimal memory overhead (just one pointer)
 * - Perfect for phase-based allocation patterns
 * 
 * Limitations:
 * - Cannot free individual allocations
 * - Memory can only be reclaimed all at once
 * - No reuse of freed memory until full reset
 * 
 * Memory Layout:
 * The heap starts at 3MB (0x300000) and extends to 4MB (0x400000),
 * giving us 1MB of heap space. This is well above our kernel and stack.
 */

#include "memory.h"
#include "serial.h"

/* Heap configuration.
 * We place the heap at 3MB, well clear of our kernel (loaded at 1MB)
 * and stack (at 2MB growing down). This gives us a clean 1MB heap. */
#define HEAP_START 0x300000  /* 3MB mark */
#define HEAP_END   0x400000  /* 4MB mark */
#define HEAP_SIZE  (HEAP_END - HEAP_START)  /* 1MB heap */

/* Alignment requirement for allocations.
 * Aligning to 4 bytes ensures proper access for 32-bit values. */
#define ALIGN_SIZE 4
#define ALIGN_MASK (ALIGN_SIZE - 1)

/* Align a size up to the nearest multiple of ALIGN_SIZE */
#define ALIGN_UP(size) (((size) + ALIGN_MASK) & ~ALIGN_MASK)

/* Current position of the bump allocator */
static unsigned char* heap_current = (unsigned char*)HEAP_START;

/* Statistics tracking */
static size_t total_allocated = 0;
static size_t allocation_count = 0;

/* Initialize the memory allocator */
void init_memory(void) {
    heap_current = (unsigned char*)HEAP_START;
    total_allocated = 0;
    allocation_count = 0;
    
    serial_write_string("Memory allocator initialized: ");
    serial_write_int(HEAP_SIZE / 1024);
    serial_write_string("KB heap at 0x");
    serial_write_hex(HEAP_START);
    serial_write_string("\n");
}

/* Allocate memory using bump allocation */
void* malloc(size_t size) {
    void* result;
    size_t aligned_size;
    
    /* Handle zero size */
    if (size == 0) {
        return NULL;
    }
    
    /* Align the size for proper memory alignment */
    aligned_size = ALIGN_UP(size);
    
    /* Check if we have enough space */
    if ((size_t)(heap_current + aligned_size) > HEAP_END) {
        /* Out of memory */
        serial_write_string("ERROR: Out of heap memory! Requested: ");
        serial_write_int(size);
        serial_write_string(" bytes, available: ");
        serial_write_int(HEAP_END - (size_t)heap_current);
        serial_write_string(" bytes\n");
        return NULL;
    }
    
    /* Allocate by returning current position and bumping pointer */
    result = heap_current;
    heap_current += aligned_size;
    total_allocated += aligned_size;
    allocation_count++;
    
    return result;
}

/* Allocate and zero memory */
void* calloc(size_t count, size_t size) {
    size_t total_size;
    void* ptr;
    
    /* Check for multiplication overflow */
    total_size = count * size;
    if (count != 0 && total_size / count != size) {
        /* Overflow occurred */
        return NULL;
    }
    
    /* Allocate memory */
    ptr = malloc(total_size);
    if (ptr != NULL) {
        /* Zero the allocated memory */
        memset(ptr, 0, total_size);
    }
    
    return ptr;
}

/* Free is a no-op in bump allocator.
 * We provide this function for compatibility but it does nothing.
 * To actually free memory, use reset_heap() to free everything at once. */
void free(void* ptr) {
    /* Bump allocator doesn't support individual frees */
    /* This is intentionally empty */
    (void)ptr;  /* Suppress unused parameter warning */
}

/* Reset the entire heap */
void reset_heap(void) {
    heap_current = (unsigned char*)HEAP_START;
    
    serial_write_string("Heap reset: freed ");
    serial_write_int(total_allocated);
    serial_write_string(" bytes from ");
    serial_write_int(allocation_count);
    serial_write_string(" allocations\n");
    
    total_allocated = 0;
    allocation_count = 0;
}

/* Get current heap usage */
size_t get_heap_used(void) {
    return (size_t)(heap_current - HEAP_START);
}

/* Get total heap size */
size_t get_heap_size(void) {
    return HEAP_SIZE;
}

/* Get free heap space */
size_t get_heap_free(void) {
    return HEAP_END - (size_t)heap_current;
}

/* Memory copy implementation.
 * Copies n bytes from src to dest. Returns dest.
 * Handles overlapping regions correctly. */
void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    size_t i;
    
    /* Check for overlap - if dest is after src and regions overlap,
     * we need to copy backwards to avoid corruption */
    if (d > s && d < s + n) {
        /* Copy backwards */
        for (i = n; i > 0; i--) {
            d[i-1] = s[i-1];
        }
    } else {
        /* Copy forwards (normal case) */
        for (i = 0; i < n; i++) {
            d[i] = s[i];
        }
    }
    
    return dest;
}

/* Memory set implementation.
 * Sets n bytes of memory at s to the value c. Returns s. */
void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    size_t i;
    
    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    
    return s;
}

/* Memory compare implementation.
 * Compares n bytes of s1 and s2.
 * Returns 0 if equal, <0 if s1<s2, >0 if s1>s2. */
int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    size_t i;
    
    for (i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    
    return 0;
}