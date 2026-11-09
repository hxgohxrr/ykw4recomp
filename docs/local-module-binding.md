# Local module binding (current development build)

The current source integrates binding into preparation and the actual runtime
module loader on Windows and desktop Linux. This does not alter already
published V.0.1 Early Access binaries, which did not enable binding, or any
previously delivered installations. Rebuild to obtain the behavior described here.

## Consent and compatibility

- New launcher preparations require explicit acceptance before writes and bind
  the compiled module locally. Declining cancels preparation.
- Existing ordinary AMD64 DLLs on Windows and ELF modules on Linux remain
  unbound when opened or rebuilt over. There is no silent legacy conversion.
- The Equipo tab can explicitly bind an existing installation after confirmation.
- Already bound modules stay bound on preparation/update. Invalid or unreadable
  modules fail closed. Preparation stages output before publication.
- Developer compilation commands are not automatically restricted by this policy.
- DLLs and ELF modules are different platform builds: copying or renaming a
  Windows DLL does not turn it into a Linux module.

Only the generated module is protected. Saves, RomFS, source inputs, compiler
intermediates and keys are not protected by this feature and must not be
redistributed as if encryption covered the entire installation. No identifier,
key, activation request or game file is uploaded by the binding feature.

## Windows

The SMBIOS system UUID is hashed with a feature-specific domain and used as
additional entropy for machine-scope Windows DPAPI. The versioned envelope
contains protected bytes, not a directly loadable DLL. Invalid/missing firmware
UUIDs prevent creating a new bound module; there is no unprotected fallback.

Loading decrypts into a random owner-only temporary DLL, with restricted DLL
dependency lookup and a read/delete handle held until process exit. Normal
cleanup deletes it. Process crashes, storage forensics and privileged local
access are outside this protection's guarantees. Whole-module encryption and
decryption need multiple module-sized buffers; the envelope limit is 1 GiB.

## Linux and WSL

The identity source is SMBIOS when readable and valid, otherwise the local
`/etc/machine-id` (including unprivileged Linux/WSL). The selected source is
pinned in the private-key header: later loss of that source fails instead of
silently switching identities.

`$HOME/.ykw4-binding/key-v2` is a 64-byte versioned local key containing the
identity-source choice and a random secret. The directory must be owned by
the current user with mode 0700 and the key with mode 0600; symlink and unsafe
permission checks reject unsafe storage. Do not copy this key or replace
`HOME` casually: Linux binding includes the user-local secret, not just hardware.

SHA-256 combines the secret and machine digest to derive the AES-256-GCM key.
The version-2 Linux envelope authenticates the encrypted module. Decryption
loads through a sealed anonymous memfd, not a plaintext file in a disk temp
directory. This requires Linux memfd/sealing support and `/proc/self/fd` access.
Android and Apple loaders are not changed to use this desktop binding.

## Recovery and limits

Motherboard/UUID changes, OS reinstallation, changed machine-id, missing private
keys, or lost Windows machine secrets can invalidate binding. WSL distributions
and native Linux installations are not promised to share an identity. GPU
replacement alone is not an identity input. There is no reset server or expiry.

If binding breaks, prepare again from your own authorized files in a new
folder and migrate backed-up saves separately. Do not delete an old installation
as a troubleshooting step. The binding loader does not modify saves or system
policies.

This deters casual copying, not a privileged owner of the original computer.
It is not ownership verification, anti-cheat or unbreakable DRM. Process memory
and Windows decrypted temporary modules remain inspectable locally; developers
can rebuild the open-source loader. Binding does not change source licenses or
the rights they grant.

## Validation

Synthetic DLL/ELF tests cover legacy loading, sealing, same-identity reload,
changed-identity rejection, corruption and cleanup. Linux tests also cover key
loss, symlinks and unsafe permissions. The runtime integration test exercises
the actual platform loader, consent enforcement, RomFS packing and title-ID
validation without game content. Identity tests do not print raw identifiers.

A real second computer, full game-sized preparation, gameplay and hardware
replacement remain separate checks. Synthetic tests do not resolve renderer
issues or prove universal hardware compatibility.
