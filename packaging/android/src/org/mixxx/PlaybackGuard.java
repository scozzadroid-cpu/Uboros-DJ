package org.mixxx;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.media.AudioAttributes;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Build;
import android.util.Log;

/**
 * Keeps the playback alive and at full level while Mixxx runs:
 * - foreground service with a partial wake lock (see KeepAliveService), so
 *   that the audio and the stem separation survive the screen turning off;
 * - the media volume goes to the maximum when the output is only USB: Android
 *   applies the volume in software to USB sound cards (half volume = -26 dB);
 * - audio focus requested for music, but never paused or ducked by it: a DJ
 *   must keep playing through notifications and calls; losses are only logged.
 */
public class PlaybackGuard {
    private static final String TAG = "MixxxGuard";
    private static AudioFocusRequest sFocus = null;
    private static AudioDeviceCallback sDeviceCallback = null;

    public static void start(final Activity activity) {
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(() -> install(activity));
    }

    public static void stop(Context context) {
        if (context == null) {
            return;
        }
        Context app = context.getApplicationContext();
        AudioManager am = (AudioManager) app.getSystemService(Context.AUDIO_SERVICE);
        if (am != null) {
            if (sFocus != null) {
                am.abandonAudioFocusRequest(sFocus);
                sFocus = null;
            }
            if (sDeviceCallback != null) {
                am.unregisterAudioDeviceCallback(sDeviceCallback);
                sDeviceCallback = null;
            }
        }
        app.stopService(new Intent(app, KeepAliveService.class));
    }

    private static void install(Activity activity) {
        final Context app = activity.getApplicationContext();
        try {
            Intent intent = new Intent(app, KeepAliveService.class);
            app.startForegroundService(intent);
        } catch (Exception e) {
            // Not allowed to start it (e.g. started from the background)
            Log.w(TAG, "Cannot start the keep-alive service: " + e);
        }
        final AudioManager am = (AudioManager) app.getSystemService(Context.AUDIO_SERVICE);
        if (am == null) {
            return;
        }
        maximizeVolumeIfUsbOnly(am);
        if (sDeviceCallback == null) {
            sDeviceCallback = new AudioDeviceCallback() {
                @Override
                public void onAudioDevicesAdded(AudioDeviceInfo[] added) {
                    maximizeVolumeIfUsbOnly(am);
                }

                @Override
                public void onAudioDevicesRemoved(AudioDeviceInfo[] removed) {
                }
            };
            am.registerAudioDeviceCallback(sDeviceCallback, null);
        }
        if (sFocus == null) {
            sFocus = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                    .setAudioAttributes(new AudioAttributes.Builder()
                            .setUsage(AudioAttributes.USAGE_MEDIA)
                            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                            .build())
                    .setWillPauseWhenDucked(false)
                    .setAcceptsDelayedFocusGain(false)
                    .setOnAudioFocusChangeListener(change ->
                            Log.i(TAG, "Audio focus change " + change + " (ignored: playback goes on)"))
                    .build();
            int result = am.requestAudioFocus(sFocus);
            Log.i(TAG, "Audio focus request: " + result);
        }
    }

    /** Media volume to the maximum if the only output device is USB. */
    private static void maximizeVolumeIfUsbOnly(AudioManager am) {
        try {
            boolean usb = false;
            boolean other = false;
            for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
                switch (d.getType()) {
                    case AudioDeviceInfo.TYPE_USB_DEVICE:
                    case AudioDeviceInfo.TYPE_USB_HEADSET:
                    case AudioDeviceInfo.TYPE_USB_ACCESSORY:
                        usb = true;
                        break;
                    case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER:
                    case AudioDeviceInfo.TYPE_BUILTIN_EARPIECE:
                    case AudioDeviceInfo.TYPE_TELEPHONY:
                    case AudioDeviceInfo.TYPE_BUS:
                    case AudioDeviceInfo.TYPE_REMOTE_SUBMIX:
                        // always present, not what the audio is routed to
                        break;
                    default:
                        other = true; // wired jack, Bluetooth, HDMI...
                }
            }
            if (usb && !other) {
                int max = am.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
                if (am.getStreamVolume(AudioManager.STREAM_MUSIC) != max) {
                    am.setStreamVolume(AudioManager.STREAM_MUSIC, max, 0);
                    Log.i(TAG, "USB output only: media volume set to the maximum");
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "Cannot set the media volume: " + e);
        }
    }
}
