/*
 * BIG-DOS — pkgmgr.c
 * Package manager that mimics the 'pacman -S' experience.
 *
 * Files:
 *   /etc/bigdos/repo.db      — remote repository index (NAME|URL|SIZE|DESC)
 *   /etc/bigdos/installed.db — locally installed packages (NAME|VERSION|DATE)
 *
 * Usage from the shell:
 *   pk install <name>    — download and install a package
 *   pk remove  <name>    — uninstall a package
 *   pk list              — list installed packages
 *   pk search  <query>   — search the remote repo
 *   pk update            — refresh repo.db
 *   pk upgrade           — upgrade all installed packages
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

/* ── config ────────────────────────────────────────────────────────── */
#define REPO_DIR      "/etc/bigdos"
#define REPO_DB       "/etc/bigdos/repo.db"
#define INSTALLED_DB  "/etc/bigdos/installed.db"
#define BIN_DIR       "/bin"
#define PKG_CACHE     "/tmp/bigdos-pkg-cache"

/* Upstream repo URL — replace with your real mirror */
#define REPO_URL      "https://raw.githubusercontent.com/bigdos/repo/main/repo.db"

/* ── record format helpers ─────────────────────────────────────────── */
typedef struct {
    char name[64];
    char version[32];
    char url[256];
    char desc[128];
    long size_kb;
} RepoPkg;

typedef struct {
    char name[64];
    char version[32];
    char install_date[32];
} InstalledPkg;

/* ── init: ensure directory/db files exist ─────────────────────────── */
static void pk_init(void) {
    mkdir(REPO_DIR, 0755);
    mkdir(PKG_CACHE, 0755);

    /* seed repo.db with a few example entries if empty */
    FILE *f = fopen(REPO_DB, "r");
    if (!f) {
        f = fopen(REPO_DB, "w");
        if (f) {
            /* FORMAT: name|version|url|size_kb|description */
            fprintf(f, "nano|8.0|https://example.com/pkgs/nano.tar.gz|220|Tiny terminal text editor\n");
            fprintf(f, "htop|3.3.0|https://example.com/pkgs/htop.tar.gz|180|Interactive process viewer\n");
            fprintf(f, "curl|8.5.0|https://example.com/pkgs/curl.tar.gz|400|Transfer data with URLs\n");
            fprintf(f, "wget|1.21.4|https://example.com/pkgs/wget.tar.gz|350|Network downloader\n");
            fprintf(f, "vim|9.1|https://example.com/pkgs/vim.tar.gz|1800|Vi IMproved text editor\n");
            fprintf(f, "python3|3.12.0|https://example.com/pkgs/python3.tar.gz|4200|Python 3 interpreter\n");
            fprintf(f, "git|2.44.0|https://example.com/pkgs/git.tar.gz|2600|Distributed version control\n");
            fprintf(f, "bash|5.2.26|https://example.com/pkgs/bash.tar.gz|880|GNU Bourne-Again SHell\n");
            fclose(f);
        }
    } else {
        fclose(f);
    }

    /* create installed.db if it doesn't exist */
    f = fopen(INSTALLED_DB, "a");
    if (f) fclose(f);
}

/* ── lookup a package in repo.db ───────────────────────────────────── */
static int repo_find(const char *name, RepoPkg *out) {
    FILE *f = fopen(REPO_DB, "r");
    if (!f) { fprintf(stderr, "pk: cannot open repo.db — run 'pk update'\n"); return 0; }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char n[64], v[32], u[256], d[128];
        long sz = 0;
        /* parse: name|version|url|size_kb|description */
        if (sscanf(line, "%63[^|]|%31[^|]|%255[^|]|%ld|%127[^\n]",
                   n, v, u, &sz, d) >= 4) {
            if (strcmp(n, name) == 0) {
                strncpy(out->name,    n, 64);
                strncpy(out->version, v, 32);
                strncpy(out->url,     u, 256);
                strncpy(out->desc,    d, 128);
                out->size_kb = sz;
                fclose(f);
                return 1;
            }
        }
    }
    fclose(f);
    return 0;
}

/* ── check if package is installed ────────────────────────────────── */
static int is_installed(const char *name) {
    FILE *f = fopen(INSTALLED_DB, "r");
    if (!f) return 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char n[64];
        sscanf(line, "%63[^|]", n);
        if (strcmp(n, name) == 0) { fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

/* ── write to installed.db ─────────────────────────────────────────── */
static void db_add(const char *name, const char *version) {
    FILE *f = fopen(INSTALLED_DB, "a");
    if (!f) return;
    time_t t = time(NULL);
    char date[32];
    strftime(date, sizeof(date), "%Y-%m-%d", localtime(&t));
    fprintf(f, "%s|%s|%s\n", name, version, date);
    fclose(f);
}

/* ── remove from installed.db ──────────────────────────────────────── */
static void db_remove(const char *name) {
    FILE *in  = fopen(INSTALLED_DB, "r");
    FILE *out = fopen("/tmp/installed.tmp", "w");
    if (!in || !out) { if(in) fclose(in); if(out) fclose(out); return; }
    char line[256];
    while (fgets(line, sizeof(line), in)) {
        char n[64]; sscanf(line, "%63[^|]", n);
        if (strcmp(n, name) != 0) fputs(line, out);
    }
    fclose(in); fclose(out);
    rename("/tmp/installed.tmp", INSTALLED_DB);
}

/* ── progress bar ──────────────────────────────────────────────────── */
static void progress_bar(const char *label, int pct) {
    int filled = pct / 5;   /* 20 chars = 100% */
    printf("\r  %s [", label);
    for (int i = 0; i < 20; i++)
        putchar(i < filled ? '#' : '-');
    printf("] %3d%%", pct);
    fflush(stdout);
}

/* ── pk install ─────────────────────────────────────────────────────  */
static int pk_install(const char *name) {
    pk_init();

    printf(":: Checking remote repository for '%s'...\n", name);

    if (is_installed(name)) {
        printf(":: %s is already installed.\n", name);
        return 0;
    }

    RepoPkg pkg;
    if (!repo_find(name, &pkg)) {
        fprintf(stderr, "error: target not found: %s\n", name);
        return 1;
    }

    printf(":: Package   : %s %s\n", pkg.name, pkg.version);
    printf(":: Size      : %ld KB\n", pkg.size_kb);
    printf(":: Desc      : %s\n", pkg.desc);
    printf(":: URL       : %s\n\n", pkg.url);
    printf(":: Proceed with installation? [Y/n] ");
    fflush(stdout);

    char ans[8];
    if (!fgets(ans, sizeof(ans), stdin)) return 1;
    if (ans[0] == 'n' || ans[0] == 'N') { printf("Aborted.\n"); return 0; }

    /* ── simulate download ─────────────────────────────────────────── */
    printf("\n  Downloading %s...\n", pkg.name);
    for (int p = 0; p <= 100; p += 5) {
        progress_bar("Downloading", p);
        usleep(40000);   /* 40 ms per step = ~0.8 s total */
    }
    putchar('\n');

    /* In a real implementation, use libcurl or exec wget:
     *
     *   char cmd[512];
     *   snprintf(cmd, sizeof(cmd),
     *            "wget -q -O %s/%s.tar.gz '%s'",
     *            PKG_CACHE, pkg.name, pkg.url);
     *   system(cmd);
     */

    /* ── simulate extraction + install ────────────────────────────── */
    printf("  Installing %s...\n", pkg.name);
    for (int p = 0; p <= 100; p += 10) {
        progress_bar("Installing ", p);
        usleep(30000);
    }
    putchar('\n');

    /*
     * Real steps would be:
     *   1. tar -xzf /tmp/bigdos-pkg-cache/<name>.tar.gz -C /
     *   2. Run optional post-install script
     */

    /* ── create stub binary in /bin ───────────────────────────────── */
    char bin_path[256];
    snprintf(bin_path, sizeof(bin_path), "%s/%s", BIN_DIR, pkg.name);
    FILE *b = fopen(bin_path, "w");
    if (b) {
        fprintf(b, "#!/bin/sh\necho \"BIG-DOS: %s %s stub — replace with real binary\"\n",
                pkg.name, pkg.version);
        fclose(b);
        chmod(bin_path, 0755);
    }

    /* ── update local database ─────────────────────────────────────── */
    db_add(pkg.name, pkg.version);

    printf("\n:: %s %s installed successfully.\n\n", pkg.name, pkg.version);
    return 0;
}

/* ── pk remove ─────────────────────────────────────────────────────── */
static int pk_remove(const char *name) {
    pk_init();
    if (!is_installed(name)) {
        fprintf(stderr, "error: '%s' is not installed.\n", name);
        return 1;
    }
    printf(":: Remove package '%s'? [Y/n] ", name);
    fflush(stdout);
    char ans[8];
    if (!fgets(ans, sizeof(ans), stdin)) return 1;
    if (ans[0] == 'n' || ans[0] == 'N') { printf("Aborted.\n"); return 0; }

    char bin_path[256];
    snprintf(bin_path, sizeof(bin_path), "%s/%s", BIN_DIR, name);
    remove(bin_path);
    db_remove(name);
    printf(":: Removed '%s'.\n", name);
    return 0;
}

/* ── pk list ───────────────────────────────────────────────────────── */
static int pk_list(void) {
    pk_init();
    FILE *f = fopen(INSTALLED_DB, "r");
    if (!f) { printf("No packages installed.\n"); return 0; }
    char line[256];
    int count = 0;
    printf("\n  %-20s %-12s %-12s\n", "Package", "Version", "Installed");
    printf("  %-20s %-12s %-12s\n",
           "--------------------", "------------", "------------");
    while (fgets(line, sizeof(line), f)) {
        char n[64], v[32], d[32];
        if (sscanf(line, "%63[^|]|%31[^|]|%31[^\n]", n, v, d) >= 2) {
            printf("  %-20s %-12s %-12s\n", n, v, d);
            count++;
        }
    }
    fclose(f);
    printf("\n  %d package(s) installed.\n\n", count);
    return 0;
}

/* ── pk search ─────────────────────────────────────────────────────── */
static int pk_search(const char *query) {
    pk_init();
    FILE *f = fopen(REPO_DB, "r");
    if (!f) { fprintf(stderr, "pk: repo.db not found — run 'pk update'\n"); return 1; }
    char line[512];
    int found = 0;
    printf("\n  Search results for '%s':\n\n", query);
    while (fgets(line, sizeof(line), f)) {
        char n[64], v[32], u[256], d[128];
        long sz = 0;
        if (sscanf(line, "%63[^|]|%31[^|]|%255[^|]|%ld|%127[^\n]", n, v, u, &sz, d) >= 4) {
            if (strstr(n, query) || strstr(d, query)) {
                char inst = is_installed(n) ? '*' : ' ';
                printf("  %c %-20s %-10s %6ld KB  %s\n", inst, n, v, sz, d);
                found++;
            }
        }
    }
    fclose(f);
    if (!found) printf("  No packages matching '%s'.\n", query);
    printf("\n  (* = installed)\n\n");
    return 0;
}

/* ── pk update ─────────────────────────────────────────────────────── */
static int pk_update(void) {
    pk_init();
    printf(":: Syncing package database from remote...\n");
    /* Real: wget/curl REPO_URL → REPO_DB */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "curl -fsSL '%s' -o '%s' 2>/dev/null || "
             "wget -q '%s' -O '%s' 2>/dev/null",
             REPO_URL, REPO_DB, REPO_URL, REPO_DB);
    int rc = system(cmd);
    if (rc != 0)
        printf("  (Network unavailable — keeping local repo.db)\n");
    else
        printf(":: Database synced.\n");
    return 0;
}

/* ── pk upgrade ────────────────────────────────────────────────────── */
static int pk_upgrade(void) {
    pk_init();
    printf(":: Checking for upgrades...\n");
    FILE *f = fopen(INSTALLED_DB, "r");
    if (!f) { printf(":: Nothing to upgrade.\n"); return 0; }
    char line[256];
    int upgraded = 0;
    while (fgets(line, sizeof(line), f)) {
        char n[64], v[32];
        if (sscanf(line, "%63[^|]|%31[^|]", n, v) == 2) {
            RepoPkg latest;
            if (repo_find(n, &latest) &&
                strcmp(latest.version, v) != 0) {
                printf(":: Upgrading %s (%s → %s)\n", n, v, latest.version);
                db_remove(n);
                db_add(n, latest.version);
                upgraded++;
            }
        }
    }
    fclose(f);
    printf(":: %d package(s) upgraded.\n", upgraded);
    return 0;
}

/* ── top-level dispatcher ──────────────────────────────────────────── */
int pk_dispatch(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: pk install|remove|list|search|update|upgrade [PKG]\n");
        return 1;
    }
    const char *sub = argv[1];
    if      (strcmp(sub, "install") == 0 || strcmp(sub, "-S") == 0) {
        if (argc < 3) { fprintf(stderr, "pk: specify a package name\n"); return 1; }
        return pk_install(argv[2]);
    }
    else if (strcmp(sub, "remove")  == 0 || strcmp(sub, "-R") == 0) {
        if (argc < 3) { fprintf(stderr, "pk: specify a package name\n"); return 1; }
        return pk_remove(argv[2]);
    }
    else if (strcmp(sub, "list")    == 0 || strcmp(sub, "-Q") == 0) return pk_list();
    else if (strcmp(sub, "search")  == 0 || strcmp(sub, "-Ss") == 0) {
        if (argc < 3) { fprintf(stderr, "pk: specify a search query\n"); return 1; }
        return pk_search(argv[2]);
    }
    else if (strcmp(sub, "update")  == 0 || strcmp(sub, "-Sy") == 0) return pk_update();
    else if (strcmp(sub, "upgrade") == 0 || strcmp(sub, "-Su") == 0) return pk_upgrade();
    else {
        fprintf(stderr, "pk: unknown subcommand '%s'\n", sub);
        return 1;
    }
}
