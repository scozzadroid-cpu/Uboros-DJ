package org.mixxx;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

/**
 * Optional home-screen entry ("launcher") for a dedicated DJ phone.
 *
 * It is declared disabled in the manifest, so it changes nothing on normal
 * installs. A rooted helper (the "DJ mode" Magisk module) enables it and makes
 * it the default home activity while a controller is connected.
 *
 * It is only a trampoline: pressing HOME brings the existing Mixxx task to the
 * front (or starts Mixxx), exactly like tapping the launcher icon, and then it
 * finishes. Mixxx itself therefore never runs in the system's home task, so
 * there is never a second QtActivity instance in the process.
 */
public class DjHome extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Intent launch = getPackageManager().getLaunchIntentForPackage(getPackageName());
        if (launch != null) {
            launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                    | Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);
            startActivity(launch);
        }
        finish();
        overridePendingTransition(0, 0);
    }
}
