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
- **Encrypted notes** — device-bound encrypted note vault (`Ctrl+Y`); optional per-note password (Argon2id + XSalsa20)

## Install

### Quick install (macOS / Linux)

```bash
curl -fsSL https://raw.githubusercontent.com/Quantre34/cref/main/install.sh | sh
```

The script detects your OS and architecture, downloads the correct binary from the latest release, and installs it to `~/.local/bin/`.

### Build from source

**Dependencies:** `gcc`, `make`, `ncurses`, `libsodium`

```bash
# macOS
brew install libsodium   # ncurses usually already present via Xcode CLT

# Ubuntu / Debian
sudo apt-get install -y libncurses-dev libsodium-dev

# Fedora / RHEL
sudo dnf install -y ncurses-devel libsodium-devel
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

### Updating to a new version

Re-run whichever install method you used originally — it overwrites the existing binary in place:

```bash
curl -fsSL https://raw.githubusercontent.com/Quantre34/cref/main/install.sh | sh
```

```bash
# or, from a source checkout
git pull
make
sudo make install      # same PREFIX you used before
```

Then **open a new terminal** (or run `hash -r` in bash / `rehash` in zsh in the current one) before checking — see "still running the old version" below if `cref` still looks unchanged after that.

### Uninstalling

If you installed via the quick-install script:

```bash
rm "${CREF_INSTALL_DIR:-$HOME/.local/bin}/cref"
```

If you installed from source with `make install`, use the matching `PREFIX`:

```bash
make uninstall PREFIX=/usr/local   # or PREFIX=~/.local, whichever you used
```

**Still running the old version after reinstalling?** Run `which -a cref` first — it lists every copy on your `PATH`, in the order your shell would pick them. Two common causes:

- **You have more than one `cref` on your `PATH`.** The quick-install script and `make install` use different locations (`~/.local/bin` vs `/usr/local/bin` by default), so if you've ever used both, both binaries still exist — and whichever directory comes first in `PATH` wins, not necessarily the one you installed most recently. Remove the one you don't want by its exact path from the `which -a` output, e.g.:

  ```bash
  sudo rm /usr/local/bin/cref        # remove the make-install copy
  # or
  rm ~/.local/bin/cref               # remove the quick-install copy
  ```

  Don't automate this as `sudo rm $(which cref)` — plain `which` returns only the *first* match, so if that happens to be the correct, newer binary (common once `~/.local/bin` is early on `PATH`), the command deletes the one you meant to keep instead of the stale one. Check `which -a cref`'s output yourself and remove the specific path that's actually stale.

- **Your shell cached the old path.** bash/zsh remember the resolved path of a command for the lifetime of the shell session, so a terminal that already ran `cref` before you reinstalled it will keep using the old binary even if a newer one now comes first on `PATH`. Run `hash -r` (bash) or `rehash` (zsh) after removing/reinstalling, or just open a new terminal.

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
| `Ctrl+Y` | Toggle notes panel |
| `Ctrl+N` | New file |
| `Ctrl+K` | New directory |
| `Ctrl+G` | Go to directory (type path) |
| `Ctrl+D` | Delete selected file |
| `Ctrl+R` | Reload directory |
| `Ctrl+B` | Build (`make` or `gcc` all `.c` files) |
| `Ctrl+O` | Open embedded terminal |
| `Ctrl+P` | Theme picker (Default/Dracula/Nord/Gruvbox/Solarized) |
| `?` | Help overlay |
| `q` | Quit |

### Notes Panel (`Ctrl+Y`)
| Key | Action |
|-----|--------|
| `↑` / `↓` or `k` / `j` | Navigate notes |
| `→` / `Enter` | Open note (password prompt if locked) |
| `e` | Edit open note |
| `Ctrl+Y` | Back to file tree |
| `Ctrl+N` | New note (optionally password-protected) |
| `Ctrl+K` | New category / folder |
| `Ctrl+D` | Delete note |
| `/` | Search in unencrypted notes |

### Note Editor
| Key | Action |
|-----|--------|
| `Esc` | Exit editor, return to notes |
| `Ctrl+S` | Save |
| `Ctrl+Z` | Undo |
| `Ctrl+E` | Set / change / remove note password |
| `Ctrl+F` | Find in note |
| `Ctrl+A` | Select all |
| `Ctrl+C` / `Ctrl+V` | Copy / Paste |
| `Ctrl+X` | Cut |
| `Ctrl+D` | Duplicate line |

### File Editor
| Key | Action |
|-----|--------|
| Scroll wheel / touchpad | Scroll content |
| `Ctrl+S` | Save |
| `Ctrl+Z` | Undo |
| `Ctrl+F` | Find in file |
| `Ctrl+A` | Select all |
| `Ctrl+C` / `Ctrl+V` | Copy / Paste (Ctrl+C also copies to OS clipboard) |
| `Ctrl+X` | Cut (also copies to OS clipboard) |
| `Ctrl+K` | Delete current line |
| `Alt+↑` / `Alt+↓` | Move line up / down |
| `Ctrl+D` | Duplicate line |
| `Ctrl+R` | Reload file from disk |
| `Shift+Alt+←/→` | Word selection |
| `Ctrl+P` | Theme picker |

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
