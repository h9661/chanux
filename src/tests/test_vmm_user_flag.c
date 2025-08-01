/*
 * Test program to verify vmm_map_page_secure USER flag handling
 */

#include <stdint.h>
#include <stdbool.h>
#include "kernel/vmm.h"
#include "kernel/memory_protection.h"

extern void terminal_writestring(const char* data);
extern void terminal_write_hex(uint32_t value);

void test_vmm_user_flag(void) {
    terminal_writestring("\n=== Testing VMM USER Flag Handling ===\n");
    
    page_directory_t* pd = vmm_get_current_directory();
    
    /* Test 1: Map a user page without USER flag - should auto-add it */
    terminal_writestring("\nTest 1: Mapping user page (0x10000000) without USER flag...\n");
    vmm_map_page_secure(pd, 0x10000000, 0x300000, PAGE_PRESENT | PAGE_WRITABLE);
    
    /* Check if it's user accessible */
    bool is_user = vmm_is_user_accessible(pd, 0x10000000);
    terminal_writestring("Result: Page is ");
    terminal_writestring(is_user ? "USER accessible (PASS)\n" : "NOT user accessible (FAIL)\n");
    
    /* Test 2: Check page directory entry */
    uint32_t dir_index = PAGE_DIR_INDEX(0x10000000);
    uint32_t dir_entry = pd->entries[dir_index];
    terminal_writestring("Page directory entry: 0x");
    terminal_write_hex(dir_entry);
    terminal_writestring(" - USER flag ");
    terminal_writestring((dir_entry & PAGE_USER) ? "SET (PASS)\n" : "NOT SET (FAIL)\n");
    
    /* Test 3: Check page table entry */
    page_table_entry_t* pte = vmm_get_page_entry(pd, 0x10000000);
    if (pte && (*pte & PAGE_PRESENT)) {
        terminal_writestring("Page table entry: 0x");
        terminal_write_hex(*pte);
        terminal_writestring(" - USER flag ");
        terminal_writestring((*pte & PAGE_USER) ? "SET (PASS)\n" : "NOT SET (FAIL)\n");
    } else {
        terminal_writestring("Page table entry not found or not present!\n");
    }
    
    terminal_writestring("\n=== End of VMM USER Flag Test ===\n");
}