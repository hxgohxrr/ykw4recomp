// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2026 the ScarletNX contributors
package org.YKW4Recomp;

import android.Manifest;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Insets;
import android.media.MediaScannerConnection;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.PowerManager;
import android.provider.Settings;
import android.view.DisplayCutout;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import java.nio.charset.StandardCharsets;
import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;


public class MainActivity extends SDLActivity {

    private static final int ASKED_NOTIFICATIONS = 0x10000, ASKED_BATTERY = 0x10001, PICKED_FOLDER = 0x10002;

    @Override
    protected String[] getLibraries() {
        return new String[] {"main"};
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);

        if (Build.VERSION.SDK_INT >= 30) {
            if (!Environment.isExternalStorageManager())
                startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                                         Uri.parse("package:" + getPackageName())));
        } else if (Build.VERSION.SDK_INT >= 23 &&
                   checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED)
            requestPermissions(new String[] {Manifest.permission.WRITE_EXTERNAL_STORAGE}, 0);
    }


    public void play(String[] args) {
        startActivity(new Intent(this, GameActivity.class).putExtra("args", args));
    }


    public void toBack() {
        moveTaskToBack(true);
    }


    public void running(byte[] game, byte[] step, int percent) {
        String title = new String(game, StandardCharsets.UTF_8), text = new String(step, StandardCharsets.UTF_8);
        runOnUiThread(() -> {
            if (RecompileService.show(this, title, text, percent)) askBackground();
        });
    }


    public void ran(byte[] result) {
        String text = result != null ? new String(result, StandardCharsets.UTF_8) : null;
        runOnUiThread(() -> RecompileService.end(text));
    }

    @Override
    protected void onResume() {
        super.onResume();
        RecompileService.seen(this);
    }


    private void askBackground() {
        SharedPreferences asked = getSharedPreferences("asked", MODE_PRIVATE);
        String[] maker = Makers.limits();
        if (Build.VERSION.SDK_INT >= 33 &&
            checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED &&
            once(asked, "notifications"))
            requestPermissions(new String[] {Manifest.permission.POST_NOTIFICATIONS}, ASKED_NOTIFICATIONS);
        else if (Build.VERSION.SDK_INT >= 23 &&
                 !((PowerManager)getSystemService(POWER_SERVICE)).isIgnoringBatteryOptimizations(getPackageName()) &&
                 once(asked, "battery")) {
            try {
                startActivityForResult(new Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS,
                                                  Uri.parse("package:" + getPackageName())),
                                       ASKED_BATTERY);
            } catch (ActivityNotFoundException e) {
                askBackground();
            }
        } else if (maker != null && once(asked, "maker"))
            Makers.ask(this, maker);
    }


    private static boolean once(SharedPreferences asked, String what) {
        if (asked.getBoolean(what, false)) return false;
        asked.edit().putBoolean(what, true).apply();
        return true;
    }

    @Override
    public void onRequestPermissionsResult(int code, String[] permissions, int[] results) {
        if (code == ASKED_NOTIFICATIONS)
            askBackground();
        else
            super.onRequestPermissionsResult(code, permissions, results);
    }

    @Override
    protected void onActivityResult(int code, int result, Intent data) {
        super.onActivityResult(code, result, data);
        if (code == ASKED_BATTERY) askBackground();
        if (code == PICKED_FOLDER) {
            Uri tree = result == RESULT_OK && data != null ? data.getData() : null;
            folderPicked(tree != null ? tree.toString().getBytes(StandardCharsets.UTF_8) : null);
        }
    }


    public void pickFolder() {
        startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), PICKED_FOLDER);
    }


    private static native void folderPicked(byte[] uri);


    public void share(String path) {
        share(this, path);
    }


    static void share(Activity activity, String path) {
        Uri document = DataProvider.documentUri(activity, new java.io.File(path));
        if (document != null) {
            send(activity, document);
            return;
        }
        MediaScannerConnection.scanFile(activity, new String[] {path}, new String[] {"text/plain"}, (file, uri) -> {
            if (uri != null) send(activity, uri);
        });
    }

    private static void send(Activity activity, Uri uri) {
        Intent send = new Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_STREAM, uri);
        activity.startActivity(
            Intent.createChooser(send.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION), "YKW4Recomp log"));
    }


    public void openData() {
        Intent view = new Intent(Intent.ACTION_VIEW)
                          .setDataAndType(DataProvider.rootUri(this), "vnd.android.document/root")
                          .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        try {
            startActivity(view);
        } catch (ActivityNotFoundException e) {
            try {
                startActivity(view.setPackage("com.google.android.documentsui"));
            } catch (ActivityNotFoundException again) {
                startActivity(view.setPackage("com.android.documentsui"));
            }
        }
    }

    @Override
    protected SDLSurface createSDLSurface(Context context) {
        return clearOfBars(context);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideBars(mSurface);
    }


    static void hideBars(View view) {
        WindowInsetsController bars =
            Build.VERSION.SDK_INT >= 30 && view != null ? view.getWindowInsetsController() : null;
        if (mFullscreenModeActive && bars != null) bars.hide(WindowInsets.Type.systemBars());
    }


    static SDLSurface clearOfBars(Context context) {
        return new SDLSurface(context) {
            final Runnable hide = () -> hideBars(this);

            @Override
            public WindowInsets onApplyWindowInsets(View view, WindowInsets insets) {
                WindowInsets result = super.onApplyWindowInsets(view, insets);
                if (Build.VERSION.SDK_INT >= 30) {
                    Insets safe = insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
                    SDLActivity.onNativeInsetsChanged(safe.left, safe.right, safe.top, safe.bottom);

                    removeCallbacks(hide);
                    if (insets.isVisible(WindowInsets.Type.statusBars()) ||
                        insets.isVisible(WindowInsets.Type.navigationBars()))
                        postDelayed(hide, 2000);
                } else {
                    DisplayCutout cut = Build.VERSION.SDK_INT >= 28 ? insets.getDisplayCutout() : null;
                    SDLActivity.onNativeInsetsChanged(
                        Math.max(insets.getSystemWindowInsetLeft(), cut != null ? cut.getSafeInsetLeft() : 0),
                        Math.max(insets.getSystemWindowInsetRight(), cut != null ? cut.getSafeInsetRight() : 0),
                        Math.max(insets.getSystemWindowInsetTop(), cut != null ? cut.getSafeInsetTop() : 0),
                        Math.max(insets.getSystemWindowInsetBottom(), cut != null ? cut.getSafeInsetBottom() : 0));
                }
                return result;
            }
        };
    }
}
