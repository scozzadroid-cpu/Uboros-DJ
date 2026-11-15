package org.mixxx;

import android.app.Activity;
import android.app.Application;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.provider.Settings;
import android.util.Log;
import android.widget.Toast;

/**
 * Turns "Do Not Disturb" on while Mixxx is in the foreground, so that
 * notifications and their sounds do not end up in the mix, and restores the
 * previous state when Mixxx goes to the background or is closed.
 *
 * - Uses the normal DND mode (INTERRUPTION_FILTER_PRIORITY): the exceptions the
 *   user configured (alarms, starred contacts...) still get through.
 * - If DND is already on, it is left alone and not touched on exit.
 * - If the user changes DND while Mixxx runs, it is not restored on exit.
 * - If Mixxx is killed before restoring, the next start restores it.
 *   (MIUI kills apps swiped away from the recents without callbacks: that is
 *   why DND is restored as soon as Mixxx goes to the background.)
 * - Needs the "Do Not Disturb access" granted by the user in the system
 *   settings: the settings page is opened once, with an explanation.
 */
public class DoNotDisturb {
    private static final String TAG = "MixxxDnd";
    private static final String PREFS = "mixxx_dnd";
    // We turned DND on and have not restored it yet
    private static final String KEY_ACTIVE = "active";
    // Filter that was set before we turned DND on
    private static final String KEY_PREVIOUS = "previous";
    // The access settings page was already shown once
    private static final String KEY_ASKED = "asked";

    private static boolean sInstalled = false;

    /** Called once from native code at startup. */
    public static void start(final Activity activity, final boolean enabled) {
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(() -> install(activity, enabled));
    }

    private static void install(final Activity activity, boolean enabled) {
        final Context context = activity.getApplicationContext();
        // Left over by a previous run that was killed
        restore(context);
        if (!enabled || sInstalled) {
            return;
        }
        sInstalled = true;

        if (!hasAccess(context)) {
            SharedPreferences prefs = prefs(context);
            if (!prefs.getBoolean(KEY_ASKED, false)) {
                prefs.edit().putBoolean(KEY_ASKED, true).apply();
                Toast.makeText(context,
                        "Mixxx: allow \"Do Not Disturb\" access so that notifications "
                                + "stay silent while you play",
                        Toast.LENGTH_LONG).show();
                try {
                    activity.startActivity(new Intent(
                            Settings.ACTION_NOTIFICATION_POLICY_ACCESS_SETTINGS));
                } catch (Exception e) {
                    Log.w(TAG, "Cannot open the DND access settings: " + e);
                }
            }
        } else {
            enable(context);
        }

        activity.getApplication().registerActivityLifecycleCallbacks(
                new Application.ActivityLifecycleCallbacks() {
                    @Override
                    public void onActivityResumed(Activity a) {
                        // Also after coming back from the access settings page
                        if (a == activity) {
                            enable(context);
                        }
                    }

                    @Override
                    public void onActivityStopped(Activity a) {
                        // In the background (home, recent apps, screen locked):
                        // MIUI kills swiped-away apps without any callback, so
                        // restoring later could leave DND on after Mixxx is gone.
                        if (a == activity && !a.isChangingConfigurations()) {
                            restore(context);
                        }
                    }

                    @Override
                    public void onActivityDestroyed(Activity a) {
                        if (a == activity) {
                            restore(context);
                        }
                    }

                    @Override
                    public void onActivityCreated(Activity a, Bundle b) {}

                    @Override
                    public void onActivityStarted(Activity a) {}

                    @Override
                    public void onActivityPaused(Activity a) {}

                    @Override
                    public void onActivitySaveInstanceState(Activity a, Bundle b) {}
                });
    }

    /** Called from native code when Mixxx shuts down. */
    public static void stop(Context context) {
        if (context != null) {
            restore(context.getApplicationContext());
        }
    }

    private static boolean hasAccess(Context context) {
        NotificationManager nm = context.getSystemService(NotificationManager.class);
        return nm != null && nm.isNotificationPolicyAccessGranted();
    }

    private static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    private static synchronized void enable(Context context) {
        if (!hasAccess(context)) {
            return;
        }
        SharedPreferences prefs = prefs(context);
        if (prefs.getBoolean(KEY_ACTIVE, false)) {
            return;
        }
        NotificationManager nm = context.getSystemService(NotificationManager.class);
        int current = nm.getCurrentInterruptionFilter();
        if (current != NotificationManager.INTERRUPTION_FILTER_ALL) {
            // DND already on (or unknown): not ours to manage
            Log.i(TAG, "DND already active (filter " + current + "), left alone");
            return;
        }
        // Store before changing it, so a crash right after can be recovered
        prefs.edit()
                .putBoolean(KEY_ACTIVE, true)
                .putInt(KEY_PREVIOUS, current)
                .commit();
        nm.setInterruptionFilter(NotificationManager.INTERRUPTION_FILTER_PRIORITY);
        Log.i(TAG, "DND turned on while Mixxx runs");
    }

    private static synchronized void restore(Context context) {
        SharedPreferences prefs = prefs(context);
        if (!prefs.getBoolean(KEY_ACTIVE, false)) {
            return;
        }
        int previous = prefs.getInt(KEY_PREVIOUS, NotificationManager.INTERRUPTION_FILTER_ALL);
        prefs.edit().putBoolean(KEY_ACTIVE, false).commit();
        if (!hasAccess(context)) {
            return;
        }
        NotificationManager nm = context.getSystemService(NotificationManager.class);
        // Only undo our own change: the user may have changed it meanwhile
        if (nm.getCurrentInterruptionFilter()
                == NotificationManager.INTERRUPTION_FILTER_PRIORITY) {
            nm.setInterruptionFilter(previous);
            Log.i(TAG, "DND restored");
        } else {
            Log.i(TAG, "DND changed by the user meanwhile, not restored");
        }
    }

}
