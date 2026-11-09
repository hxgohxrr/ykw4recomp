/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef YKW4_DISCORD_PRESENCE_H
#define YKW4_DISCORD_PRESENCE_H
#ifdef _WIN32
void discord_presence_start(const char* directory, int dlc);
void discord_presence_stop(void);
void discord_presence_update(int menu, int rendered);
int discord_presence_toggle(void);
int discord_presence_enabled(void);
int discord_presence_connected(void);
#endif
#endif
