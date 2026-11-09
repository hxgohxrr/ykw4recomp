# Extracted files only

This build accepts separate ExeFS and RomFS folders, already updated if needed.
No container extraction or decryption is included; keys and firmware are not requested.
The supplied main NSO is validated and copied unchanged. No automatic timing patch is applied.

Settings > Install update replaces only ExeFS, RomFS and the compiled module.
Saves, DLC, translations and settings remain in place. Identical executable inputs reuse the compiled module.
DLC selection is available during initial preparation. The update dialog preserves existing DLC.

Optional initial DLC input: a directory containing 16-hex-digit title directories with
`romfs/`, or files named `<title-id>.romfs`, containing decrypted RomFS images.
Only this game's DLC title range is accepted. Input files are preserved.

Only distribute the original launcher archive, never a prepared game directory.
This packaging policy is not a legal certification.
