# YKW4Recomp

Experimental desktop launcher for a Yo-kai Watch 4 recompilation project.
It prepares user-supplied extracted files locally, manages RomFS mods and
translations, and provides settings in Spanish, English and Japanese.

The launcher is integrated into a modified YKW4Recomp runtime. This repository
includes that runtime and recompiler so the source can be built. It is not a
launcher-only executable project, an official PC port, or a guarantee of complete
static recompilation: the runtime retains interpretation paths.

## Beta status

This is a testing build. Battle stutters and corrupted names/graphics remain
under investigation. Stable 60 FPS, complete gameplay, and compatibility with
every update, DLC or mod are not verified. Do not advertise 180 FPS.

## What is included

- Native Windows preparation and settings dialogs.
- A native Linux Python/Tk launcher with the project logo, settings, mods,
  Discord, mobile-build and local-binding tabs, plus a build/install script
  (including WSL). See [Linux launcher](docs/LINUX-LAUNCHER.md).
- Local machine binding integrated into Windows and Linux module loading.
  New launcher preparations require explicit acceptance; existing ordinary
  modules are not silently converted. See [binding policy](docs/local-module-binding.md).
- A desktop mobile-build assistant with explicit responsibility and licensing
  acceptance. Android builds require SDK/NDK tools; iOS IPA builds require macOS
  and Xcode. See [mobile build limits and conditions](docs/MOBILE-BUILDS.md).
- Separate ExeFS/RomFS input and optional already-extracted DLC.
- An integrated SwitchPrep assistant to select local NSP base/update/DLC packages.
- Local recompilation and RomFS packing, without embedded game timing patches.
- Mods/translations from folders or ZIP files, with load priority.
- An optional GameBanana catalog and configurable Discord Rich Presence.
- Launcher language JSON sources in `src/nx/runtime/langs/`.
- YKW4Recomp runtime/recompiler source, licenses and build recipes.

No game dumps, extracted game content, recompiled game modules, DLC, translations,
console keys, firmware or personal saves are supplied. The launcher includes
user-supplied normal/DLC artwork described by its provider as redraws.
The runtime itself accepts extracted files. The optional Windows SwitchPrep
assistant prepares local NSP packages using hactool and keys supplied by the
user. XCI/NSZ/XCZ are not supported by this assistant. No keys are downloaded
or distributed. This feature is not a legal certification; see
[supported inputs and licensing](scripts/switchprep/README.md).

## Beta testers

Extract the complete beta ZIP into a writable folder and run `YKW4.exe`. Keep the
included DLLs, `langs`, licenses and source archive beside it. Select matching
ExeFS and RomFS folders with any desired update already applied. Initial setup
also accepts already-extracted DLC. Alternatively use **Import NSP...** in setup
or the update dialog: choose a source folder, your local keys, then a base game,
update and DLC. The assistant returns the prepared paths automatically.
Preparation requires additional disk space
and may take time. Cancelling setup should leave supplied originals unchanged.

Use Settings to update an existing prepared installation. Back up saves before
testing. Import translations as RomFS mods; no fan translation is bundled.
RAR/7z packages must be extracted before import. Code/ExeFS mods are not supported.

Share only the original beta ZIP. Once prepared, the installation contains game
files and derived modules and must not be redistributed as the launcher package.
See [beta testing](docs/BETA-TESTING.md) and [build instructions](BUILDING.md).

## Network and privacy

GameBanana requests occur when using its catalog or downloading a selected mod.
HTTPS downloads are restricted to GameBanana hosts. Mod files remain subject to
their authors' permissions; catalog availability does not grant redistribution
rights. No game files are uploaded by the catalog.

Discord Rich Presence is enabled by default and can be disabled in Settings.
It talks to the local Discord desktop client, which may display the configured
activity, text and session time to others according to your Discord settings.
No Discord token is requested. Context is user-configured: location and battle
detection are not implemented. The application ID is public, not a credential.
This launcher has no automatic crash-report or telemetry upload.

## License and attribution

The fork as a whole is **GPL-3.0-or-later**, with inherited Apache-2.0 portions
and separately licensed components. Preserve [LICENSE](LICENSE),
[LICENSE.apache](LICENSE.apache), [NOTICE](NOTICE) and component notices.

**SwitchRecomp by MZ Labs**, copyright 2026 MZ Labs.
Upstream projects and contributors are listed in [credits](docs/CREDITS.md)
and [source provenance](docs/SOURCE-PROVENANCE.json).
Launcher artwork is documented in [LAUNCHER-ARTWORK.md](dist/icons/LAUNCHER-ARTWORK.md).
**Project logo by Pabl06**, displayed in the launcher and on the website.
Wimsy fallback resources by Liz, GPL-3.0-or-later. See [credits](docs/CREDITS.md),
[license review](docs/LICENSE-REVIEW.md) and [local changes](docs/LOCAL-CHANGES.md).

This is an unofficial project and is not affiliated with Nintendo or LEVEL-5.
The source licenses grant no rights to game content or trademarks. Excluding
game files is a distribution precaution, not a certification of legal status.
