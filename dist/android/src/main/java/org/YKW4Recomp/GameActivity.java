// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2026 the ScarletNX contributors
package org.YKW4Recomp;

import android.content.Context;
import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;


public class GameActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"YKW4Recomp"};
    }

    @Override
    protected String getMainFunction() {
        return "main";
    }

    @Override
    protected SDLSurface createSDLSurface(Context context) {
        return MainActivity.clearOfBars(context);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) MainActivity.hideBars(mSurface);
    }

    @Override
    protected String[] getArguments() {
        String[] args = getIntent().getStringArrayExtra("args");
        return args != null ? args : new String[0];
    }


    public void share(String path) {
        MainActivity.share(this, path);
    }


    @Override
    protected void onDestroy() {
        super.onDestroy();
        System.exit(0);
    }
}
