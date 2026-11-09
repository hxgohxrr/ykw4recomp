/* SwitchRecomp - Copyright 2026 MZ Labs
   Licensed under the Apache License 2.0. See LICENSE.apache and NOTICE;
   any copy or derived work must keep the credit to SwitchRecomp by MZ Labs.
   This fork's changes are under GPL-3.0-or-later, the license of the fork as a whole (LICENSE). */

#ifndef SNX_SERVICES_H
#define SNX_SERVICES_H
#include "nx/hle/kernel/ipc.h"

void svc_core_init(void);  
void svc_fs_init(void);
void svc_hid_init(void);
void svc_vi_init(void);
void svc_pl_init(void);        
int set_language_count(void);  

#endif
