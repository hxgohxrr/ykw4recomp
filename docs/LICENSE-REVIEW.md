# License and content review

Review date: 2026-10-25. This records the scope of a source/package audit, not a
legal opinion or a guarantee against claims.

| Included component | Terms and notices |
| --- | --- |
| ScarletNX fork and launcher | GPL-3.0-or-later, `LICENSE`, source headers |
| Inherited SwitchRecomp code | Apache-2.0, `LICENSE.apache`, `NOTICE` and source notices |
| SwitchPrep Python assistant | GPL-3.0-or-later, `scripts/switchprep/LICENSE` |
| hactool backend | ISC with ScarletNX GPL-3.0-or-later adaptations; LibHac MIT notices; linked OpenSSL Apache 2.0 and LZ4 BSD notices in binary package |
| Frozen assistant runtime | Python PSF and PyInstaller bootloader exception; notices in binary package |
| cJSON | MIT, `src/nx/runtime/vendor/cjson/LICENSE` |
| miniz | MIT, `src/nx/runtime/vendor/miniz/LICENSE` |
| Launcher normal/DLC artwork | User-supplied redraws; see `dist/icons/LAUNCHER-ARTWORK.md` |
| Wimsy fallback icon | Liz, GPL-3.0-or-later, `dist/wimsy.svg` metadata |
| Touch controls | GPL notices in `dist/controls/README.md` |
| Other ported source | Original per-file copyright/license notices and `docs/CREDITS.md` |

Dependencies fetched at build time retain their own licenses. They include LLVM,
libc++, OpenSSL, SDL3, LZ4, FFmpeg, Vulkan-Headers, volk and VMA. Source-only Git
publication does not include their compiled libraries. Binary distribution must
carry the actual build's dependency license texts and required source materials;
a source URL alone is not automatically sufficient for every distribution mode.

Modified upstream source files are marked and the provenance record identifies
the base. The GPL license text and inherited Apache NOTICE remain intact. Do not
relicense the combined fork as MIT or Apache-only, remove upstream credits, or
impose a noncommercial restriction inconsistent with those source licenses.

No game files, firmware, keys or fan translations are included in the publication
tree. The provider requested the inclusion of normal/DLC logo redraws. Their
underlying third-party rights and originality have not been independently verified. Input title IDs and format
validation logic do not supply the user's game. Names refer to compatibility and
do not imply trademark permission, affiliation or endorsement. Mods downloaded
by users are not licensed by this repository.

References: https://www.gnu.org/licenses/gpl-3.0.html and
https://www.apache.org/licenses/LICENSE-2.0 .


The optional preparation assistant can decrypt user-supplied NSP content using locally supplied keys. Open-source component licensing does not determine whether a particular dump or circumvention is lawful in a user jurisdiction. No legal clearance is claimed.
