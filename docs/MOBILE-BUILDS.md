# Experimental mobile builds

The desktop launcher includes **Mobile builds** (Windows: **Compilar móviles**).
It opens `tools/mobile_builder.py`, a Python 3.10+ / Tk assistant shared with
Linux. A complete source checkout is required; a prepared game folder is not
source. The helper is copied next to the runtime during build and installed by
CMake. Source-only users can run `python3 scripts/mobile_builder.py --gui`.
The assistant currently displays its detailed conditions in Spanish; the native
Windows entry is translated into Spanish, English and Japanese.

## What it does

- Android arm64: runs the existing `android-llvm.sh` recipe if target LLVM is
  absent, then `android-apk.sh`. Requires a POSIX build environment, Android SDK
  platform 36, an NDK, JDK 21 and a compatible Gradle 9 installation. The existing
  Android Gradle plugin version is pinned in `dist/android/build.gradle`.
- Windows Android builds run through a user-selected WSL distribution. Select
  Linux SDK/NDK paths (for example `/home/user/Android/Sdk`), not a Windows NDK:
  the SDK tools must run inside WSL. Windows drive paths are converted with
  `wslpath`; commands are argument vectors, not interpolated shell commands.
- iOS: rejects Windows and Linux/WSL. On macOS with Xcode, builds target LLVM
  if necessary, invokes the `ios` CMake preset, installs the native output and
  runs `apple-app.sh` with the selected iOS MoltenVK distribution. The resulting
  IPA does not acquire a distribution signing identity. This does not promise
  App Store acceptance, installability, JIT permission or gameplay compatibility.

The output directory must be empty or previously initialized by this assistant.
LLVM and package build folders must be separate. Build tools can download pinned
dependencies and consume substantial time, memory and disk space. The assistant
does not install SDKs, accept Android SDK license agreements, upload packages,
copy game data or request signing credentials or console keys.

The Compile button is disabled until the conditions are explicitly accepted.
Acceptance resets after each attempt and when the target changes. CLI builds
also require `--accept-terms` with the exact current version, printed by:

```sh
python3 scripts/mobile_builder.py --show-terms
python3 scripts/mobile_builder.py --target android --source "$PWD" \
  --output "$HOME/YKW4-mobile/android" --llvm "$HOME/YKW4-mobile/llvm" \
  --android-sdk "$HOME/Android/Sdk" --check
```

`--check` checks prerequisites and prints the command plan without accepting
conditions, creating packages or starting a build. Read the conditions before
invoking a real build with the printed acceptance version. Never treat a planned
command, a successful configure step or a stale file as a newly generated APK/IPA.

`mobile-build.log` and `mobile-build-result.json` are written in the dedicated
output directory. The result records the accepted conditions version, timestamp,
target, success/failure/cancellation and generated artifact paths. It is local
metadata, not tamper-proof legal evidence. Cancel terminates the owned build
process group; Windows forwards cancellation to the Linux helper in WSL.

## Responsibility and redistribution

The conditions explain responsibility for the user's own actions and the need
to have authorization for material used or shared. They explicitly **do not**
claim that clicking a checkbox transfers all legal liability to the user or
absolves developers of their own obligations. A disclaimer cannot guarantee that
outcome. Obtain jurisdiction-specific legal review before public distribution.

Do not redistribute game data, keys, protected assets or derived game modules
without permission. Preserve third-party notices and satisfy the applicable
source-code licenses, including corresponding-source requirements where relevant.
The helper builds runtime source only, not a redistributable game package.

References for legal review (not a certification):
- Spanish Civil Code, articles 1102 and 1255:
  https://www.boe.es/buscar/act.php?id=BOE-A-1889-4763
- GNU GPL v3, distribution and corresponding source:
  https://www.gnu.org/licenses/gpl-3.0.html
- Apple developer agreements and SDK terms:
  https://developer.apple.com/support/terms/
