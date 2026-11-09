# SwitchPrep

Local NSP/XCI preparation backend for the native Windows setup/update dialog. Select
the base NSP or XCI, optional update and DLC packages, and local key files directly in the
launcher, then press Prepare. The backend runs without a console, returns the
prepared paths automatically, and the launcher continues compilation/installation.
The original extracted-folder mode remains available under Use folders.
SwitchPrep can also run independently from a console.

- Reads CNMT metadata instead of guessing titles from filenames.
- Groups base applications, updates and DLC by their application identifier.
- Lets you choose an update and individual DLC; duplicate DLC versions cannot be selected together.
- Applies the chosen update over its matching base NCA and merges ExeFS.
- Verifies selected NCA content SHA-256 against CNMT while copying.
- Reads NSP/PFS0, XCI/HFS0 and RomFS with bounds, duplicate and traversal checks;
  RomFS directory cycles are rejected too.
- Creates a new output directory for each successful preparation. Inputs are never modified.
- Uses English or Spanish, based on the launcher language or system locale. Other locales fall back to English.
- No network requests, automatic key downloads, telemetry or game downloads.

The user supplies their own local dumps and keys. `title.keys` is optional and,
if present beside `prod.keys`, is used automatically. Some contents require it.
Existing encrypted keys from common tickets in the supplied packages are also
merged only into the temporary title-key file. Personalized tickets are ignored.
Only NCA extraction key entries are written to the temporary backend prod.keys.
Unrelated hardware derivation entries are ignored; malformed NCA keys are rejected
by name and expected length without logging their values. Original keys are never modified.
Keys are copied only into a temporary private working directory for the backend
and removed on normal completion/cancellation. They are never included in the
extracted output or reports. Abrupt power loss/process termination can leave
work directories; treat any remaining temporary data as private.

Supports single-file NSP and XCI dumps, including trimmed XCIs whose partition
contents remain complete. Compressed NSZ/XCZ, split dumps and multi-program games
are not supported. Missing delta fragments are allowed because base + patch
reconstruction does not need them. Required program/data content is still checked.
No general compatibility or legal certification is claimed.

## XCI selection

Select the original XCI directly; no XCI-to-NSP conversion is needed. SwitchPrep
reads the root HFS0 and its secure partition, discovers titles through CNMT, and
copies selected NCA bytes without rewriting their game-card headers. The same
NCA-to-CNMT SHA-256 checks used for NSP remain mandatory. This does not repair
already modified NCA files or bypass a hash mismatch. HFS0 signatures and prefix
hashes are not authenticated by this parser; CNMT checks apply to materialized
content, not to the entire game-card image.

An XCI may bundle a base game, an older update and DLC. In the native dialog:

- Base: select the XCI containing the base game.
- Update: select a separate update NSP, or select the same XCI to use its bundled
  compatible update. Leaving this field empty does not apply a bundled update.
- DLC: explicitly select the DLC NSPs, or the same XCI to include its compatible
  DLC titles. Leaving this field empty does not install bundled DLC.

Only content for the requested application is used. Selecting a container for
DLC selects all compatible DLC titles in that container; use separate DLC NSPs
or the console assistant to choose individual titles. Game-card NCAs without
rights-ID encryption do not need a ticket/title key, but local NCA keys are still
required. Original dumps and key files are never modified.

## Backend and licenses

The assistant calls [hactool](https://github.com/SciresM/hactool), ISC, with
`--suppresskeys --disablekeywarns`. It uses fixed raw output files; its own bounded
parsers extract PFS0/RomFS paths. Raw backend messages are not displayed or exported
because they can contain sensitive data. A backend failure reports the stage
without printing key values.

The Windows beta includes the locally built hactool backend under `backend/`,
its ISC and YKW4Recomp GPL adaptation notices and corresponding source/patches in the source archive. The runtime
itself remains extracted-files-only. The helper's source is GPL-3.0-or-later; the launcher's
existing GPL and upstream notices still apply. Python/PyInstaller packaging
notices accompany the binary. No keys, firmware, game contents or prepared
installation are part of the distributable archive.

Extracted output belongs to your private installation. Do not share it with the
launcher ZIP. The existence of an open-source tool does not grant rights to game
content or establish an exception to local anti-circumvention law.

## Development

Python 3.12+, standard library only:

```
python scripts/switchprep/switchprep.py --folder PATH --backend HACTOOL --keys PROD_KEYS
python -m unittest discover -s scripts/switchprep -p "test_*.py"
```

Windows 10/11 executable packaging uses Python 3.12 and PyInstaller 6.16.0:

```
python -m PyInstaller scripts/switchprep/SwitchPrep.spec
```

`--scan-only --report REPORT.json` performs metadata discovery without extracting
game payloads. `--title-id` restricts the application shown by the launcher.
`--result` writes a new UTF-16 INI with the prepared paths after successful output
publication. Cancelling or failing does not publish a successful result.

Native launcher protocol: `--request selection.ini --output OUTPUT --result RESULT --title-id APPLICATION`.
The request is a UTF-16 INI with a `Selection` section: `base`, optional `update`,
`keys`, optional `title_keys`, `dlc_count` and `dlc0` through `dlc63`. No prompts
are shown in this mode. Only explicitly selected NSP/XCI files are inspected. The launcher
stores extracted data and a persistent `setup.log` under its own unique
`setup-work` attempt directory. Open log remains available after failure.
The native cancellation path removes private backend key copies if the helper
process must be terminated. Native mode does not imply direct execution of an
encrypted NSP/XCI: local preparation and compilation still precede gameplay.

The native NSP/XCI path keeps validated RomFS/DLC images and hands them directly to installation. It avoids unpacking every RomFS file and repacking the same data again. The standalone interactive extraction mode still writes folders.
