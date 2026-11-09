# Adaptive in-game input guide

The SDL backend now tracks the last meaningful keyboard/mouse or mapped gamepad
input. Connecting a controller does not steal selection. Button presses and
stick/trigger motion above a threshold select the controller. Removing the active
controller returns to keyboard/mouse; removing a different controller does not.
Xbox 360/One, PlayStation 3/4/5, Switch Pro and Joy-Con families are distinguished.
Unknown mapped controllers remain generic. A native guide is now drawn over the
game, showing each original Switch button beside its actual keyboard key or
controller button. PlayStation face buttons use drawn cross/circle/square/triangle
symbols. Xbox and Switch use their physical face labels; generic pads use positions.

Press **F6** to show/hide the guide. The `input_prompts` setting remembers this
choice. The guide hides while the F2 menu or touch-layout editor is open and
respects the window's safe area. It is omitted in windows too small to fit it.
The original game HUD and its original button glyphs are not replaced.

Mappings are taken from the same live input path as the game, including custom
maps, per-player overrides, A/B swapping, and separate Joy-Con layouts. An unbound
button is shown as `--`, never guessed. Keyboard labels refer to physical US key
positions, matching the existing backend. Controller disconnects return the guide
to keyboard; connection alone never selects a new device. Settings changes are
reflected without restarting the game.

Tests cover family selection, axis noise thresholds, binding inversion, remaps,
unmapped controls, keycaps and safe-area bounds from 320x180 through 4K. They do
not substitute for testing physical controllers on every supported platform.

## Asset inspection

Local CPK table inspection of the supplied game's `data/nx/font/font.cpk`
found gaiji_key.g4tx, gaiji_xinput.g4tx, gaiji_ps4.g4tx, gaiji_nx.g4tx,
gaiji_ic.g4tx and gaiji_yw4.g4tx. No PS5-specific sheet was in that archive.
Decompression matched each entry's declared uncompressed length. The keyboard,
Xbox/PS4 and Switch resources have different layouts and dimensions; renaming
one sheet to another is not a validated replacement. No extracted game assets
are included in this repository or redistribution packages.

Replacing the original HUD still requires identifying the game's glyph lookup/registration path,
map logical actions and configured bindings to the matching glyph, and refresh
cached prompts when the input family changes. DualSense can share the PS4 face
symbols, but any distinct touchpad/menu instructions need explicit mapping.
Unknown-controller and unsupported-glyph cases must retain a valid original
symbol rather than show the wrong binding. Validate HUD, dialogue, menus,
battle prompts, hot plugging and keyboard/controller switching before enabling.

Synthetic selection tests do not establish in-game prompt correctness.
