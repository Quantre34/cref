# cref — Claude Working Notes

## Project
ncurses TUI file browser / C reference viewer. C11, gcc, macOS + Linux.
Binary: `cref`. Install: `sudo make install` → `/usr/local/bin/cref`.

## Build
```sh
make                     # dev build (debug flags)
sudo make install        # copies binary + ref/ data to /usr/local
make test                # unit + CLI tests
```

**Important:** Never run `sudo make` for the build step itself — only for install.
`sudo make` leaves `tui.o` and `cref` owned by root, breaking subsequent `make` calls.
If this happens: `sudo chown $USER /Users/macintosh/Clang/c-ara/cref /Users/macintosh/Clang/c-ara/tui.o`

## Platform notes — ncurses mouse
macOS ships ncurses v1 (`NCURSES_MOUSE_VERSION == 1`):
- Only 4 buttons decoded. Button 5 (scroll down) not supported.
- `BUTTON4_PRESSED` works (scroll up, SGR button 64).
- Scroll down: terminal sends KEY_MOUSE but `getmouse()` returns ERR with bstate=0.
- Fix: treat `getmouse()==ERR` inside a KEY_MOUSE case as scroll down.

Linux ships ncurses v2:
- Fully decodes button 5 → `BUTTON_PRESS(ev.bstate, 5)` works.
- `getmouse()==ERR` inside KEY_MOUSE is a real error, not scroll down.

Cross-platform pattern (used in all KEY_MOUSE handlers):
```c
case KEY_MOUSE: {
    MEVENT ev;
    if (getmouse(&ev) == OK) {
        if (ev.bstate & BUTTON4_PRESSED)      { /* scroll up */ }
        else if (BUTTON_PRESS(ev.bstate, 5))  { /* scroll down — Linux */ }
    } else {
        /* scroll down — macOS ncurses v1 ERR fallback */
    }
    break;
}
```

Scroll direction (user uses traditional/non-natural scrolling):
- Wheel up   → BUTTON4_PRESSED → offset/sel decrease (show earlier content)
- Wheel down → BUTTON5/ERR     → offset/sel increase (show later content)

## User preferences
- No Co-Authored-By in commits.
- No git push without explicit instruction.
- Mouse: scroll only for now. Click-to-position is a future feature.

## Future features (backlog)
- Click-to-position (BUTTON1_CLICKED) in list/content panels
- Config file (~/.config/cref/config)
- LSP integration
- Split pane in tui.c
