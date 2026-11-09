# Launcher language packs

The shared dictionaries live in `src/nx/runtime/langs`. German (`de.json`) is
based on the contributed German pack, retaining its original entries and adding
the newer Windows and Linux strings. English and German have coverage checks
for launcher labels, preparation, mods, Discord, binding and confirmation dialogs.
The launcher language does not change the game's translation or the OS language
used by native file pickers. External tool logs retain their original wording.
The separate mobile build assistant is not localized by these dictionaries.

Windows embeds Spanish, English, Japanese and German and extracts missing packs
to `langs` next to the launcher. Existing files are not overwritten: edited packs
take precedence, and missing keys fall back to the current embedded dictionary.
Updating the executable therefore supplies new mappings even when an older
`langs/en.json` is already present. Unknown keys still display their source text.

Linux ships the same packs in `tools/langs`. Select `de` or `en` in the language
selector and Apply to persist it in `YKW4.ini`. Dialogs, status labels, mod headings
and the Activity window use the selected language. Numeric settings and internal
values such as `game`, `original` and `es` remain unchanged.

Add new user-facing keys to both English and German, preserving format arguments
such as `%ls`, `{path}` and `{count}`. Do not translate filenames or identifiers.
Validate without opening the launcher:

```sh
python3 tests/launcher_localization_test.py
ctest --test-dir build/launcher-windows -R '^launcher_language_(english|german)$' --output-on-failure
```

The Windows tests use a temporary partial custom pack to verify embedded fallback
and preservation of user edits. They create no windows and launch no game.
