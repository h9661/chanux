/*
 * Physical Memory Manager Tests
 * 
 * This file contains unit tests for the PMM to verify correct functionality
 * of memory allocation, deallocation, and tracking.
 */

#include <stdint.h>
#include <stddef.h>
#include "../include/pmm.h"

/* Terminal output functions */
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

/* Test 1: Basic single page allocation and free */
static void test_single_page_alloc(void) {
    uint32_t page = pmm_alloc_page();
    int passed = (page != 0) && (page % PAGE_SIZE == 0);
    
    if (passed) {
        /* Verify page is marked as used */
        passed = pmm_test_page(page);
        
        /* Free the page */
        pmm_free_page(page);
        
        /* Verify page is marked as free */
        passed = passed && !pmm_test_page(page);
    }
    
    print_test_result("Single page allocation/free", passed);
}

/* Test 2: Multiple page allocation */
static void test_multiple_page_alloc(void) {
    uint32_t pages = pmm_alloc_pages(5);
    int passed = (pages != 0) && (pages % PAGE_SIZE == 0);
    
    if (passed) {
        /* Verify all pages are marked as used */
        for (int i = 0; i < 5; i++) {
            if (!pmm_test_page(pages + i * PAGE_SIZE)) {
                passed = 0;
                break;
            }
        }
        
        /* Free the pages */
        pmm_free_pages(pages, 5);
        
        /* Verify all pages are marked as free */
        for (int i = 0; i < 5 && passed; i++) {
            if (pmm_test_page(pages + i * PAGE_SIZE)) {
                passed = 0;
                break;
            }
        }
    }
    
    print_test_result("Multiple page allocation/free", passed);
}

/* Test 3: Allocation uniqueness */
static void test_allocation_uniqueness(void) {
    uint32_t page1 = pmm_alloc_page();
    uint32_t page2 = pmm_alloc_page();
    uint32_t page3 = pmm_alloc_page();
    
    int passed = (page1 != 0) && (page2 != 0) && (page3 != 0) &&
                 (page1 != page2) && (page2 != page3) && (page1 != page3);
    
    /* Clean up */
    if (page1) pmm_free_page(page1);
    if (page2) pmm_free_page(page2);
    if (page3) pmm_free_page(page3);
    
    print_test_result("Allocation uniqueness", passed);
}

/* Test 4: Free and reallocate */
static void test_free_and_reallocate(void) {
    /* Allocate a page */
    uint32_t page1 = pmm_alloc_page();
    int passed = (page1 != 0);
    
    if (passed) {
        /* Free it */
        pmm_free_page(page1);
        
        /* Allocate again - should get the same page */
        uint32_t page2 = pmm_alloc_page();
        passed = (page2 == page1);
        
        /* Clean up */
        pmm_free_page(page2);
    }
    
    print_test_result("Free and reallocate", passed);
}

/* Test 5: Memory statistics */
static void test_memory_stats(void) {
    struct pmm_stats stats1 = pmm_get_stats();
    uint32_t initial_free = stats1.free_pages;
    
    /* Allocate 10 pages */
    uint32_t pages[10];
    for (int i = 0; i < 10; i++) {
        pages[i] = pmm_alloc_page();
    }
    
    struct pmm_stats stats2 = pmm_get_stats();
    int passed = (stats2.free_pages == initial_free - 10) &&
                 (stats2.used_pages == stats1.used_pages + 10);
    
    /* Free the pages */
    for (int i = 0; i < 10; i++) {
        if (pages[i]) pmm_free_page(pages[i]);
    }
    
    struct pmm_stats stats3 = pmm_get_stats();
    passed = passed && (stats3.free_pages == initial_free) &&
             (stats3.used_pages == stats1.used_pages);
    
    print_test_result("Memory statistics tracking", passed);
}

/* Test 6: Contiguous allocation */
static void test_contiguous_allocation(void) {
    uint32_t base = pmm_alloc_pages(4);
    int passed = (base != 0);
    
    if (passed) {
        /* Verify pages are contiguous */
        for (int i = 1; i < 4; i++) {
            uint32_t expected = base + i * PAGE_SIZE;
            if (!pmm_test_page(expected)) {
                passed = 0;
                break;
            }
        }
        
        /* Free the pages */
        pmm_free_pages(base, 4);
    }
    
    print_test_result("Contiguous page allocation", passed);
}

/* Test 7: Region initialization */
static void test_region_init(void) {
    /* Find a free region */
    uint32_t region = pmm_alloc_pages(8);
    int passed = (region != 0);
    
    if (passed) {
        /* Free it first */
        pmm_free_pages(region, 8);
        
        /* Mark first 4 pages as used using init_region */
        pmm_init_region(region, 4 * PAGE_SIZE);
        
        /* Verify first 4 are used and last 4 are free */
        for (int i = 0; i < 4; i++) {
            if (!pmm_test_page(region + i * PAGE_SIZE)) {
                passed = 0;
                break;
            }
        }
        for (int i = 4; i < 8 && passed; i++) {
            if (pmm_test_page(region + i * PAGE_SIZE)) {
                passed = 0;
                break;
            }
        }
        
        /* Clean up */
        pmm_deinit_region(region, 8 * PAGE_SIZE);
    }
    
    print_test_result("Region initialization", passed);
}

/* Run all PMM tests */
void pmm_run_tests(void) {
    terminal_writestring("\nRunning PMM unit tests...\n");
    terminal_writestring("========================\n");
    
    test_single_page_alloc();
    test_multiple_page_alloc();
    test_allocation_uniqueness();
    test_free_and_reallocate();
    test_memory_stats();
    test_contiguous_allocation();
    test_region_init();
    
    terminal_writestring("\nTest Results: ");
    terminal_write_dec(tests_passed);
    terminal_writestring(" passed, ");
    terminal_write_dec(tests_failed);
    terminal_writestring(" failed\n");
    
    if (tests_failed == 0) {
        terminal_writestring("All tests passed!\n");
    }
}