# Emscripten (WebAssembly) Build

This builds Amulets & Armor for the browser with [Emscripten](https://emscripten.org).
The game code runs as WebAssembly; there is no DOS emulation.

## Scope

- Uses CMake as the build system (the `EMSCRIPTEN` branch of `CMakeLists.txt`).
- Single player only. IPX networking is compiled out (`WIN_IPX=0`); browsers have
  no raw UDP.
- Sound, mouse, keyboard and saved characters work.
- Mouselook (in-game mouse-driven turning) has not been tested in a browser.

## Prerequisites

Install the Emscripten SDK (any recent version; developed with 6.0.9):

```sh
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh      # once per shell
```

You also need `cmake` and `ninja`. SDL 1.2 comes with Emscripten
(`-sUSE_SDL=1`), so nothing else is installed.

## Build

From the repo root:

```sh
emcmake cmake -S . -B out/web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/web --target amulets-armor
```

Output in `out/web/`:

| File | Description |
|---|---|
| `amulets-armor.html` | The page: click-to-play, mute button, save handling |
| `amulets-armor.js` / `.wasm` | The game |
| `amulets-armor.data` | Game data from `Exe/` (about 110 MB; `.exe`, `.bat`, `.dll`, `.386` are left out) |

## Run

The files must be served over HTTP; opening them with `file://` does not work.

```sh
cd out/web
python3 -m http.server 8080
```

Then open <http://localhost:8080/amulets-armor.html> and click **Click to play**.
The click is required: browsers only allow audio after a user gesture, so the
game is started from it.

## Saved characters

Characters (`S0000000/CHDATA0x`), `config.ini` and `CONTROL.TXT` are stored in the
browser's IndexedDB. The game writes them relative to its working directory, so
`shell.html` mounts an IDBFS at `/persist` and makes those three paths symlinks
into it; no game code is involved. It syncs every few seconds, when the tab is
hidden, and when the page is closed.

- The storage is tied to the page's URL path. Serving the game from a different
  path starts with empty saves.
- Clearing the site's data in the browser deletes the saves.

## What differs from the native builds

These are the places where the web build needed something different. Most are
in `main.c` under `__EMSCRIPTEN__`.

- **Asyncify.** The game blocks in its own loops, so blocking calls become yield
  points back to the browser (`-sASYNCIFY`).
  - `SleepMS` and each `WindowsUpdate` frame call `emscripten_sleep`.
  - `delay()` (`OPTIONS.H`) calls `emscripten_sleep` directly. Emscripten's
    `SDL_Delay` is an alias that Asyncify does not treat as a yielding call, and
    using it makes the stack unwind while the caller keeps running.
- **Palette and scaling.** Emscripten's SDL cannot blit the 8-bit palette surface
  onto the 32-bit screen, so `WindowsUpdate` does the palette lookup and 2x scale
  itself.
- **Keyboard.** Emscripten's "SDL 1.2" uses SDL2-style key codes (special keys are
  `scancode | 1<<10`, and `SDLK_LAST` is much larger), so `KEYBOARD.C` builds its
  own translation table.
- **Mouse.** `SDL_GetRelativeMouseState` does not exist there, so it is built
  from motion events.
- **SDL headers.** CMake defines `WIN32=1` for the whole game, which makes SDL's
  headers take their Windows path. `aa_sdl_shim.h` is force-included so they are
  parsed with `WIN32` hidden.
- **File names.** The virtual filesystem is case sensitive, so the same
  case-insensitive `open`/`fopen` wrapper as the Linux build is used
  (`Source/Unix/casefile.c`).
- **Working directory.** The game data is preloaded into `/game` and the program
  changes to it at startup.

## Debugging

- Build with `-DCMAKE_BUILD_TYPE=Debug` and `-DCMAKE_EXE_LINKER_FLAGS=-sASSERTIONS=2`
  to get real call stacks and Emscripten's runtime checks. Note that in such a
  build the assertions abort on the fractional mouse coordinates you get when the
  canvas is scaled with CSS; release builds just truncate them.
- `FS` is exported, so the virtual filesystem can be inspected from the browser
  console (for example `Module.FS.readdir('/game')`).
- The game's own `DebugCheck` assertions (`error.log`) only exist in non-release
  builds. A trap such as `unreachable` or `null function` in a release build after
  a yield usually means a blocking call is not going through `emscripten_sleep`.
- If your checkout path contains characters such as `&`, build through a symlink
  to it; CMake's generated shell commands do not quote them.
