# Discord Rich Presence

Windows builds connect to the desktop Discord client's local IPC pipe only while
YKW4 is running. This does not require a token, account login or extra DLL.
Connection attempts and updates run on a separate worker; updates on a connection
are at most once every 15 seconds, with reconnection when Discord restarts. Closing the game closes
the connection and clears the activity. If Discord is closed, gameplay continues.

The native launcher's Discord tab saves `YKW4.discord.ini` next to the executable.
It offers enabled (default on), session elapsed time (default on), application ID,
main text, context, and image asset keys. Defaults:

```ini
[Discord]
enabled=1
show_time=1
application_id=1561797893252317345
details=Yo-kai Watch 4 Recomp
context=
normal_image=
dlc_image=
```

The activity automatically reports `Starting YKW4`, `Playing YKW4` after an image
has been presented, or `In the in-game menu` while the native F2 menu/touch editor
is open. These are runtime states, not inferred locations or battle/menu states
inside the original game. Optional custom context is appended, with UTF-8-safe
length limiting. A button opens the project website.

Press **F7** during the game to disable/enable activity immediately. Disabling
clears the activity and closes the pipe; enabling reconnects without restarting.
The choice is saved to `YKW4.discord.ini`. The input guide reports the configured
on/off choice, not proof that Discord has displayed the activity. Disabled startup
does not connect to Discord. A dormant worker remains available for F7.

Discord's displayed application name comes from its Developer Portal application,
which should be named **Yo-kai Watch 4 Recomp**. Any configured asset keys must already exist
in that application's Rich Presence assets. Any installed YKW4 DLC detected by
the same filesystem index used by the game's add-on service selects `dlc_image`.
This indicates installation, not that the current scene uses DLC content.

The timestamp measures this running session, not lifetime saved playtime. Context
is user supplied. Automatic current-area and battle/menu detection is not yet
implemented: no stable, version-validated game-state source has been identified.
Do not infer the player's current location from the last opened asset, which may
have been preloaded for another scene. No character name or save contents are sent.

Launcher field edits apply at the next game start; F7 and runtime states apply live.
The launcher, setup and CLI recompiler do not
publish presence. Users must also have Discord activity sharing enabled.

Protocol reference: https://discord.com/developers/docs/topics/rpc

`tests/discord_presence_win32.c` exercises a private fake Discord pipe: handshake,
ping/pong, activity payload, DLC image choice, escaped custom text, optional session
timestamps, structured JSON events (including non-error text containing ERROR),
live runtime stages, clear/re-enable/reconnect, Unicode, disabled startup and
shutdown without Discord. It never connects to
the user's Discord client.

The extracted-only distribution uses no default Discord artwork. Supply original artwork keys in the launcher if desired. No game logos are bundled.
