# BIG-DOS — Makefile
# Usage:
#   make          → build text-mode binary
#   make gui      → build with Raylib GUI
#   make iso      → run full build.sh pipeline
#   make run      → build ISO + launch QEMU
#   make clean    → remove build artifacts

CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=c11
SRC     = src/main.c src/commands.c src/gui.c src/pkgmgr.c
BIN     = build/bigdos

.PHONY: all gui iso run clean

all: $(BIN)

$(BIN): $(SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) -DBIGDOS_NO_GUI $(SRC) -o $(BIN) -lm
	@echo "Built (no GUI): $(BIN)"

gui: $(SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) \
	    $$(pkg-config --cflags --libs raylib 2>/dev/null || echo "-lm")
	@echo "Built (with GUI): $(BIN)"

iso:
	@bash build.sh

run:
	@bash build.sh --run

clean:
	@bash build.sh --clean
	@rm -f $(BIN)
	@echo "Clean complete."
