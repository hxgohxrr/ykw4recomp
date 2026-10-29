# Yo-kai Watch 4 Recomp

A fan-made Windows project for playing Yo-kai Watch 4 on PC. The launcher prepares
your own game files locally, recompiles the game code, and runs it through a
modified SwitchRecomp / ScarletNX runtime. It also handles settings, updates,
RomFS mods, and translations.

The first public Windows beta is planned for **October 30, 2026**. We're preparing
a clean launcher package for that release. There are no public downloads yet;
the files will appear on the [releases page](https://github.com/hxgohxrr/ykw4recomp/releases)
when they're ready.

[Website](https://yw4.yo-kaiporters.com) · [Releases](https://github.com/hxgohxrr/ykw4recomp/releases) · [Report a problem](https://github.com/hxgohxrr/ykw4recomp/issues)

## Before you download

You need a 64-bit Windows PC, a Vulkan-capable GPU, and your own compatible game
dump. This isn't a game download: the launcher does not include the game, updates,
DLC, console keys, firmware, saves, or a precompiled game module. We can't provide
those files.

The current setup targets game title ID `010086C00AF7C000`. Don't assume another
edition or title ID will work. The launcher interface is available in English,
Spanish, and Japanese; changing it does not translate the game.

This is still a beta, not a finished PC port. The runtime retains interpretation
paths, and we haven't verified a complete playthrough or stable performance in
every area. There are no finished Android, iOS, or macOS releases.

## Getting started

Once the release is available:

1. Download the launcher ZIP from Releases and extract the whole archive into a
   new folder you can write to. Don't run it from inside the ZIP.
2. Open `YKW4Recomp.exe`. Keep the included libraries, language files, and other
   package contents beside it.
3. Use the setup window to select your own base-game NSP and local keys. Updates
   and DLC are optional inputs. If you already have extracted files, use the
   ExeFS/RomFS setup option instead.
4. Let preparation finish. It compiles code and packs game data on your PC, so
   allow extra disk space and some time for the first setup.
5. Open the launcher settings before playing if you want to change the display,
   controls, language, mods, or Discord activity.

The NSP assistant does not accept XCI, NSZ, or XCZ. ExeFS and RomFS must belong to
the same prepared game version. Back up your saves before testing an update or
changing a prepared installation.

## Mods and translations

Import a RomFS mod folder or ZIP through the Mods tab. If a download is RAR or 7z,
extract it first. When two enabled mods replace the same file, the one higher in
the list wins.

Translations are installed as mods; they aren't included with the launcher. A
translation may require a particular game update. Check its author's instructions
rather than assuming any patch will work. Code and ExeFS patches aren't supported
by the RomFS mod loader.

The optional GameBanana catalog lets you browse and select mods. It doesn't grant
permission to reupload someone else's work.

## Prepared installations

Share the original launcher ZIP, not your folder after setup. A prepared folder
contains game data and generated code, even if it started as a clean launcher.

The upcoming Windows launcher binds new preparations to the PC and Windows
installation that created them. Setup asks before doing this. The binding does
not upload hardware identifiers or require an activation account. Existing
unbound installations aren't silently converted.

Replacing a motherboard or reinstalling Windows can invalidate that binding. In
that case, prepare the game again from your own files in a new folder and handle
save migration separately. A GPU replacement alone is not part of the identity.
This is meant to discourage copying prepared installations, not to prove game
ownership or make copying impossible.

## Known issues

- Some scenes, effects, character names, and transitions still have rendering or
  compatibility problems.
- Battle stutter and uneven frame times remain under investigation. Stable
  60 FPS is not guaranteed.
- A device-loss crash during scene transitions has been reported on an RX 6600.
  Results vary between Radeon cards; a successful run on another card doesn't
  confirm a fix for that crash.
- Compatibility with every update, DLC, translation, and mod has not been tested.

Diagnostic builds are for testing. They may be much slower than normal builds,
and a workaround that avoids one crash is not necessarily a fix.

## Reporting a problem

Open an issue with your CPU, GPU, driver version, Windows version, launcher
version, and the steps that reproduce the problem. Include `YKW4.log`, or the
preparation log under `setup-work` if setup failed. Say whether mods were enabled
and which game update you used.

Review logs before posting: local paths may contain your Windows username. Don't
upload keys, game files, generated game modules, or personal saves to an issue.

The launcher has no automatic crash-report upload. Discord Rich Presence talks
to the local Discord client and can be disabled in Settings. GameBanana network
requests are made when using its catalog or downloading a selected mod.

## Credits

The runtime builds on **SwitchRecomp by MZ Labs** and
[ScarletNX](https://github.com/scarletnx/ScarletNX), including their contributors
and upstream dependencies. The Spanish translation option uses the work of
**REY3DS / YKW4_ES** and its contributors; translation patches remain separate
from this project.

The modified runtime is GPL-3.0-or-later, with inherited Apache-2.0 portions and
separately licensed dependencies. Keep the source and license notices that
accompany a launcher release. Those licenses do not grant rights to the game's
content or trademarks.

Yo-kai Watch belongs to LEVEL-5. This is an unofficial fan project, not an
official PC release, and is not affiliated with LEVEL-5 or Nintendo.
