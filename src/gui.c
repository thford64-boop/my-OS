/*
 * BIG-DOS — gui.c
 * Windowed GUI using Raylib.
 *
 * To compile with Raylib:
 *   gcc ... gui.c -lraylib -lm -ldl -lpthread -lGL
 *
 * To compile WITHOUT Raylib (stub / text-mode fallback):
 *   gcc -DBIGDOS_NO_GUI ... gui.c
 *
 * Window system design:
 *   - Windows are stored in a fixed array (windows[]).
 *   - The focused window is the topmost/active one.
 *   - Dragging is detected by mouse-down on the title bar.
 *   - Each window has an optional render callback for its content.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Window struct ─────────────────────────────────────────────────── */
#define MAX_WINDOWS  16
#define TITLE_H      28         /* title bar height in pixels           */
#define BORDER_W      2         /* window border width                  */

typedef struct Window_ {
    int   x, y;                 /* top-left corner                      */
    int   width, height;        /* total dimensions (title bar included) */
    int   focused;              /* 1 = active / topmost                 */
    int   open;                 /* 1 = visible, 0 = closed/destroyed    */
    char  title[64];            /* title bar text                       */
    void (*draw_content)(struct Window_ *w);  /* content renderer cb    */
} Window;

static Window windows[MAX_WINDOWS];
static int    wcount = 0;

/* ── window management ─────────────────────────────────────────────── */
Window *win_create(int x, int y, int w, int h, const char *title) {
    if (wcount >= MAX_WINDOWS) return NULL;
    Window *win     = &windows[wcount++];
    win->x          = x;
    win->y          = y;
    win->width      = w;
    win->height     = h;
    win->focused    = 0;
    win->open       = 1;
    win->draw_content = NULL;
    strncpy(win->title, title, sizeof(win->title) - 1);
    return win;
}

static void win_focus(Window *target) {
    for (int i = 0; i < wcount; i++)
        windows[i].focused = (&windows[i] == target) ? 1 : 0;
}

static void win_close(Window *w) {
    w->open = 0;
}

/* ── content renderers ─────────────────────────────────────────────── */
#ifndef BIGDOS_NO_GUI
#include "raylib.h"

static void draw_terminal_content(Window *w) {
    /* Simple static terminal prompt inside the window */
    int cx = w->x + BORDER_W + 6;
    int cy = w->y + TITLE_H + 6;
    DrawText("bigdos> _", cx, cy, 14, DARKGREEN);
    DrawText("BIG-DOS Terminal v1.0", cx, cy + 20, 12, GRAY);
    DrawText("Type commands below:", cx, cy + 36, 12, GRAY);
}

static void draw_filemanager_content(Window *w) {
    int cx = w->x + BORDER_W + 6;
    int cy = w->y + TITLE_H + 8;
    const char *files[] = { "/bin", "/etc", "/home", "/proc", "/dev", NULL };
    for (int i = 0; files[i]; i++) {
        Color col = (i % 2 == 0) ? SKYBLUE : BLUE;
        DrawText(files[i], cx, cy + i * 18, 13, col);
    }
}

static void draw_about_content(Window *w) {
    int cx = w->x + BORDER_W + 12;
    int cy = w->y + TITLE_H + 14;
    DrawText("BIG-DOS  v1.0",     cx, cy,      18, WHITE);
    DrawText("A modular Linux OS", cx, cy + 26, 13, LIGHTGRAY);
    DrawText("Built in C + Raylib", cx, cy + 44, 13, LIGHTGRAY);
    DrawRectangle(cx, cy + 70, w->width - 24, 2, DARKGRAY);
    DrawText("(c) BIG-DOS Project", cx, cy + 80, 11, GRAY);
}

/* ── draw one window ───────────────────────────────────────────────── */
static void draw_window(Window *w) {
    if (!w->open) return;

    Color border_col = w->focused ? RAYWHITE : DARKGRAY;
    Color title_col  = w->focused ? (Color){40, 40, 120, 255} : DARKGRAY;

    /* shadow */
    DrawRectangle(w->x + 4, w->y + 4, w->width, w->height,
                  (Color){0, 0, 0, 80});

    /* border */
    DrawRectangle(w->x - BORDER_W, w->y - BORDER_W,
                  w->width + 2 * BORDER_W, w->height + 2 * BORDER_W,
                  border_col);

    /* body */
    DrawRectangle(w->x, w->y, w->width, w->height, (Color){18, 18, 28, 255});

    /* title bar */
    DrawRectangle(w->x, w->y, w->width, TITLE_H, title_col);
    DrawText(w->title, w->x + 8, w->y + 7, 14, WHITE);

    /* close button */
    DrawRectangle(w->x + w->width - 22, w->y + 5, 16, 16,
                  (Color){180, 40, 40, 255});
    DrawText("X", w->x + w->width - 18, w->y + 6, 13, WHITE);

    /* content area */
    BeginScissorMode(w->x, w->y + TITLE_H, w->width,
                     w->height - TITLE_H);
    if (w->draw_content) w->draw_content(w);
    EndScissorMode();
}

/* ── input handling ────────────────────────────────────────────────── */
static int   dragging  = 0;
static Window *drag_w  = NULL;
static int   drag_ox, drag_oy;   /* offset from window origin */

static void handle_input(void) {
    Vector2 mp = GetMousePosition();
    int     mx = (int)mp.x, my = (int)mp.y;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        /* check windows in reverse order (topmost first) */
        for (int i = wcount - 1; i >= 0; i--) {
            Window *w = &windows[i];
            if (!w->open) continue;

            /* hit test */
            if (mx >= w->x && mx <= w->x + w->width &&
                my >= w->y && my <= w->y + w->height) {

                win_focus(w);

                /* close button */
                if (mx >= w->x + w->width - 22 &&
                    my >= w->y + 5 && my <= w->y + 21) {
                    win_close(w);
                    break;
                }

                /* title bar drag start */
                if (my <= w->y + TITLE_H) {
                    dragging = 1;
                    drag_w   = w;
                    drag_ox  = mx - w->x;
                    drag_oy  = my - w->y;
                }
                break;
            }
        }
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        dragging = 0; drag_w = NULL;
    }

    if (dragging && drag_w) {
        drag_w->x = mx - drag_ox;
        drag_w->y = my - drag_oy;
    }
}

/* ── taskbar ───────────────────────────────────────────────────────── */
static void draw_taskbar(int sw, int sh) {
    int tbh = 36;
    DrawRectangle(0, sh - tbh, sw, tbh, (Color){20, 20, 60, 255});
    DrawText("BIG-DOS 1.0", 8, sh - tbh + 10, 14, WHITE);

    /* buttons for each open window */
    int bx = 120;
    for (int i = 0; i < wcount; i++) {
        if (!windows[i].open) continue;
        Color bc = windows[i].focused
                   ? (Color){60, 60, 160, 255}
                   : (Color){40, 40, 80, 255};
        DrawRectangle(bx, sh - tbh + 4, 100, 28, bc);
        DrawText(windows[i].title, bx + 4, sh - tbh + 11, 12, WHITE);
        bx += 108;
    }
}

/* ── desktop icons ─────────────────────────────────────────────────── */
typedef struct { int x, y; const char *label; int w_idx; } Icon;
static Icon icons[] = {
    { 20, 20, "Terminal",     0 },
    { 20, 80, "Files",        1 },
    { 20, 140, "About",       2 },
};
#define NUM_ICONS (int)(sizeof(icons)/sizeof(icons[0]))

static void draw_desktop(void) {
    Vector2 mp = GetMousePosition();
    for (int i = 0; i < NUM_ICONS; i++) {
        Icon *ic = &icons[i];
        int hover = (mp.x >= ic->x && mp.x <= ic->x + 48 &&
                     mp.y >= ic->y && mp.y <= ic->y + 48);
        Color bg = hover ? (Color){80, 80, 200, 200} : (Color){40, 40, 140, 180};
        DrawRectangle(ic->x, ic->y, 48, 48, bg);
        DrawText(ic->label, ic->x, ic->y + 52, 11, WHITE);

        /* double-click to open (use IsMouseButtonPressed as proxy) */
        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (ic->w_idx < wcount)
                windows[ic->w_idx].open = 1;
        }
    }
}

/* ── main GUI entry point ──────────────────────────────────────────── */
int start_gui(void) {
    int SW = 1024, SH = 600;
    InitWindow(SW, SH, "BIG-DOS Windowed GUI");
    SetTargetFPS(60);

    /* create default windows */
    Window *term = win_create(120, 60,  500, 300, "Terminal");
    Window *files = win_create(300, 160, 320, 220, "Files");
    Window *about = win_create(500, 80,  280, 180, "About BIG-DOS");

    if (term)  { term->draw_content  = draw_terminal_content;  win_focus(term); }
    if (files) { files->draw_content = draw_filemanager_content; files->open = 0; }
    if (about) { about->draw_content = draw_about_content;       about->open = 0; }

    while (!WindowShouldClose()) {
        handle_input();
        BeginDrawing();
        ClearBackground((Color){10, 10, 30, 255});   /* dark desktop */

        draw_desktop();

        for (int i = 0; i < wcount; i++)
            draw_window(&windows[i]);

        draw_taskbar(SW, SH);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}

#else /* BIGDOS_NO_GUI */

/* ── stub for non-GUI builds ───────────────────────────────────────── */
int start_gui(void) {
    fprintf(stderr,
        "BIG-DOS: GUI not compiled in.\n"
        "Rebuild without -DBIGDOS_NO_GUI and link -lraylib.\n");
    return 1;
}

#endif /* BIGDOS_NO_GUI */