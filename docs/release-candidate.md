# Windows release candidate: 2026-10-30

This candidate is prepared for review, not a public release or an updater rollout.
The exact source snapshot is shipped with the binary, including local changes
not yet represented by the repository's default branch.

## Changes in this candidate

- Native adaptive input guide with keyboard, Xbox, PlayStation, Switch and generic
  controller labels. It follows the effective bindings, not the button name alone.
- F6 toggles the guide and remembers the choice. Original HUD glyphs are unchanged.
- Discord IPC uses structured JSON events instead of searching arbitrary payload
  text for READY or ERROR. Text containing ERROR no longer causes false disconnects.
- English runtime presence stages, project website button and live F7 opt-out.
- Native setup and runtime no longer bind modules to hardware or Windows secrets.
  Earlier encrypted modules must be prepared again from the user's own files.
  The embedded helper
  must be rebuilt from the source snapshot rather than reused from an older beta.

## Known limitations

- A loading/device-loss failure was reported on an RX 6600. It is not resolved or
  established to be specific to one computer. The release owner has accepted this
  known issue for candidate preparation; no universal GPU compatibility is claimed.
- The guide is an overlay, not a replacement for the original game's HUD glyphs.
- Presence does not detect the current area, combat, story progress or original
  game menus. DLC artwork selection indicates installed DLC, not current content.
- Local RTX 3060 startup and synthetic tests are not a full playthrough, Radeon
  certification, physical-controller matrix, or clean second-PC preparation test.
- New native Windows preparations create normal, unbound DLL modules. The prior
  bound candidate is not converted automatically; see local-module-binding.md.

## Distribution boundary

Distribute only the reviewed launcher package and its corresponding sources and
licenses. Never distribute prepared installations, generated game code, game
archives, game keys, saves, caches, private configuration or diagnostic captures.
SHA-256 proves byte identity with the supplied manifest; it is not a signature,
malware certification or proof of authenticity if package and hashes are replaced
together. No signing certificate or SmartScreen reputation is claimed.
