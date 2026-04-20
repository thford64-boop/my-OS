/*
 * BIG-DOS — commands.c
 * Implementations for every entry in the command table.
 * Each function signature: int cmd_foo(int argc, char **argv)
 * Return 0 on success, non-zero on error (POSIX convention).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

/* ── helpers ───────────────────────────────────────────────────────── */
static void perr(const char *ctx) {
    fprintf(stderr, "bigdos: %s: %s\n", ctx, strerror(errno));
}

/* ── ls ────────────────────────────────────────────────────────────── */
int cmd_ls(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : ".";
    DIR *d = opendir(path);
    if (!d) { perr("ls"); return 1; }
    struct dirent *e;
    while ((e = readdir(d)))
        if (e->d_name[0] != '.')
            printf("%s\n", e->d_name);
    closedir(d);
    return 0;
}

/* ── cd ────────────────────────────────────────────────────────────── */
int cmd_cd(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : getenv("HOME");
    if (!path) path = "/";
    if (chdir(path) != 0) { perr("cd"); return 1; }
    return 0;
}

/* ── pwd ───────────────────────────────────────────────────────────── */
int cmd_pwd(int argc, char **argv) {
    (void)argc; (void)argv;
    char buf[4096];
    if (!getcwd(buf, sizeof(buf))) { perr("pwd"); return 1; }
    printf("%s\n", buf);
    return 0;
}

/* ── cat ───────────────────────────────────────────────────────────── */
int cmd_cat(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: cat FILE...\n"); return 1; }
    char buf[4096];
    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "r");
        if (!f) { perr(argv[i]); continue; }
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            fwrite(buf, 1, n, stdout);
        fclose(f);
    }
    return 0;
}

/* ── echo ──────────────────────────────────────────────────────────── */
int cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) putchar(' ');
        fputs(argv[i], stdout);
    }
    putchar('\n');
    return 0;
}

/* ── mkdir ─────────────────────────────────────────────────────────── */
int cmd_mkdir(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: mkdir DIR\n"); return 1; }
    if (mkdir(argv[1], 0755) != 0) { perr("mkdir"); return 1; }
    return 0;
}

/* ── rm ────────────────────────────────────────────────────────────── */
int cmd_rm(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: rm [-r] FILE\n"); return 1; }
    int recursive = 0, start = 1;
    if (strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "-rf") == 0) {
        recursive = 1; start = 2;
    }
    for (int i = start; i < argc; i++) {
        if (recursive) {
            /* exec system rm -rf for simplicity in this shell */
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "rm -rf %s", argv[i]);
            system(cmd);
        } else {
            if (remove(argv[i]) != 0) perr(argv[i]);
        }
    }
    return 0;
}

/* ── cp ────────────────────────────────────────────────────────────── */
int cmd_cp(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: cp SRC DST\n"); return 1; }
    FILE *in  = fopen(argv[1], "rb");
    FILE *out = fopen(argv[2], "wb");
    if (!in)  { perr(argv[1]); return 1; }
    if (!out) { perr(argv[2]); fclose(in); return 1; }
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);
    fclose(in); fclose(out);
    return 0;
}

/* ── mv ────────────────────────────────────────────────────────────── */
int cmd_mv(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: mv SRC DST\n"); return 1; }
    if (rename(argv[1], argv[2]) != 0) { perr("mv"); return 1; }
    return 0;
}

/* ── touch ─────────────────────────────────────────────────────────── */
int cmd_touch(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: touch FILE\n"); return 1; }
    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "a");
        if (!f) { perr(argv[i]); } else { fclose(f); }
    }
    return 0;
}

/* ── clear ─────────────────────────────────────────────────────────── */
int cmd_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("\033[2J\033[H");  /* ANSI: erase screen, move cursor home */
    return 0;
}

/* ── uname ─────────────────────────────────────────────────────────── */
int cmd_uname(int argc, char **argv) {
    int all = (argc > 1 && strcmp(argv[1], "-a") == 0);
    if (all)
        printf("BIG-DOS 1.0 bigdos-host bigdos-kernel #1 SMP x86_64 GNU/Linux\n");
    else
        printf("BIG-DOS\n");
    return 0;
}

/* ── whoami ────────────────────────────────────────────────────────── */
int cmd_whoami(int argc, char **argv) {
    (void)argc; (void)argv;
    char *user = getenv("USER");
    printf("%s\n", user ? user : "root");
    return 0;
}

/* ── date ──────────────────────────────────────────────────────────── */
int cmd_date(int argc, char **argv) {
    (void)argc; (void)argv;
    time_t t = time(NULL);
    printf("%s", ctime(&t));
    return 0;
}

/* ── uptime (stub) ─────────────────────────────────────────────────── */
int cmd_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    FILE *f = fopen("/proc/uptime", "r");
    double up = 0;
    if (f) { fscanf(f, "%lf", &up); fclose(f); }
    int h = (int)(up / 3600), m = (int)((up - h * 3600) / 60);
    printf("up %d:%02d\n", h, m);
    return 0;
}

/* ── ps (stub) ─────────────────────────────────────────────────────── */
int cmd_ps(int argc, char **argv) {
    (void)argc; (void)argv;
    system("ps");
    return 0;
}

/* ── kill ──────────────────────────────────────────────────────────── */
int cmd_kill(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: kill PID\n"); return 1; }
    int pid = atoi(argv[1]);
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "kill %d", pid);
    return system(cmd);
}

/* ── free ──────────────────────────────────────────────────────────── */
int cmd_free(int argc, char **argv) {
    (void)argc; (void)argv;
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) { perr("free"); return 1; }
    char line[256];
    printf("%-20s %10s\n", "", "kB");
    while (fgets(line, sizeof(line), f))
        if (strncmp(line, "Mem", 3) == 0 || strncmp(line, "Swap", 4) == 0)
            fputs(line, stdout);
    fclose(f);
    return 0;
}

/* ── df (stub via system) ──────────────────────────────────────────── */
int cmd_df(int argc, char **argv) {
    (void)argc; (void)argv;
    system("df -h");
    return 0;
}

/* ── find ──────────────────────────────────────────────────────────── */
int cmd_find(int argc, char **argv) {
    char cmd[1024] = "find";
    for (int i = 1; i < argc; i++) {
        strcat(cmd, " ");
        strncat(cmd, argv[i], sizeof(cmd) - strlen(cmd) - 1);
    }
    return system(cmd);
}

/* ── grep ──────────────────────────────────────────────────────────── */
int cmd_grep(int argc, char **argv) {
    char cmd[1024] = "grep";
    for (int i = 1; i < argc; i++) {
        strcat(cmd, " ");
        strncat(cmd, argv[i], sizeof(cmd) - strlen(cmd) - 1);
    }
    return system(cmd);
}

/* ── wc ────────────────────────────────────────────────────────────── */
int cmd_wc(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: wc FILE\n"); return 1; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perr(argv[1]); return 1; }
    long lines = 0, words = 0, bytes = 0;
    int c, in_word = 0;
    while ((c = fgetc(f)) != EOF) {
        bytes++;
        if (c == '\n') lines++;
        if (isspace(c)) { in_word = 0; }
        else { if (!in_word) { words++; in_word = 1; } }
    }
    fclose(f);
    printf("%6ld %6ld %6ld %s\n", lines, words, bytes, argv[1]);
    return 0;
}

/* ── head ──────────────────────────────────────────────────────────── */
int cmd_head(int argc, char **argv) {
    int  n    = 10;
    const char *path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) { n = atoi(argv[++i]); }
        else path = argv[i];
    }
    if (!path) { fprintf(stderr, "usage: head [-n N] FILE\n"); return 1; }
    FILE *f = fopen(path, "r");
    if (!f) { perr(path); return 1; }
    char line[4096];
    for (int i = 0; i < n && fgets(line, sizeof(line), f); i++)
        fputs(line, stdout);
    fclose(f);
    return 0;
}

/* ── tail ──────────────────────────────────────────────────────────── */
int cmd_tail(int argc, char **argv) {
    int n = 10; const char *path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) { n = atoi(argv[++i]); }
        else path = argv[i];
    }
    if (!path) { fprintf(stderr, "usage: tail [-n N] FILE\n"); return 1; }
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "tail -n %d %s", n, path);
    return system(cmd);
}

/* ── sort ──────────────────────────────────────────────────────────── */
int cmd_sort(int argc, char **argv) {
    char cmd[512] = "sort";
    for (int i = 1; i < argc; i++) { strcat(cmd, " "); strncat(cmd, argv[i], 64); }
    return system(cmd);
}

/* ── uniq ──────────────────────────────────────────────────────────── */
int cmd_uniq(int argc, char **argv) {
    char cmd[512] = "uniq";
    for (int i = 1; i < argc; i++) { strcat(cmd, " "); strncat(cmd, argv[i], 64); }
    return system(cmd);
}

/* ── env ───────────────────────────────────────────────────────────── */
extern char **environ;
int cmd_env(int argc, char **argv) {
    (void)argc; (void)argv;
    for (char **e = environ; *e; e++) printf("%s\n", *e);
    return 0;
}

/* ── export ────────────────────────────────────────────────────────── */
int cmd_export(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: export NAME=VALUE\n"); return 1; }
    /* putenv takes ownership of the string — strdup for safety */
    char *kv = strdup(argv[1]);
    if (putenv(kv) != 0) { perr("export"); return 1; }
    return 0;
}

/* ── alias (in-process store) ──────────────────────────────────────── */
#define MAX_ALIASES 64
static char *alias_keys[MAX_ALIASES];
static char *alias_vals[MAX_ALIASES];
static int   alias_count = 0;

int cmd_alias(int argc, char **argv) {
    if (argc < 2) {
        for (int i = 0; i < alias_count; i++)
            printf("alias %s='%s'\n", alias_keys[i], alias_vals[i]);
        return 0;
    }
    char *eq = strchr(argv[1], '=');
    if (!eq) { fprintf(stderr, "alias: expected NAME=VALUE\n"); return 1; }
    *eq = '\0';
    if (alias_count < MAX_ALIASES) {
        alias_keys[alias_count] = strdup(argv[1]);
        alias_vals[alias_count] = strdup(eq + 1);
        alias_count++;
    }
    return 0;
}

/* ── man ───────────────────────────────────────────────────────────── */
int cmd_man(int argc, char **argv) {
    if (argc < 2) { printf("usage: man COMMAND\n"); return 1; }
    /* Try system man first, fall back to our built-in help */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "man %s 2>/dev/null", argv[1]);
    if (system(cmd) != 0) {
        /* look up in our table */
        extern const int NUM_COMMANDS; /* visible via translation unit */
        printf("BIG-DOS built-in — no external man page for '%s'.\n", argv[1]);
        printf("Try 'help' to list all commands.\n");
    }
    return 0;
}

/* ── which ─────────────────────────────────────────────────────────── */
int cmd_which(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: which CMD\n"); return 1; }
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "which %s", argv[1]);
    return system(cmd);
}

/* ── chmod ─────────────────────────────────────────────────────────── */
int cmd_chmod(int argc, char **argv) {
    char cmd[512] = "chmod";
    for (int i = 1; i < argc; i++) { strcat(cmd, " "); strncat(cmd, argv[i], 64); }
    return system(cmd);
}

/* ── chown ─────────────────────────────────────────────────────────── */
int cmd_chown(int argc, char **argv) {
    char cmd[512] = "chown";
    for (int i = 1; i < argc; i++) { strcat(cmd, " "); strncat(cmd, argv[i], 64); }
    return system(cmd);
}

/* ── ln ────────────────────────────────────────────────────────────── */
int cmd_ln(int argc, char **argv) {
    char cmd[512] = "ln";
    for (int i = 1; i < argc; i++) { strcat(cmd, " "); strncat(cmd, argv[i], 64); }
    return system(cmd);
}

/* ── tar ───────────────────────────────────────────────────────────── */
int cmd_tar(int argc, char **argv) {
    char cmd[512] = "tar";
    for (int i = 1; i < argc; i++) { strcat(cmd, " "); strncat(cmd, argv[i], 64); }
    return system(cmd);
}

/* ── gzip ──────────────────────────────────────────────────────────── */
int cmd_gzip(int argc, char **argv) {
    char cmd[512] = "gzip";
    for (int i = 1; i < argc; i++) { strcat(cmd, " "); strncat(cmd, argv[i], 64); }
    return system(cmd);
}

/* ── wget ──────────────────────────────────────────────────────────── */
int cmd_wget(int argc, char **argv) {
    char cmd[512] = "wget";
    for (int i = 1; i < argc; i++) { strcat(cmd, " "); strncat(cmd, argv[i], 64); }
    return system(cmd);
}

/* ── curl ──────────────────────────────────────────────────────────── */
int cmd_curl(int argc, char **argv) {
    char cmd[512] = "curl";
    for (int i = 1; i < argc; i++) { strcat(cmd, " "); strncat(cmd, argv[i], 64); }
    return system(cmd);
}

/* ── ping ──────────────────────────────────────────────────────────── */
int cmd_ping(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: ping HOST\n"); return 1; }
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ping -c 4 %s", argv[1]);
    return system(cmd);
}

/* ── ifconfig ──────────────────────────────────────────────────────── */
int cmd_ifconfig(int argc, char **argv) {
    (void)argc; (void)argv;
    return system("ip addr show 2>/dev/null || ifconfig");
}

/* ── reboot ────────────────────────────────────────────────────────── */
int cmd_reboot(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("BIG-DOS: rebooting...\n");
    sync();
    return system("reboot");
}

/* ── shutdown ──────────────────────────────────────────────────────── */
int cmd_shutdown(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("BIG-DOS: shutting down...\n");
    sync();
    return system("poweroff");
}

/* ── startgui: forward-declared, implemented in gui.c ─────────────── */
int start_gui(void);   /* defined in gui.c */
int cmd_startgui(int argc, char **argv) {
    (void)argc; (void)argv;
    return start_gui();
}

/* ── pk: forward-declared, implemented in pkgmgr.c ────────────────── */
int pk_dispatch(int argc, char **argv);  /* defined in pkgmgr.c */
int cmd_pk(int argc, char **argv) {
    return pk_dispatch(argc, argv);
}
