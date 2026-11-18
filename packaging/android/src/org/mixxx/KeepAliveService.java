package org.mixxx;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.IBinder;
import android.os.PowerManager;
import android.util.Log;

/**
 * Foreground service that keeps the process alive and the CPU awake (partial
 * wake lock) while Mixxx plays or separates stems with the screen off or the
 * app in the background. Started and stopped by PlaybackGuard.
 */
public class KeepAliveService extends Service {
    private static final String TAG = "MixxxKeepAlive";
    private static final String CHANNEL = "mixxx_keepalive";
    private static final int ID = 4711;
    private PowerManager.WakeLock mWakeLock = null;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        NotificationManager nm = getSystemService(NotificationManager.class);
        nm.createNotificationChannel(new NotificationChannel(CHANNEL,
                "Playback", NotificationManager.IMPORTANCE_LOW));
        Intent open = getPackageManager().getLaunchIntentForPackage(getPackageName());
        Notification.Builder b = new Notification.Builder(this, CHANNEL)
                .setSmallIcon(getApplicationInfo().icon)
                .setContentTitle("Ubøros DJ")
                .setContentText("Playback and stems running")
                .setOngoing(true);
        if (open != null) {
            b.setContentIntent(PendingIntent.getActivity(this, 0, open,
                    PendingIntent.FLAG_IMMUTABLE));
        }
        try {
            startForeground(ID, b.build(), ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK);
        } catch (Exception e) {
            Log.w(TAG, "Cannot go foreground: " + e);
            stopSelf();
            return START_NOT_STICKY;
        }
        if (mWakeLock == null) {
            PowerManager pm = getSystemService(PowerManager.class);
            mWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "mixxx:keepalive");
            mWakeLock.setReferenceCounted(false);
            mWakeLock.acquire();
        }
        // The app is restarted by the user, not by the system
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        if (mWakeLock != null && mWakeLock.isHeld()) {
            mWakeLock.release();
        }
        mWakeLock = null;
        super.onDestroy();
    }
}
