/*
 * Heap Allocator Unit Tests
 * 
 * This file contains comprehensive tests for the heap allocator
 * to verify correct allocation, deallocation, and memory management.
 */

#include <stdint.h>
#include <stddef.h>
#include "../include/heap.h"
#include "../include/string.h"

/* External functions */
extern void terminal_writestring(const char* data);
extern void terminal_write_hex(uint32_t value);
extern void terminal_write_dec(uint32_t value);

/* Test result tracking */
static int tests_passed = 0;
static int tests_failed = 0;

/* Helper function to print test result */
static void print_test_result(const char* test_name, int passed) {
    terminal_writestring("[");
    if (passed) {
        terminal_writestring("PASS");
        tests_passed++;
    } else {
        terminal_writestring("FAIL");
        tests_failed++;
    }
    terminal_writestring("] ");
    terminal_writestring(test_name);
    terminal_writestring("\n");
}

/* Test 1: Basic allocation and free */
static void test_basic_alloc(void) {
    void* ptr1 = malloc(100);
    void* ptr2 = malloc(200);
    
    int passed = (ptr1 != NULL) && (ptr2 != NULL) && (ptr1 != ptr2);
    
    if (passed) {
        /* Write test patterns */
        memset(ptr1, 0xAA, 100);
        memset(ptr2, 0xBB, 200);
        
        /* Verify patterns */
        uint8_t* p1 = (uint8_t*)ptr1;
        uint8_t* p2 = (uint8_t*)ptr2;
        
        for (int i = 0; i < 100 && passed; i++) {
            if (p1[i] != 0xAA) passed = 0;
        }
        for (int i = 0; i < 200 && passed; i++) {
            if (p2[i] != 0xBB) passed = 0;
        }
    }
    
    /* Free memory */
    free(ptr1);
    free(ptr2);
    
    print_test_result("Basic allocation and free", passed);
}

/* Test 2: Zero allocation */
static void test_zero_alloc(void) {
    void* ptr = malloc(0);
    int passed = (ptr == NULL);
    print_test_result("Zero size allocation", passed);
}

/* Test 3: Calloc functionality */
static void test_calloc(void) {
    size_t num = 10;
    size_t size = sizeof(uint32_t);
    uint32_t* ptr = (uint32_t*)calloc(num, size);
    
    int passed = (ptr != NULL);
    
    if (passed) {
        /* Verify all bytes are zero */
        for (size_t i = 0; i < num && passed; i++) {
            if (ptr[i] != 0) {
                passed = 0;
            }
        }
    }
    
    free(ptr);
    print_test_result("Calloc zero initialization", passed);
}

/* Test 4: Realloc functionality */
static void test_realloc(void) {
    /* Initial allocation */
    char* ptr = (char*)malloc(50);
    int passed = (ptr != NULL);
    
    if (passed) {
        /* Fill with pattern */
        for (int i = 0; i < 50; i++) {
            ptr[i] = (char)i;
        }
        
        /* Realloc larger */
        ptr = (char*)realloc(ptr, 100);
        passed = (ptr != NULL);
        
        if (passed) {
            /* Verify original data preserved */
            for (int i = 0; i < 50 && passed; i++) {
                if (ptr[i] != (char)i) {
                    passed = 0;
                }
            }
        }
        
        /* Realloc smaller */
        if (passed) {
            ptr = (char*)realloc(ptr, 25);
            passed = (ptr != NULL);
            
            if (passed) {
                /* Verify data still preserved */
                for (int i = 0; i < 25 && passed; i++) {
                    if (ptr[i] != (char)i) {
                        passed = 0;
                    }
                }
            }
        }
    }
    
    free(ptr);
    print_test_result("Realloc functionality", passed);
}

/* Test 5: Many small allocations */
static void test_many_small_allocs(void) {
    void* ptrs[100];
    int passed = 1;
    
    /* Allocate many small blocks */
    for (int i = 0; i < 100; i++) {
        ptrs[i] = malloc(16 + i);
        if (!ptrs[i]) {
            passed = 0;
            break;
        }
        /* Write pattern */
        memset(ptrs[i], i & 0xFF, 16 + i);
    }
    
    /* Verify patterns */
    if (passed) {
        for (int i = 0; i < 100 && passed; i++) {
            uint8_t* p = (uint8_t*)ptrs[i];
            for (int j = 0; j < 16 + i; j++) {
                if (p[j] != (i & 0xFF)) {
                    passed = 0;
                    break;
                }
            }
        }
    }
    
    /* Free all */
    for (int i = 0; i < 100; i++) {
        if (ptrs[i]) {
            free(ptrs[i]);
        }
    }
    
    print_test_result("Many small allocations", passed);
}

/* Test 6: Large allocation */
static void test_large_alloc(void) {
    size_t large_size = 64 * 1024;  /* 64KB */
    void* ptr = malloc(large_size);
    int passed = (ptr != NULL);
    
    if (passed) {
        /* Write pattern at beginning and end */
        uint8_t* p = (uint8_t*)ptr;
        memset(p, 0xCC, 1024);
        memset(p + large_size - 1024, 0xDD, 1024);
        
        /* Verify patterns */
        for (int i = 0; i < 1024 && passed; i++) {
            if (p[i] != 0xCC) passed = 0;
            if (p[large_size - 1024 + i] != 0xDD) passed = 0;
        }
    }
    
    free(ptr);
    print_test_result("Large allocation (64KB)", passed);
}

/* Test 7: Fragmentation and coalescing */
static void test_fragmentation(void) {
    void* ptr1 = malloc(100);
    void* ptr2 = malloc(100);
    void* ptr3 = malloc(100);
    
    int passed = (ptr1 != NULL) && (ptr2 != NULL) && (ptr3 != NULL);
    
    if (passed) {
        /* Free middle block */
        free(ptr2);
        
        /* Allocate slightly smaller - should reuse freed block */
        void* ptr4 = malloc(80);
        passed = (ptr4 != NULL);
        
        /* Free all and allocate large block - should coalesce */
        free(ptr1);
        free(ptr3);
        free(ptr4);
        
        void* ptr5 = malloc(250);
        passed = passed && (ptr5 != NULL);
        
        free(ptr5);
    }
    
    print_test_result("Fragmentation and coalescing", passed);
}

/* Test 8: Heap statistics */
static void test_heap_stats(void) {
    heap_stats_t stats1 = heap_get_stats();
    
    /* Allocate some memory */
    void* ptr1 = malloc(1000);
    void* ptr2 = malloc(2000);
    
    heap_stats_t stats2 = heap_get_stats();
    
    int passed = (ptr1 != NULL) && (ptr2 != NULL);
    
    if (passed) {
        /* Check that used size increased */
        passed = (stats2.used_size > stats1.used_size) &&
                (stats2.free_size < stats1.free_size);
    }
    
    /* Free memory */
    free(ptr1);
    free(ptr2);
    
    heap_stats_t stats3 = heap_get_stats();
    
    /* Used size should be back close to original */
    if (passed) {
        /* Allow some overhead for block headers */
        size_t overhead = 1024;  /* Reasonable overhead allowance */
        passed = (stats3.used_size <= stats1.used_size + overhead);
    }
    
    print_test_result("Heap statistics tracking", passed);
}

/* Test 9: Alignment */
static void test_alignment(void) {
    int passed = 1;
    
    /* Test various sizes for alignment */
    size_t sizes[] = {1, 7, 8, 9, 15, 16, 17, 31, 32, 33};
    
    for (int i = 0; i < 10 && passed; i++) {
        void* ptr = malloc(sizes[i]);
        if (!ptr) {
            passed = 0;
            break;
        }
        
        /* Check alignment */
        if ((uintptr_t)ptr % HEAP_ALIGNMENT != 0) {
            passed = 0;
        }
        
        free(ptr);
    }
    
    print_test_result("Memory alignment", passed);
}

/* Test 10: Heap integrity */
static void test_heap_integrity(void) {
    /* Allocate and free in various patterns */
    void* ptrs[10];
    
    for (int i = 0; i < 10; i++) {
        ptrs[i] = malloc(100 + i * 10);
    }
    
    /* Free every other block */
    for (int i = 0; i < 10; i += 2) {
        free(ptrs[i]);
    }
    
    /* Check integrity */
    int passed = heap_check_integrity();
    
    /* Free remaining blocks */
    for (int i = 1; i < 10; i += 2) {
        free(ptrs[i]);
    }
    
    /* Check integrity again */
    passed = passed && heap_check_integrity();
    
    print_test_result("Heap integrity check", passed);
}

/* Run all heap tests */
void heap_run_tests(void) {
    terminal_writestring("\nRunning heap allocator tests...\n");
    terminal_writestring("==============================\n");
    
    test_basic_alloc();
    test_zero_alloc();
    test_calloc();
    test_realloc();
    test_many_small_allocs();
    test_large_alloc();
    test_fragmentation();
    test_heap_stats();
    test_alignment();
    test_heap_integrity();
    
    terminal_writestring("\nTest Results: ");
    terminal_write_dec(tests_passed);
    terminal_writestring(" passed, ");
    terminal_write_dec(tests_failed);
    terminal_writestring(" failed\n");
    
    if (tests_failed == 0) {
        terminal_writestring("All tests passed!\n");
    }
    
    /* Print final heap statistics */
    terminal_writestring("\nFinal heap statistics:\n");
    heap_stats_t stats = heap_get_stats();
    terminal_writestring("Total size: ");
    terminal_write_dec(stats.total_size / 1024);
    terminal_writestring(" KB\n");
    terminal_writestring("Used: ");
    terminal_write_dec(stats.used_size);
    terminal_writestring(" bytes\n");
    terminal_writestring("Free: ");
    terminal_write_dec(stats.free_size);
    terminal_writestring(" bytes\n");
    terminal_writestring("Active allocations: ");
    terminal_write_dec(stats.num_allocations);
    terminal_writestring("\n");
}