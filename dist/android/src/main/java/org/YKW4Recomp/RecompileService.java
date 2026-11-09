// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2026 the ScarletNX contributors
package org.YKW4Recomp;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import android.util.Log;


public class RecompileService extends Service {
    private static final String CHANNEL = "recompile";
    private static final int RUNNING = 1, ENDED = 2;

    private static final long AWAKE_MS = 6 * 60 * 60 * 1000L;
    private static String game = "", step = "", result;
    private static int percent = -1;
    private static boolean running;
    private static RecompileService service;
    private PowerManager.WakeLock awake;
    private int lastStart;


    static boolean show(Context context, String game, String step, int percent) {
        RecompileService.game = game;
        RecompileService.step = step;
        RecompileService.percent = percent;
        if (running) {
            if (service != null) service.post();
            return false;
        }
        running = true;
        result = null;
        manager(context).cancel(ENDED);
        Intent intent = new Intent(context, RecompileService.class);
        try {
            if (Build.VERSION.SDK_INT >= 26)
                context.startForegroundService(intent);
            else
                context.startService(intent);
        } catch (RuntimeException e) {

            Log.w("YKW4Recomp", "no recompile service: " + e);
        }
        return true;
    }


    static void end(String result) {
        if (!running) return;
        running = false;
        RecompileService.result = result;
        if (service != null) service.finish();
    }


    static void seen(Context context) {
        manager(context).cancel(ENDED);
    }

    private static NotificationManager manager(Context context) {
        return (NotificationManager)context.getSystemService(NOTIFICATION_SERVICE);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (Build.VERSION.SDK_INT >= 26)
            manager(this).createNotificationChannel(
                new NotificationChannel(CHANNEL, "Recompile", NotificationManager.IMPORTANCE_LOW));
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        service = this;
        lastStart = startId;
        try {
            if (Build.VERSION.SDK_INT >= 29)
                startForeground(RUNNING, notification(true), ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
            else
                startForeground(RUNNING, notification(true));
        } catch (RuntimeException e) {

            Log.w("YKW4Recomp", "no recompile service: " + e);
            service = null;
            stopSelf();
            return START_NOT_STICKY;
        }
        if (awake == null) {
            awake = ((PowerManager)getSystemService(POWER_SERVICE))
                        .newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "YKW4Recomp:recompile");
            awake.setReferenceCounted(false);
        }
        awake.acquire(AWAKE_MS);
        if (!running) finish();  
        return START_NOT_STICKY;
    }


    @Override
    public void onTimeout(int startId, int type) {
        result = null;
        finish();
    }

    @Override
    public void onDestroy() {
        if (awake != null && awake.isHeld()) awake.release();
        if (service == this) service = null;
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void post() {
        manager(this).notify(RUNNING, notification(true));
    }

    private void finish() {
        if (awake != null && awake.isHeld()) awake.release();
        if (Build.VERSION.SDK_INT >= 24)
            stopForeground(STOP_FOREGROUND_REMOVE);
        else
            stopForeground(true);

        if (result != null) manager(this).notify(ENDED, notification(false));
        stopSelfResult(lastStart);
    }


    private Notification notification(boolean going) {
        Notification.Builder note =
            Build.VERSION.SDK_INT >= 26 ? new Notification.Builder(this, CHANNEL) : new Notification.Builder(this);
        Intent menu = new Intent(this, MainActivity.class)
                          .setAction(Intent.ACTION_MAIN)
                          .addCategory(Intent.CATEGORY_LAUNCHER)
                          .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        note.setSmallIcon(R.drawable.notification)
            .setContentTitle(game)
            .setContentText(going ? step : result)
            .setContentIntent(PendingIntent.getActivity(this, 0, menu,
                                                        Build.VERSION.SDK_INT >= 23 ? PendingIntent.FLAG_IMMUTABLE : 0))
            .setOngoing(going)
            .setAutoCancel(!going)
            .setOnlyAlertOnce(true)
            .setShowWhen(false);
        if (going) note.setProgress(100, Math.max(percent, 0), percent < 0);
        return note.build();
    }
}
