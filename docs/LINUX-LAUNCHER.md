# Linux desktop launcher

Run the installed `bin/YKW4Recomp` without arguments in a graphical session
(native Linux or WSLg). Python 3.10+ and Tk are required. No Wine is used.
The runtime starts the sibling `tools/desktop_launcher.py`; keep installed
tools, logo and language files alongside the binary. Build instructions are
in `../BUILDING.md`.

The desktop shell uses a navy sidebar, light content surfaces, yellow selection
and flat blue controls instead of Tk's default gray/beveled widgets. The sidebar
retains keyboard/controller focus behavior and the five underlying pages. The
Activity button opens a separate log window so logs never displace Play/Apply
at the minimum supported 980x780 window size. No extra GUI dependency is added.

The interface includes the project logo and five tabs:

- Juego y ajustes: FPS, resolution, display, controller preferences and local
  preparation/update from extracted ExeFS/RomFS or NSP/XCI plus a Linux backend.
- Mods: folder/ZIP import, activation, ordering, translations and GameBanana.
- Discord: optional Rich Presence through a user-owned local Discord IPC socket.
  It runs while a game launched from this interface runs; no automatic in-game
  area detection is claimed.
- Compilar moviles: existing build assistant with explicit consent. Android
  needs SDK/NDK tools; IPA needs macOS/Xcode, not Linux or WSL.
- Equipo: local binding status, validation and an explicit bind-existing action.

Shared language dictionaries are loaded for Spanish, English, Japanese and German.
English and German include the Linux preparation/binding dialogs, confirmations,
status text and mod headings. Technical output from external tools is preserved.
The separate mobile build assistant has its own interface. The interface is not the
old SDL launcher menu and is not a claim of pixel-identical Win32 controls.

## Installation folders

A module next to the runtime selects a portable installation. Otherwise the
last successfully prepared location is remembered in
`$XDG_CONFIG_HOME/ykw4recomp/launcher.json` (default `~/.config`). Failed or
cancelled preparation does not switch the active installation. A non-writable
runtime directory falls back to `$XDG_DATA_HOME/ykw4recomp/game` (default
`~/.local/share`). The helper's `--base` option overrides automatic selection.
Settings are reloaded when preparation switches installations.

Prepare Linux ELF modules locally; an existing Windows DLL is not convertible
by changing its extension. See `local-module-binding.md` before migration:
new preparations bind locally only after explicit consent, while ordinary
legacy modules remain unbound unless explicitly converted.

The package helper needs an installed Linux hactool, your own console keys
and authorized base/update/DLC files. The preparation dialog accepts an explicit
backend path. Extracted preparation does not need hactool. The optional backend
build requires separately supplied patched sources; see `../BUILDING.md`.

## Input and checks

Keyboard focus, Enter/Space and Escape work through Tk. Readable Linux joystick
devices `/dev/input/js0` through `js3` provide conventional A/B and vertical
stick/D-pad navigation; no device permissions are escalated. Controller layouts
vary, and physical controllers have not been verified under WSL.

Synthetic tests and Xvfb smoke tests do not establish full-game preparation,
graphics performance, real controller behavior, live GameBanana downloads or
successful APK/IPA production. WSL needs WSLg or another usable display server;
a headless shell alone cannot display the launcher.

## Linux performance and software rendering

The game uses Vulkan. Before Play, the Linux helper queries Vulkan adapters in
a separate process with a five-second timeout, without opening a game or a
window. When only CPU adapters expose Vulkan 1.3 or newer, it asks for explicit
confirmation (default No). An NVIDIA adapter appearing in `nvidia-smi` is not
proof that this Vulkan renderer can use it. Unknown probe results are logged,
not presented as hardware acceleration. A Vulkan version check alone does not
guarantee every feature required by the renderer.

The probe respects the inherited driver environment; it does not force
lavapipe, replace drivers, disable security checks or change graphics quality.
For a no-window diagnostic:

```sh
python3 scripts/launcher_core.py --graphics-info
```

CPU rendering is a diagnostic fallback, not a playable-performance guarantee.
Lower resolution, handheld mode and disabled shadows can reduce its workload,
but cannot make it equivalent to a hardware GPU. Validate native Linux with
the actual Vulkan device shown in the runtime log; WSL software rendering does
not establish native Linux performance.

Normal preparation uses optimization level 3. The LLVM backend already targets
the build machine's CPU and instruction features for local modules; a diagnostic
O1 module should not be used as the final CPU-performance build. Recompilation
does not fix missing GPU acceleration. Module updates must keep the existing
binding policy and must not overwrite an active game's module.

Launcher log traffic is bounded to 500 pending messages of at most 4096 characters.
Control events (completion, errors and confirmations) are not dropped or blocked
behind log floods. Text-widget updates are batched and the visible history is
trimmed to about 2000 lines. Hidden activity windows are not scrolled on every
batch. These changes reduce launcher overhead; they are not GPU FPS claims.

The following tests create no GUI and launch no game; subprocesses and dialogs
in the launcher-flow tests are mocked:

```sh
python3 tests/desktop_launcher_test.py
python3 tests/linux_launcher_performance_test.py
python3 tests/linux_launcher_io_test.py
```

Mod discovery applies the saved priority order before loading metadata for the
128 displayed mods. Directory and RomFS-link checks still apply to candidates;
duplicate saved priorities retain their first position. This avoids reading
every mod.ini in large libraries without changing the visible order.

Game settings and mod settings are saved with one YKW4.ini update. Unchanged INI
content and active-mod lists are not rewritten. Changed files still use atomic
replacement and fsync; unchanged symlinks are replaced rather than retained.
Comments, unknown settings and existing encodings are preserved as before.
These are launcher I/O improvements, not measured game FPS improvements.

## Interface regression tests

With Python/Tk installed and an X display (or Xvfb), run:

```sh
xvfb-run -a python3 tests/desktop_launcher_ui_test.py
```

The tests cover navigation, selection/title synchronization, state gating,
widget bounds at 980x780 and 1060x820, the independent activity window,
installation switching and inherited preparation-dialog styling. To test the
installed helper rather than source, set `YKW4_DESKTOP_TOOLS` to its `bin/tools`
directory and `YKW4_RUNTIME` to its `bin/YKW4Recomp` executable.
