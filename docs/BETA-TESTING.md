# Beta testing

This is an experimental testing package. Keep a separate backup of saves and use
a fresh writable folder. Do not replace a working installation for the first test.

1. Verify the release SHA-256 and extract the complete ZIP.
2. Start without game data. Confirm that setup opens and requests separate ExeFS
   and RomFS directories, with optional already-extracted DLC.
3. Cancel setup. Confirm source files and any existing installation are unchanged.
4. Prepare matching game files. Progress and a local setup log should be produced.
5. Change launcher language among English, Spanish and Japanese.
6. Import a small permitted RomFS mod/translation as a folder and ZIP. Check enable,
   disable and priority. Do not submit the mod or game files with the report.
7. Run a battle, record intermittent stutters and capture corrupted names if seen.
8. Test updating a copy of an existing installation. Confirm saves, settings,
   translations and DLC survive; keep the original copy as a fallback.
9. Disable Discord presence and confirm it stops. Custom context is not automatic
   detection of the current zone. Do not provide a Discord token.

Report the launcher/package version, Windows build, CPU/GPU/RAM/driver, input
version, reproduction steps and sanitized logs/screenshots. Never attach game
content, keys, firmware, saves or memory dumps to public issues.

Known limitations: battle stutters, corrupted names, incomplete full-game
validation, no verified stable60/180FPS, no automatic area detection, RomFS mods
only; the runtime accepts extracted files and the optional SwitchPrep assistant prepares user-supplied NSP files. First setup on a separate tester PC remains a beta
acceptance check; passing component tests is not proof it works everywhere.


## Spanish translation

Select **Español · REY3DS 7.0** under Game and settings, or open
**Mods > Español y créditos...**. The download button opens the authors' 7.0
release. Download the Switch RAR (not the PS4 PKG), extract it, and use
**Install extracted folder** to select its `romfs` folder. Requires game update
2.2.0. Apply or Play enables the installed Spanish mod; selecting Original
disables this specific Spanish mod. Other mods keep their priority and settings.
The patch is not bundled. Existing legacy packed Spanish installations remain supported.

## Package assistant

In initial setup or Settings > Install update, use **Import NSP...**. A console
assistant scans a folder of local NSP files, reads CNMT metadata, and offers the
matching base game, updates, and individual DLC. Supply your own local keys;
no keys are downloaded. The output paths return to setup automatically. Review
and press Prepare/Update. Existing saves are retained; selected DLC are merged
with existing DLC during an update. Keep the original ZIP for redistribution;
prepared inputs and installed game data are private and are not part of it.
