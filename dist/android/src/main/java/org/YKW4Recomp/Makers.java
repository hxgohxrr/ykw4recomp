// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2026 the ScarletNX contributors
package org.YKW4Recomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import java.util.Locale;


final class Makers {
    private static final String OPPO = "Battery usage: allow background activity and auto launch.";
    private static final String HUAWEI = "App launch: Manage manually, all on.";
    private static final String TRANSSION = "Phone Master, Auto-start: on.";

    private static final String[][] LIMITS = {
        {"samsung", "Battery, Background usage limits: add YKW4Recomp to Never sleeping apps.",
         "com.samsung.android.lool", "com.samsung.android.sm.battery.ui.BatteryActivity",
         "com.samsung.android.lool", "com.samsung.android.sm.ui.battery.BatteryActivity"},
        {"xiaomi", "Autostart on, and Battery saver: No restrictions."},
        {"oppo", OPPO},
        {"realme", OPPO},
        {"oneplus", OPPO},
        {"vivo", "Autostart on, and high background power use allowed.",
         "com.iqoo.secure", "com.iqoo.secure.ui.phoneoptimize.AddWhiteListActivity",
         "com.vivo.permissionmanager", "com.vivo.permissionmanager.activity.BgStartUpManagerActivity",
         "com.iqoo.secure", "com.iqoo.secure.ui.phoneoptimize.BgStartUpManager"},
        {"huawei", HUAWEI,
         "com.huawei.systemmanager", "com.huawei.systemmanager.startupmgr.ui.StartupNormalAppListActivity",
         "com.huawei.systemmanager", "com.huawei.systemmanager.optimize.process.ProtectActivity"},
        {"honor", HUAWEI,
         "com.hihonor.systemmanager", "com.hihonor.systemmanager.startupmgr.ui.StartupNormalAppListActivity",
         "com.huawei.systemmanager", "com.huawei.systemmanager.optimize.process.ProtectActivity"},
        {"asus", "Auto-start: allow YKW4Recomp.",
         "com.asus.mobilemanager", "com.asus.mobilemanager.autostart.AutoStartActivity",
         "com.asus.mobilemanager", "com.asus.mobilemanager.powersaver.PowerSaverSettings"},
        {"tecno", TRANSSION, "com.transsion.phonemaster", "com.cyin.himgr.autostart.AutoStartActivity"},
        {"infinix", TRANSSION, "com.transsion.phonemaster", "com.cyin.himgr.autostart.AutoStartActivity"},
        {"itel", TRANSSION, "com.transsion.phonemaster", "com.cyin.himgr.autostart.AutoStartActivity"},
    };

    private Makers() {}


    static String[] limits() {
        String maker = (Build.MANUFACTURER + " " + Build.BRAND).toLowerCase(Locale.ROOT);
        for (String[] row : LIMITS)
            if (maker.contains(row[0])) return row;
        return null;
    }


    static void ask(Activity activity, String[] row) {
        new AlertDialog.Builder(activity, android.R.style.Theme_Material_Dialog_Alert)
            .setMessage("This phone may stop a recompile behind other apps. In Settings: " + row[1])
            .setPositiveButton("Settings", (dialog, button) -> open(activity, row))
            .setNegativeButton("Not now", null)
            .show();
    }

    private static void open(Activity activity, String[] row) {
        for (int i = 2; i + 1 < row.length; i += 2) {
            try {
                activity.startActivity(new Intent().setClassName(row[i], row[i + 1]));
                return;
            } catch (ActivityNotFoundException | SecurityException e) {

            }
        }
        try {
            activity.startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                                              Uri.parse("package:" + activity.getPackageName())));
        } catch (ActivityNotFoundException e) {

        }
    }
}
