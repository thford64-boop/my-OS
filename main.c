/*
 * BIG-DOS — main.c
 * Modular command table architecture.
 * Add new commands by: (1) writing cmd_foo() in commands.c,
 * (2) adding ONE line to the COMMANDS[] array below. That's it.
 * The dispatcher uses bsearch() so the table must stay sorted.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── forward declarations ──────────────────────────────────────────── */
int cmd_ls(int argc, char **argv);
int cmd_cd(int argc, char **argv);
int cmd_pwd(int argc, char **argv);
int cmd_cat(int argc, char **argv);
int cmd_echo(int argc, char **argv);
int cmd_mkdir(int argc, char **argv);
int cmd_rm(int argc, char **argv);
int cmd_cp(int argc, char **argv);
int cmd_mv(int argc, char **argv);
int cmd_touch(int argc, char **argv);
int cmd_clear(int argc, char **argv);
int cmd_help(int argc, char **argv);
int cmd_exit(int argc, char **argv);
int cmd_uname(int argc, char **argv);
int cmd_whoami(int argc, char **argv);
int cmd_date(int argc, char **argv);
int cmd_uptime(int argc, char **argv);
int cmd_ps(int argc, char **argv);
int cmd_kill(int argc, char **argv);
int cmd_free(int argc, char **argv);
int cmd_df(int argc, char **argv);
int cmd_find(int argc, char **argv);
int cmd_grep(int argc, char **argv);
int cmd_wc(int argc, char **argv);
int cmd_head(int argc, char **argv);
int cmd_tail(int argc, char **argv);
int cmd_sort(int argc, char **argv);
int cmd_uniq(int argc, char **argv);
int cmd_env(int argc, char **argv);
int cmd_export(int argc, char **argv);
int cmd_alias(int argc, char **argv);
int cmd_history(int argc, char **argv);
int cmd_man(int argc, char **argv);
int cmd_which(int argc, char **argv);
int cmd_chmod(int argc, char **argv);
int cmd_chown(int argc, char **argv);
int cmd_ln(int argc, char **argv);
int cmd_tar(int argc, char **argv);
int cmd_gzip(int argc, char **argv);
int cmd_wget(int argc, char **argv);
int cmd_curl(int argc, char **argv);
int cmd_ping(int argc, char **argv);
int cmd_ifconfig(int argc, char **argv);
int cmd_reboot(int argc, char **argv);
int cmd_shutdown(int argc, char **argv);
/* package manager */
int cmd_pk(int argc, char **argv);
/* gui launcher */
int cmd_startgui(int argc, char **argv);

/* ── command struct ────────────────────────────────────────────────── */
typedef struct {
    const char *name;                       /* command string, e.g. "ls"   */
    int        (*fn)(int, char **);         /* function pointer             */
    const char *synopsis;                   /* one-liner for 'help'         */
} Command;

/* ── command table (MUST stay alphabetically sorted for bsearch) ──── */
static const Command COMMANDS[] = {
    { "alias",    cmd_alias,    "alias NAME=VALUE — define a command alias"    },
    { "cat",      cmd_cat,      "cat FILE ... — concatenate and print files"   },
    { "cd",       cmd_cd,       "cd DIR — change working directory"            },
    { "chmod",    cmd_chmod,    "chmod MODE FILE — change file permissions"     },
    { "chown",    cmd_chown,    "chown USER:GRP FILE — change ownership"        },
    { "clear",    cmd_clear,    "clear — clear the terminal screen"            },
    { "cp",       cmd_cp,       "cp SRC DST — copy file"                       },
    { "curl",     cmd_curl,     "curl URL — transfer data from a URL"          },
    { "date",     cmd_date,     "date — print current date and time"           },
    { "df",       cmd_df,       "df — disk free space"                         },
    { "echo",     cmd_echo,     "echo TEXT — print text to stdout"             },
    { "env",      cmd_env,      "env — list environment variables"             },
    { "exit",     cmd_exit,     "exit — leave BIG-DOS"                         },
    { "export",   cmd_export,   "export NAME=VALUE — set environment variable" },
    { "find",     cmd_find,     "find PATH -name PATTERN — find files"         },
    { "free",     cmd_free,     "free — display memory usage"                  },
    { "grep",     cmd_grep,     "grep PATTERN FILE — search file for pattern"  },
    { "gzip",     cmd_gzip,     "gzip FILE — compress a file"                  },
    { "head",     cmd_head,     "head [-n N] FILE — print first N lines"       },
    { "help",     cmd_help,     "help — list all commands"                     },
    { "history",  cmd_history,  "history — show command history"               },
    { "ifconfig", cmd_ifconfig, "ifconfig — network interface configuration"   },
    { "kill",     cmd_kill,     "kill PID — terminate a process"               },
    { "ln",       cmd_ln,       "ln [-s] SRC DST — create link"                },
    { "ls",       cmd_ls,       "ls [DIR] — list directory contents"           },
    { "man",      cmd_man,      "man CMD — show manual for command"            },
    { "mkdir",    cmd_mkdir,    "mkdir DIR — create directory"                 },
    { "mv",       cmd_mv,       "mv SRC DST — move or rename file"             },
    { "ping",     cmd_ping,     "ping HOST — test network connectivity"        },
    { "pk",       cmd_pk,       "pk install|remove|list PKG — package mgr"    },
    { "ps",       cmd_ps,       "ps — list running processes"                  },
    { "pwd",      cmd_pwd,      "pwd — print working directory"                },
    { "reboot",   cmd_reboot,   "reboot — restart the system"                  },
    { "rm",       cmd_rm,       "rm [-r] FILE — remove file or directory"      },
    { "shutdown",  cmd_shutdown, "shutdown — power off the system"             },
    { "sort",     cmd_sort,     "sort FILE — sort lines of a file"             },
    { "startgui", cmd_startgui, "startgui — launch the BIG-DOS windowed GUI"  },
    { "tail",     cmd_tail,     "tail [-n N] FILE — print last N lines"        },
    { "tar",      cmd_tar,      "tar [czxf] ARCHIVE FILES — archive utility"  },
    { "touch",    cmd_touch,    "touch FILE — create empty file / update mtime"},
    { "uname",    cmd_uname,    "uname [-a] — print system information"        },
    { "uniq",     cmd_uniq,     "uniq FILE — remove duplicate adjacent lines"  },
    { "wc",       cmd_wc,       "wc FILE — word/line/byte count"               },
    { "wget",     cmd_wget,     "wget URL — download file from URL"            },
    { "which",    cmd_which,    "which CMD — locate a command in PATH"         },
    { "whoami",   cmd_whoami,   "whoami — print current user name"             },
};

#define NUM_COMMANDS  (int)(sizeof(COMMANDS) / sizeof(COMMANDS[0]))
#define MAX_ARGS       64
#define MAX_INPUT    1024
#define HISTORY_SIZE  100

/* ── global command history ────────────────────────────────────────── */
static char *g_history[HISTORY_SIZE];
static int   g_hist_count = 0;

static void history_push(const char *line) {
    if (g_hist_count < HISTORY_SIZE)
        g_history[g_hist_count++] = strdup(line);
}

/* ── bsearch comparator ────────────────────────────────────────────── */
static int cmd_cmp(const void *key, const void *elem) {
    return strcmp((const char *)key, ((const Command *)elem)->name);
}

/* ── tokenizer ─────────────────────────────────────────────────────── */
static int tokenize(char *line, char **argv, int max) {
    int argc = 0;
    char *p = line;
    while (*p && argc < max - 1) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) *p++ = '\0';
    }
    argv[argc] = NULL;
    return argc;
}

/* ── dispatcher ────────────────────────────────────────────────────── */
static int dispatch(int argc, char **argv) {
    if (argc == 0) return 0;
    const Command *c = bsearch(argv[0], COMMANDS, NUM_COMMANDS,
                               sizeof(Command), cmd_cmp);
    if (!c) {
        fprintf(stderr, "bigdos: command not found: %s\n", argv[0]);
        return 127;
    }
    return c->fn(argc, argv);
}

/* ── built-in: help ────────────────────────────────────────────────── */
int cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("\n  BIG-DOS v1.0 — available commands\n\n");
    for (int i = 0; i < NUM_COMMANDS; i++)
        printf("  %-12s  %s\n", COMMANDS[i].name, COMMANDS[i].synopsis);
    putchar('\n');
    return 0;
}

/* ── built-in: history ─────────────────────────────────────────────── */
int cmd_history(int argc, char **argv) {
    (void)argc; (void)argv;
    for (int i = 0; i < g_hist_count; i++)
        printf("  %3d  %s\n", i + 1, g_history[i]);
    return 0;
}

/* ── built-in: exit ────────────────────────────────────────────────── */
int cmd_exit(int argc, char **argv) {
    int code = (argc > 1) ? atoi(argv[1]) : 0;
    printf("Goodbye from BIG-DOS.\n");
    exit(code);
}

/* ── REPL ──────────────────────────────────────────────────────────── */
int main(int argc, char **argv) {
    (void)argc; (void)argv;

    char   line[MAX_INPUT];
    char  *args[MAX_ARGS];
    int    n;

    printf("\n  Welcome to BIG-DOS 1.0\n");
    printf("  Type 'help' for a command list, 'startgui' for GUI mode.\n\n");

    while (1) {
        printf("bigdos> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            break;                          /* EOF / Ctrl-D */

        /* strip trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

        if (line[0] == '\0') continue;

        history_push(line);

        char copy[MAX_INPUT];
        strncpy(copy, line, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';

        n = tokenize(copy, args, MAX_ARGS);
        dispatch(n, args);
    }

    return 0;
}
