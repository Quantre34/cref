# cref

A terminal file browser and editor with a built-in C standard library reference.

Browse any directory, edit files, grep across codebases — and press `Ctrl+L` to instantly switch to an offline C stdlib reference covering functions, types, macros, and more.

![platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux-blue)
![license](https://img.shields.io/badge/license-GPLv3-green)

## Features

- **File browser** — tree view for any directory, fuzzy search, filetype filter; mouse wheel / touchpad scroll
- **Editor** — full text editing with undo, selection, clipboard, in-file find; scroll with mouse wheel
- **C reference library** — 64 offline markdown files covering the C standard library (`--ref` or `Ctrl+L`)
- **Embedded terminal** — real PTY shell inside the TUI (`Ctrl+O`)
- **Grep mode** — search inside file contents across the whole directory
- **Syntax highlighting** — via `bat` if installed, with built-in fallback tokenizer
- **Build runner** — `Ctrl+B` runs `make` if a Makefile exists, otherwise compiles all `.c` files in the directory with automatic library detection (~130 headers mapped to linker flags)

## Install

### Quick install (macOS / Linux)

```bash
curl -fsSL https://raw.githubusercontent.com/Quantre34/cref/main/install.sh | sh
```

The script detects your OS and architecture, downloads the correct binary from the latest release, and installs it to `~/.local/bin/`.

### Build from source

**Dependencies:** `gcc`, `make`, `ncurses`

```bash
# macOS
brew install ncurses   # usually already present via Xcode CLT

# Ubuntu / Debian
sudo apt-get install -y libncurses-dev

# Fedora / RHEL
sudo dnf install -y ncurses-devel
```

```bash
git clone https://github.com/Quantre34/cref.git
cd cref
make
sudo make install      # installs to /usr/local/bin + /usr/local/share/cref/ref
```

Or install for the current user only:

```bash
make install PREFIX=~/.local
```

## Usage

```bash
cref                        # browse current directory
cref ~/my-project           # browse a specific directory
cref --ref                  # open the C stdlib reference library
cref malloc                 # pre-fill the search query
cref --grep malloc          # grep inside file contents
cref --score pointer        # count matches per file and exit
cref -d ~/src --filetype c,h  # browse only .c and .h files
```

## Keyboard Shortcuts

### File List
| Key | Action |
|-----|--------|
| `↑` / `↓` or `k` / `j` | Navigate |
| Scroll wheel / touchpad | Scroll list |
| `Enter` / `→` | Open file |
| `Esc` / `←` | Back |
| Type | Fuzzy search |
| `Ctrl+T` | Filter by file type (e.g. `c,h`) |
| `Ctrl+L` | Toggle C reference library |
| `Ctrl+N` | New file |
| `Ctrl+D` | Delete selected file |
| `Ctrl+R` | Reload directory |
| `Ctrl+B` | Build (`make` or `gcc` all `.c` files) |
| `Ctrl+O` | Open embedded terminal |
| `?` | Help overlay |
| `q` | Quit |

### Editor
| Key | Action |
|-----|--------|
| Scroll wheel / touchpad | Scroll content |
| `Ctrl+S` | Save |
| `Ctrl+Z` | Undo |
| `Ctrl+F` | Find in file |
| `Ctrl+A` | Select all |
| `Ctrl+C` / `Ctrl+V` | Copy / Paste |
| `Ctrl+X` | Cut |
| `Alt+↑` / `Alt+↓` | Move line up / down |
| `Ctrl+D` | Duplicate line |
| `Ctrl+R` | Reload file from disk |
| `Shift+Alt+←/→` | Word selection |

### Grep Mode
| Key | Action |
|-----|--------|
| `↑` / `↓` | Navigate hits |
| `Enter` | Jump to file and line |
| `Esc` | Back to file list |

## C Reference Library

`cref --ref` (or press `Ctrl+L` from any view) opens an offline reference covering:

- Memory: `malloc`, `calloc`, `realloc`, `free`
- I/O: `printf`, `scanf`, `fopen`, `fread`, `fwrite`
- Strings: `strlen`, `strcpy`, `strcat`, `strcmp`, `strstr`
- Math: `abs`, `sqrt`, `pow`, `rand`
- Process: `exit`, `atexit`, `system`, `getenv`
- Types: `size_t`, `ptrdiff_t`, `NULL`, `EOF`
- Macros and more — 64 reference pages total

## Build System (Ctrl+B)

Press `Ctrl+B` from any view to build the current project:

- **Makefile present** → runs `make` in the project directory
- **No Makefile** → compiles all `.c` files with `gcc -Wall -Wextra -g`, output binary named after the directory

Library flags are auto-detected by scanning `#include <...>` directives across all `.c` and `.h` files in the directory. Covers ~130 headers: `math.h → -lm`, `pthread.h → -lpthread`, `ncurses.h → -lncurses`, OpenSSL, SDL2, GTK, SQLite, curl, and many more.

Build output scrolls in a dedicated panel. Press `Ctrl+B` again to rebuild, `Esc` to return.

## Building cref itself

```bash
make          # debug build
make release  # optimized build (-O2 -DNDEBUG)
make clean
make install PREFIX=/usr/local
make uninstall
```

## Project Structure

```
cref/
├── main.c      entry point, argument parsing
├── meta.h/c    frontmatter parser, directory scanner
├── search.h/c  fuzzy search, Levenshtein, scoring
├── tui.h/c     ncurses TUI, layout, keyboard events
├── term.h/c    PTY terminal emulator
├── ref/        C stdlib reference library (64 markdown files)
└── Makefile
```

## License

Copyright (C) 2026 Quantre34  
GNU General Public License v3.0 — see [LICENSE](LICENSE)
