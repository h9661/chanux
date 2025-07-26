/*
 * System Call Test Suite
 * 
 * Tests the system call interface with various system calls
 */

#include "../include/syscall.h"

/* Terminal functions */
void terminal_writestring(const char* data);
void terminal_putchar(char c);

/* Helper function to print test results */
static void print_test_result(const char* test_name, int passed) {
    terminal_writestring(passed ? "[PASS] " : "[FAIL] ");
    terminal_writestring(test_name);
    terminal_writestring("\n");
}

/* Helper function to convert integer to string */
static void int_to_string(int num, char* buffer) {
    int i = 0;
    int is_negative = 0;
    
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    /* Convert digits */
    do {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    } while (num > 0);
    
    /* Add negative sign if needed */
    if (is_negative) {
        buffer[i++] = '-';
    }
    
    /* Null terminate */
    buffer[i] = '\0';
    
    /* Reverse the string */
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
        start++;
        end--;
    }
}

/*
 * syscall_run_tests - Run system call tests
 */
void syscall_run_tests(void) {
    terminal_writestring("\nRunning system call tests...\n");
    terminal_writestring("============================\n\n");
    
    int tests_passed = 0;
    int tests_failed = 0;
    
    /* Test 1: Write system call */
    terminal_writestring("=== Write System Call Test ===\n");
    const char* test_string = "Hello from system call!\n";
    int bytes_written = write(1, test_string, 24);
    
    if (bytes_written == 24) {
        print_test_result("Write to stdout", 1);
        tests_passed++;
    } else {
        print_test_result("Write to stdout", 0);
        tests_failed++;
    }
    
    /* Test 2: GetPID system call */
    terminal_writestring("\n=== GetPID System Call Test ===\n");
    int pid = getpid();
    
    char pid_str[12];
    int_to_string(pid, pid_str);
    terminal_writestring("Process ID: ");
    terminal_writestring(pid_str);
    terminal_writestring("\n");
    
    if (pid > 0) {
        print_test_result("GetPID returns valid PID", 1);
        tests_passed++;
    } else {
        print_test_result("GetPID returns valid PID", 0);
        tests_failed++;
    }
    
    /* Test 3: Sleep system call */
    terminal_writestring("\n=== Sleep System Call Test ===\n");
    terminal_writestring("Sleeping for 500ms... ");
    
    /* Get current tick count before sleep */
    extern uint32_t timer_get_ticks(void);
    uint32_t start_ticks = timer_get_ticks();
    
    sleep(500);
    
    uint32_t end_ticks = timer_get_ticks();
    uint32_t elapsed_ms = (end_ticks - start_ticks) * 10;  /* 10ms per tick at 100Hz */
    
    terminal_writestring("done!\n");
    
    char elapsed_str[12];
    int_to_string(elapsed_ms, elapsed_str);
    terminal_writestring("Elapsed time: ");
    terminal_writestring(elapsed_str);
    terminal_writestring(" ms\n");
    
    /* Allow some tolerance (±50ms) */
    if (elapsed_ms >= 450 && elapsed_ms <= 550) {
        print_test_result("Sleep timing accuracy", 1);
        tests_passed++;
    } else {
        print_test_result("Sleep timing accuracy", 0);
        tests_failed++;
    }
    
    /* Test 4: Invalid file descriptor */
    terminal_writestring("\n=== Invalid FD Test ===\n");
    int result = write(99, "test", 4);
    
    if (result == -1) {
        print_test_result("Write with invalid FD returns error", 1);
        tests_passed++;
    } else {
        print_test_result("Write with invalid FD returns error", 0);
        tests_failed++;
    }
    
    /* Test 5: Write to stderr */
    terminal_writestring("\n=== Stderr Write Test ===\n");
    const char* err_msg = "Error message to stderr\n";
    result = write(2, err_msg, 24);
    
    if (result == 24) {
        print_test_result("Write to stderr", 1);
        tests_passed++;
    } else {
        print_test_result("Write to stderr", 0);
        tests_failed++;
    }
    
    /* Test 6: Read from invalid FD */
    terminal_writestring("\n=== Read Test ===\n");
    char buffer[10];
    result = read(99, buffer, 10);
    
    if (result == -1) {
        print_test_result("Read with invalid FD returns error", 1);
        tests_passed++;
    } else {
        print_test_result("Read with invalid FD returns error", 0);
        tests_failed++;
    }
    
    /* Test summary */
    terminal_writestring("\n=== System Call Test Summary ===\n");
    terminal_writestring("Total tests: ");
    char total_str[12];
    int_to_string(tests_passed + tests_failed, total_str);
    terminal_writestring(total_str);
    terminal_writestring("\nPassed: ");
    int_to_string(tests_passed, total_str);
    terminal_writestring(total_str);
    terminal_writestring("\nFailed: ");
    int_to_string(tests_failed, total_str);
    terminal_writestring(total_str);
    terminal_writestring("\n\n");
    
    if (tests_failed == 0) {
        terminal_writestring("All system call tests passed!\n");
    } else {
        terminal_writestring("Some system call tests failed!\n");
    }
}

/*
 * Test the exit system call (this will halt the system)
 */
void test_exit_syscall(void) {
    terminal_writestring("\n=== Exit System Call Test ===\n");
    terminal_writestring("Calling exit(42)...\n");
    
    /* This should print exit message and halt */
    exit(42);
    
    /* Should never reach here */
    terminal_writestring("ERROR: exit() returned!\n");
}