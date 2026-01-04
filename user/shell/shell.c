/**
 * =============================================================================
 * Chanux OS - Shell Program
 * =============================================================================
 * A minimal interactive shell providing basic filesystem navigation
 * and file viewing capabilities.
 *
 * Commands:
 *   help         - Show available commands
 *   echo [args]  - Print arguments
 *   cat <file>   - Display file contents
 *   ls [dir]     - List directory contents
 *   pwd          - Print working directory
 *   cd <dir>     - Change directory
 *   clear        - Clear screen
 *   exit         - Exit shell
 * =============================================================================
 */

#include "../include/syscall.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

#define MAX_LINE        256     /* Maximum command line length */
#define MAX_ARGS        16      /* Maximum number of arguments */
#define PROMPT          "chanux> "

/* VGA text mode constants for clear command */
#define VGA_CLEAR_CHAR  ' '
#define VGA_WIDTH       80
#define VGA_HEIGHT      25

/* =============================================================================
 * Forward Declarations
 * =============================================================================
 */

/* Command handlers */
static int cmd_help(int argc, char** argv);
static int cmd_echo(int argc, char** argv);
static int cmd_cat(int argc, char** argv);
static int cmd_ls(int argc, char** argv);
static int cmd_pwd(int argc, char** argv);
static int cmd_cd(int argc, char** argv);
static int cmd_clear(int argc, char** argv);
static int cmd_exit(int argc, char** argv);

/* =============================================================================
 * String Utilities
 * =============================================================================
 */

/**
 * Compare two strings.
 * Returns 0 if equal, non-zero otherwise.
 */
static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

/* =============================================================================
 * Command Table
 * =============================================================================
 */

typedef struct {
    const char* name;
    const char* description;
    int (*handler)(int argc, char** argv);
} command_t;

static const command_t commands[] = {
    { "help",  "Show available commands",    cmd_help  },
    { "echo",  "Print arguments",            cmd_echo  },
    { "cat",   "Display file contents",      cmd_cat   },
    { "ls",    "List directory contents",    cmd_ls    },
    { "pwd",   "Print working directory",    cmd_pwd   },
    { "cd",    "Change directory",           cmd_cd    },
    { "clear", "Clear screen",               cmd_clear },
    { "exit",  "Exit shell",                 cmd_exit  },
    { NULL,    NULL,                         NULL      }
};

/* =============================================================================
 * Input Handling
 * =============================================================================
 */

/**
 * Read a character from stdin with blocking.
 * Polls keyboard until a character is available.
 */
static char getchar_blocking(void) {
    char c;
    while (1) {
        ssize_t n = read(0, &c, 1);
        if (n == 1) {
            return c;
        }
        /* No input yet, yield and try again */
        yield();
    }
}

/**
 * Read a line from stdin with echo.
 * Returns the number of characters read (excluding newline).
 * Supports backspace for editing.
 */
static int readline(char* buf, int max_len) {
    int pos = 0;

    while (pos < max_len - 1) {
        char c = getchar_blocking();

        if (c == '\n' || c == '\r') {
            /* Enter pressed - end of line */
            write(1, "\n", 1);
            break;
        } else if (c == 8 || c == 127) {
            /* Backspace */
            if (pos > 0) {
                pos--;
                /* Echo: backspace, space, backspace */
                write(1, "\b \b", 3);
            }
        } else if (c >= 32 && c < 127) {
            /* Printable character */
            buf[pos++] = c;
            /* Echo the character */
            write(1, &c, 1);
        }
        /* Ignore other control characters */
    }

    buf[pos] = '\0';
    return pos;
}

/**
 * Parse a command line into argc/argv.
 * Returns argc (number of arguments).
 * Modifies the line buffer in place.
 */
static int parse_line(char* line, char** argv, int max_args) {
    int argc = 0;
    char* p = line;

    while (*p && argc < max_args - 1) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (!*p) break;

        /* Start of argument */
        argv[argc++] = p;

        /* Find end of argument */
        while (*p && *p != ' ' && *p != '\t') {
            p++;
        }
        if (*p) {
            *p++ = '\0';  /* Null-terminate argument */
        }
    }

    argv[argc] = NULL;
    return argc;
}

/* =============================================================================
 * Command Handlers
 * =============================================================================
 */

/**
 * help - Display available commands
 */
static int cmd_help(int argc, char** argv) {
    (void)argc; (void)argv;

    puts("Chanux Shell - Available Commands:\n\n");

    for (int i = 0; commands[i].name != NULL; i++) {
        puts("  ");
        puts(commands[i].name);
        /* Pad to column 10 */
        size_t len = strlen(commands[i].name);
        for (size_t j = len; j < 8; j++) {
            write(1, " ", 1);
        }
        puts(" - ");
        puts(commands[i].description);
        puts("\n");
    }

    return 0;
}

/**
 * echo - Print arguments
 */
static int cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            write(1, " ", 1);
        }
        puts(argv[i]);
    }
    puts("\n");
    return 0;
}

/**
 * cat - Display file contents
 */
static int cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        puts("Usage: cat <file>\n");
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        puts("cat: cannot open '");
        puts(argv[1]);
        puts("': No such file or directory\n");
        return 1;
    }

    char buf[256];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(1, buf, n);
    }

    close(fd);
    return 0;
}

/**
 * ls - List directory contents
 */
static int cmd_ls(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : ".";

    /* Open directory */
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        puts("ls: cannot access '");
        puts(path);
        puts("': No such file or directory\n");
        return 1;
    }

    /* Check if it's a directory */
    stat_t st;
    if (fstat(fd, &st) < 0 || st.st_mode != S_IFDIR) {
        /* It's a file, just print its name */
        puts(path);
        puts("\n");
        close(fd);
        return 0;
    }

    /* Read directory entries */
    dirent_t entry;
    int index = 0;

    while (readdir_r(fd, &entry, index) == 0) {
        /* Skip . and .. for cleaner output */
        if (strcmp(entry.d_name, ".") != 0 && strcmp(entry.d_name, "..") != 0) {
            puts(entry.d_name);

            /* Add / suffix for directories */
            if (entry.d_type == S_IFDIR) {
                puts("/");
            }
            puts("  ");
        }
        index++;
    }

    puts("\n");
    close(fd);
    return 0;
}

/**
 * pwd - Print working directory
 */
static int cmd_pwd(int argc, char** argv) {
    (void)argc; (void)argv;

    char buf[256];
    if (getcwd(buf, sizeof(buf)) != NULL) {
        puts(buf);
        puts("\n");
        return 0;
    } else {
        puts("pwd: error getting current directory\n");
        return 1;
    }
}

/**
 * cd - Change directory
 */
static int cmd_cd(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "/";

    if (chdir(path) < 0) {
        puts("cd: cannot change directory to '");
        puts(path);
        puts("'\n");
        return 1;
    }

    return 0;
}

/**
 * clear - Clear screen
 */
static int cmd_clear(int argc, char** argv) {
    (void)argc; (void)argv;

    /* Print ANSI escape sequence to clear screen and move cursor to home */
    /* For VGA text mode, we'll just print lots of newlines */
    for (int i = 0; i < VGA_HEIGHT; i++) {
        puts("\n");
    }

    return 0;
}

/**
 * exit - Exit shell
 */
static int cmd_exit(int argc, char** argv) {
    (void)argc; (void)argv;

    puts("Goodbye!\n");
    exit(0);

    return 0;  /* Never reached */
}

/* =============================================================================
 * Command Execution
 * =============================================================================
 */

/**
 * Execute a command by name.
 */
static int execute_command(int argc, char** argv) {
    if (argc == 0) {
        return 0;
    }

    /* Look up command in table */
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            return commands[i].handler(argc, argv);
        }
    }

    /* Command not found */
    puts("Unknown command: ");
    puts(argv[0]);
    puts("\nType 'help' for available commands.\n");
    return 1;
}

/* =============================================================================
 * Shell Main Loop
 * =============================================================================
 */

/**
 * Shell entry point.
 */
void shell_main(void) {
    char line[MAX_LINE];
    char* argv[MAX_ARGS];

    /* Print welcome banner */
    puts("\n");
    puts("=======================================\n");
    puts("   Welcome to Chanux Shell\n");
    puts("   Type 'help' for available commands\n");
    puts("=======================================\n");
    puts("\n");

    /* Main shell loop */
    while (1) {
        /* Print prompt */
        puts(PROMPT);

        /* Read command line */
        int len = readline(line, MAX_LINE);
        if (len == 0) {
            continue;  /* Empty line */
        }

        /* Parse command */
        int argc = parse_line(line, argv, MAX_ARGS);
        if (argc == 0) {
            continue;  /* Only whitespace */
        }

        /* Execute command */
        execute_command(argc, argv);
    }
}
