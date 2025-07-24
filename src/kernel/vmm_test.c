/*
 * Virtual Memory Manager Tests
 * 
 * This file contains unit tests for the VMM to verify correct functionality
 * of virtual memory mapping, page allocation, and address translation.
 */

#include <stdint.h>
#include <stddef.h>
#include "../include/vmm.h"
#include "../include/pmm.h"

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

/* Test 1: Basic page mapping */
static void test_page_mapping(void) {
    page_directory_t* dir = vmm_get_current_directory();
    uint32_t test_virt = 0x10000000;  /* 256MB */
    uint32_t test_phys = pmm_alloc_page();
    
    int passed = (test_phys != 0);
    
    if (passed) {
        /* Map virtual to physical */
        vmm_map_page(dir, test_virt, test_phys, PAGE_PRESENT | PAGE_WRITABLE);
        
        /* Verify mapping */
        passed = vmm_is_page_mapped(dir, test_virt) &&
                (vmm_get_physical_addr(dir, test_virt) == test_phys);
        
        /* Clean up */
        vmm_unmap_page(dir, test_virt);
        pmm_free_page(test_phys);
    }
    
    print_test_result("Basic page mapping", passed);
}

/* Test 2: Page unmapping */
static void test_page_unmapping(void) {
    page_directory_t* dir = vmm_get_current_directory();
    uint32_t test_virt = 0x10001000;
    uint32_t test_phys = pmm_alloc_page();
    
    int passed = (test_phys != 0);
    
    if (passed) {
        /* Map and then unmap */
        vmm_map_page(dir, test_virt, test_phys, PAGE_PRESENT | PAGE_WRITABLE);
        vmm_unmap_page(dir, test_virt);
        
        /* Verify unmapped */
        passed = !vmm_is_page_mapped(dir, test_virt);
        
        /* Clean up */
        pmm_free_page(test_phys);
    }
    
    print_test_result("Page unmapping", passed);
}

/* Test 3: Range mapping */
static void test_range_mapping(void) {
    page_directory_t* dir = vmm_get_current_directory();
    uint32_t test_virt = 0x10002000;
    uint32_t test_phys = pmm_alloc_pages(4);
    
    int passed = (test_phys != 0);
    
    if (passed) {
        /* Map 4 pages */
        vmm_map_range(dir, test_virt, test_phys, 4 * PAGE_SIZE, 
                      PAGE_PRESENT | PAGE_WRITABLE);
        
        /* Verify all pages mapped correctly */
        for (int i = 0; i < 4 && passed; i++) {
            uint32_t virt = test_virt + i * PAGE_SIZE;
            uint32_t expected_phys = test_phys + i * PAGE_SIZE;
            
            passed = vmm_is_page_mapped(dir, virt) &&
                    (vmm_get_physical_addr(dir, virt) == expected_phys);
        }
        
        /* Clean up */
        vmm_unmap_range(dir, test_virt, 4 * PAGE_SIZE);
        pmm_free_pages(test_phys, 4);
    }
    
    print_test_result("Range mapping", passed);
}

/* Test 4: Virtual page allocation */
static void test_virtual_alloc(void) {
    page_directory_t* dir = vmm_get_current_directory();
    uint32_t test_virt = 0x10006000;
    
    /* Allocate virtual page */
    uint32_t result = vmm_alloc_page(dir, test_virt, PAGE_PRESENT | PAGE_WRITABLE);
    int passed = (result == test_virt) && vmm_is_page_mapped(dir, test_virt);
    
    if (passed) {
        /* Get physical address */
        uint32_t phys = vmm_get_physical_addr(dir, test_virt);
        passed = (phys != 0) && (phys % PAGE_SIZE == 0);
        
        /* Clean up */
        vmm_free_page(dir, test_virt);
    }
    
    print_test_result("Virtual page allocation", passed);
}

/* Test 5: Multiple virtual page allocation */
static void test_virtual_alloc_multiple(void) {
    page_directory_t* dir = vmm_get_current_directory();
    uint32_t test_virt = 0x10007000;
    
    /* Allocate 3 virtual pages */
    uint32_t result = vmm_alloc_pages(dir, test_virt, 3, PAGE_PRESENT | PAGE_WRITABLE);
    int passed = (result == test_virt);
    
    if (passed) {
        /* Verify all pages mapped */
        for (int i = 0; i < 3 && passed; i++) {
            uint32_t virt = test_virt + i * PAGE_SIZE;
            passed = vmm_is_page_mapped(dir, virt);
        }
        
        /* Clean up */
        vmm_free_pages(dir, test_virt, 3);
    }
    
    print_test_result("Multiple virtual page allocation", passed);
}

/* Test 6: Page directory creation */
static void test_page_directory_creation(void) {
    /* Create new page directory */
    page_directory_t* new_dir = vmm_create_page_directory();
    int passed = (new_dir != NULL);
    
    if (passed) {
        /* Verify kernel space is mapped (first 4MB should be identity mapped) */
        passed = vmm_is_page_mapped(new_dir, 0x1000) &&
                (vmm_get_physical_addr(new_dir, 0x1000) == 0x1000);
        
        /* Clean up */
        vmm_free_page_directory(new_dir);
    }
    
    print_test_result("Page directory creation", passed);
}

/* Test 7: Address translation */
static void test_address_translation(void) {
    page_directory_t* dir = vmm_get_current_directory();
    uint32_t test_virt = 0x1000A234;  /* Address with offset */
    uint32_t test_phys = pmm_alloc_page();
    
    int passed = (test_phys != 0);
    
    if (passed) {
        /* Map page */
        vmm_map_page(dir, test_virt, test_phys, PAGE_PRESENT | PAGE_WRITABLE);
        
        /* Get physical address with offset */
        uint32_t translated = vmm_get_physical_addr(dir, test_virt);
        uint32_t expected = test_phys + (test_virt & 0xFFF);
        
        passed = (translated == expected);
        
        /* Clean up */
        vmm_unmap_page(dir, test_virt);
        pmm_free_page(test_phys);
    }
    
    print_test_result("Address translation with offset", passed);
}

/* Run all VMM tests */
void vmm_run_tests(void) {
    terminal_writestring("\nRunning VMM unit tests...\n");
    terminal_writestring("========================\n");
    
    test_page_mapping();
    test_page_unmapping();
    test_range_mapping();
    test_virtual_alloc();
    test_virtual_alloc_multiple();
    test_page_directory_creation();
    test_address_translation();
    
    terminal_writestring("\nTest Results: ");
    terminal_write_dec(tests_passed);
    terminal_writestring(" passed, ");
    terminal_write_dec(tests_failed);
    terminal_writestring(" failed\n");
    
    if (tests_failed == 0) {
        terminal_writestring("All tests passed!\n");
    }
}