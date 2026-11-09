# Local changes

Snapshot prepared on 2026-10-24; publication review continued on 2026-10-25.
See `SOURCE-PROVENANCE.json` for the inherited ScarletNX base commit and list of
modified upstream files. Original license and copyright notices are preserved.

The local fork adds the native Windows launcher, extracted-file preparation,
update publication, ZIP/folder mod management, GameBanana catalog, launcher
localization and configurable Discord IPC presence. It includes earlier local
runtime fixes and an adapted upstream pending GPU result pages backport
`1372b67ad82926579b7da8525097e65cf29f5cb3`.

The runtime snapshot excludes its original container extractor, game-derived
logos, local toolchains, build output, runtime caches and private development
history. The subsequently added SwitchPrep assistant calls a separate hactool
backend for user-selected NSP preparation; its source and build recipe are in
`scripts/switchprep/`. The extracted-only and distributable options are enforced.
The initial generic-icon configuration was replaced at the provider's request by the supplied normal/DLC redraws. Wimsy fallback resources retain their credits. This snapshot does not
include the separate fb21 experimental profiling/optimization branch.

Publication-specific changes: source/content audit, explicit Windows build preset,
launcher/license documentation, issue templates, dependency notice installation
and removal of a hardcoded local Perl path in the OpenSSL build recipe.


ZIP safety review: reject console device aliases and superscript COM/LPT names; native extraction tested with synthetic traversal, duplicate, device and symlink entries.
Optional explicitly version-checked OpenSSL/FFmpeg installs can be used for local builds; source builds remain the default.

Developer-context links and unused local editor hooks were removed from the publication tree. Original copyright and attribution notices remain intact.

Normal/DLC icons are embedded at Windows sizes and selected from installed DLC markers; update-only content does not select the DLC icon.

SwitchPrep integration adds asynchronous console-helper launch, UTF-16 prepared-path handoff, metadata-based title selection, optional base/update/DLC preparation, and transactional DLC archive publication during updates. Python helper source is GPL-3.0-or-later; backend build recipes retain GPL notices.

Product branding changed to YKW4Recomp on 2026-10-25: executable and bundle names, Android/JNI identifiers, log filenames, data paths, command help and window titles. Original copyrights, upstream URLs and attribution documents remain unchanged. Existing externally installed Android apps use a different application ID; their data is not migrated automatically.

Native NSP setup (2026-10-25): default native file selectors for the base, optional update and multiple DLC NSPs, plus local key selection. A noninteractive helper request runs under the existing cancellable worker/job, publishes prepared paths, and continues the existing installation transaction. Progress and errors are retained in the attempt log, with an Open log button. NCA-only key filtering avoids unrelated malformed hardware derivation entries aborting hactool. Extracted-folder import is retained.

The native NSP path keeps validated RomFS/DLC images and hands them directly to installation. It avoids unpacking every RomFS file and repacking the same data again. The standalone interactive extraction mode still writes folders.
# V.0.1 Early Access - 2026-10-30

Vertex input corrections (2026-10-31): collect the maximum beyond-stride padding
of all attributes sharing a binding before uploading that stream. Widen 8-bit
restart indices only when restart is active for the selected topology, preserving
ordinary list indices even if the guest restart register is enabled. Synthetic
old/new regressions reproduce both defects in the old renderer. Nine focused
tests pass, including headless Vulkan on RTX 3060. This does not establish that
the reported intermittent game geometry or Radeon DEVICE_LOST is fixed.

XCI input support (2026-10-31): SwitchPrep reads the game-card root/secure HFS0
directly and preserves selected NCA bytes, including game-card headers. Native
selectors accept NSP/XCI for base, update and DLC. Bundled optional titles remain
explicit selections; a separate update NSP can replace the bundled update.
Container bounds, names and overlaps are checked; NCA/CNMT hashes remain required.
Compressed and split dumps are not supported. Synthetic regressions cover mixed
titles, shared-container selection, duplicate metadata across separate packages,
unchanged input bytes, hash rejection and malformed partitions. Real XCI
decryption/installation still needs verification with a user-supplied dump.

Native Windows setup no longer seals generated modules or queries SMBIOS identity.
The runtime uses its ordinary platform DLL loader. The machine-binding and
identity implementations are not linked into the production executable; their
earlier source and isolated regression targets remain for provenance only.
Earlier bound modules are not migrated automatically. The runtime smoke test can
relocate a synthetic prepared installation before loading it (--relocate).
