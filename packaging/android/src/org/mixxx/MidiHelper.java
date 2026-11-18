package org.mixxx;

import android.content.Context;
import android.content.pm.PackageManager;
import android.hardware.usb.UsbDevice;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import java.io.IOException;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/// Helper used by the native AMidi backend (AndroidMidiEnumerator and
/// AndroidMidiController) to enumerate and open MIDI devices through
/// android.media.midi.MidiManager. All methods are static and may be called
/// from any thread attached to the JVM, except the Android main thread for
/// openDevice() which blocks until the device has been opened.
public class MidiHelper {
    /** Why the previous processes of this app ended (API 30+), one per line. */
    public static String getExitReasons(Context context) {
        if (Build.VERSION.SDK_INT < 30) {
            return "";
        }
        try {
            android.app.ActivityManager am =
                    (android.app.ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            StringBuilder sb = new StringBuilder();
            for (android.app.ApplicationExitInfo info :
                    am.getHistoricalProcessExitReasons(null, 0, 4)) {
                String reason;
                switch (info.getReason()) {
                    case android.app.ApplicationExitInfo.REASON_SIGNALED: reason = "SIGNALED"; break;
                    case android.app.ApplicationExitInfo.REASON_LOW_MEMORY: reason = "LOW_MEMORY (lmkd)"; break;
                    case android.app.ApplicationExitInfo.REASON_CRASH: reason = "CRASH (Java)"; break;
                    case android.app.ApplicationExitInfo.REASON_CRASH_NATIVE: reason = "CRASH_NATIVE"; break;
                    case android.app.ApplicationExitInfo.REASON_ANR: reason = "ANR"; break;
                    case android.app.ApplicationExitInfo.REASON_EXIT_SELF: reason = "EXIT_SELF"; break;
                    case android.app.ApplicationExitInfo.REASON_USER_REQUESTED: reason = "USER_REQUESTED"; break;
                    case android.app.ApplicationExitInfo.REASON_USER_STOPPED: reason = "USER_STOPPED"; break;
                    case android.app.ApplicationExitInfo.REASON_PACKAGE_UPDATED: reason = "PACKAGE_UPDATED"; break;
                    case android.app.ApplicationExitInfo.REASON_OTHER: reason = "OTHER"; break;
                    default: reason = "reason " + info.getReason(); break;
                }
                sb.append(new java.util.Date(info.getTimestamp())).append(" pid ")
                        .append(info.getPid()).append(" ").append(reason)
                        .append(" status ").append(info.getStatus())
                        .append(" rss ").append(info.getRss() / 1024 / 1024).append("MB")
                        .append(" ").append(info.getDescription()).append("\n");
            }
            return sb.toString();
        } catch (Throwable t) {
            return "exit reasons unavailable: " + t;
        }
    }

    private static final String TAG = "MixxxMidiHelper";

    private static HandlerThread sHandlerThread = null;
    private static Handler sHandler = null;

    // Incremented whenever a MIDI device is added or removed. Polled by the
    // native side to rescan controllers (hotplug).
    private static volatile int sDeviceGeneration = 0;
    private static boolean sCallbackRegistered = false;

    private static synchronized void ensureDeviceCallback(MidiManager midiManager) {
        if (sCallbackRegistered || midiManager == null) {
            return;
        }
        MidiManager.DeviceCallback callback = new MidiManager.DeviceCallback() {
            @Override
            public void onDeviceAdded(MidiDeviceInfo info) {
                sDeviceGeneration++;
                Log.i(TAG, "MIDI device added: " + getName(info));
            }

            @Override
            public void onDeviceRemoved(MidiDeviceInfo info) {
                sDeviceGeneration++;
                Log.i(TAG, "MIDI device removed: " + getName(info));
            }
        };
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                midiManager.registerDeviceCallback(MidiManager.TRANSPORT_MIDI_BYTE_STREAM,
                        new java.util.concurrent.Executor() {
                            @Override
                            public void execute(Runnable r) {
                                getHandler().post(r);
                            }
                        },
                        callback);
            } else {
                midiManager.registerDeviceCallback(callback, getHandler());
            }
            sCallbackRegistered = true;
        } catch (Exception e) {
            Log.w(TAG, "Unable to register MIDI device callback: " + e.toString());
        }
    }

    /// Returns a counter that changes whenever MIDI devices are added or
    /// removed. Registers the device callback on first use.
    public static int getDeviceGeneration(Context context) {
        ensureDeviceCallback(getMidiManager(context));
        return sDeviceGeneration;
    }

    private static synchronized Handler getHandler() {
        if (sHandler == null) {
            sHandlerThread = new HandlerThread("MixxxMidiHelper");
            sHandlerThread.start();
            sHandler = new Handler(sHandlerThread.getLooper());
        }
        return sHandler;
    }

    private static MidiManager getMidiManager(Context context) {
        if (context == null) {
            Log.w(TAG, "No context available");
            return null;
        }
        if (!context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_MIDI)) {
            Log.w(TAG, "This device does not support android.software.midi");
            return null;
        }
        return (MidiManager) context.getSystemService(Context.MIDI_SERVICE);
    }

    /// Returns all MIDI 1.0 byte stream devices currently known to the system.
    public static MidiDeviceInfo[] getDevices(Context context) {
        MidiManager midiManager = getMidiManager(context);
        if (midiManager == null) {
            return new MidiDeviceInfo[0];
        }
        ensureDeviceCallback(midiManager);
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                Set<MidiDeviceInfo> devices =
                        midiManager.getDevicesForTransport(MidiManager.TRANSPORT_MIDI_BYTE_STREAM);
                return devices.toArray(new MidiDeviceInfo[0]);
            }
            return midiManager.getDevices();
        } catch (Exception e) {
            Log.w(TAG, "Unable to list MIDI devices: " + e.toString());
            return new MidiDeviceInfo[0];
        }
    }

    public static int getId(MidiDeviceInfo info) {
        return info.getId();
    }

    /// One of MidiDeviceInfo.TYPE_USB (1), TYPE_VIRTUAL (2), TYPE_BLUETOOTH (3)
    public static int getType(MidiDeviceInfo info) {
        return info.getType();
    }

    /// Number of ports the app can write to (data sent to the device)
    public static int getInputPortCount(MidiDeviceInfo info) {
        return info.getInputPortCount();
    }

    /// Number of ports the app can read from (data received from the device)
    public static int getOutputPortCount(MidiDeviceInfo info) {
        return info.getOutputPortCount();
    }

    private static String getStringProperty(MidiDeviceInfo info, String key) {
        Bundle properties = info.getProperties();
        if (properties == null) {
            return "";
        }
        String value = properties.getString(key);
        return value != null ? value : "";
    }

    public static String getName(MidiDeviceInfo info) {
        String name = getStringProperty(info, MidiDeviceInfo.PROPERTY_NAME);
        if (name.isEmpty()) {
            name = getStringProperty(info, MidiDeviceInfo.PROPERTY_PRODUCT);
        }
        if (name.isEmpty()) {
            name = "MIDI device " + info.getId();
        }
        return name;
    }

    public static String getManufacturer(MidiDeviceInfo info) {
        return getStringProperty(info, MidiDeviceInfo.PROPERTY_MANUFACTURER);
    }

    public static String getProduct(MidiDeviceInfo info) {
        return getStringProperty(info, MidiDeviceInfo.PROPERTY_PRODUCT);
    }

    public static String getSerialNumber(MidiDeviceInfo info) {
        return getStringProperty(info, MidiDeviceInfo.PROPERTY_SERIAL_NUMBER);
    }

    /// Name of the given port, or an empty string.
    /// @param portType MidiDeviceInfo.PortInfo.TYPE_INPUT (1) or TYPE_OUTPUT (2)
    public static String getPortName(MidiDeviceInfo info, int portType, int portNumber) {
        MidiDeviceInfo.PortInfo[] ports = info.getPorts();
        if (ports == null) {
            return "";
        }
        for (MidiDeviceInfo.PortInfo port : ports) {
            if (port.getType() == portType && port.getPortNumber() == portNumber) {
                String name = port.getName();
                return name != null ? name : "";
            }
        }
        return "";
    }

    @SuppressWarnings("deprecation")
    private static UsbDevice getUsbDevice(MidiDeviceInfo info) {
        Bundle properties = info.getProperties();
        if (properties == null) {
            return null;
        }
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                return properties.getParcelable(MidiDeviceInfo.PROPERTY_USB_DEVICE, UsbDevice.class);
            }
            Object device = properties.getParcelable(MidiDeviceInfo.PROPERTY_USB_DEVICE);
            if (device instanceof UsbDevice) {
                return (UsbDevice) device;
            }
        } catch (Exception e) {
            Log.w(TAG, "Unable to get USB device: " + e.toString());
        }
        return null;
    }

    /// USB vendor ID, or -1 if this is not a USB device
    public static int getUsbVendorId(MidiDeviceInfo info) {
        UsbDevice device = getUsbDevice(info);
        return device != null ? device.getVendorId() : -1;
    }

    /// USB product ID, or -1 if this is not a USB device
    public static int getUsbProductId(MidiDeviceInfo info) {
        UsbDevice device = getUsbDevice(info);
        return device != null ? device.getProductId() : -1;
    }

    private static final class OpenRequest implements MidiManager.OnDeviceOpenedListener {
        private final CountDownLatch mLatch = new CountDownLatch(1);
        private MidiDevice mDevice = null;
        private boolean mAbandoned = false;

        @Override
        public void onDeviceOpened(MidiDevice device) {
            boolean closeDevice = false;
            synchronized (this) {
                if (mAbandoned) {
                    closeDevice = true;
                } else {
                    mDevice = device;
                }
            }
            if (closeDevice && device != null) {
                Log.w(TAG, "MIDI device opened after timeout, closing it");
                closeDevice(device);
            }
            mLatch.countDown();
        }

        MidiDevice await(long timeoutMs) {
            try {
                mLatch.await(timeoutMs, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
            synchronized (this) {
                if (mDevice == null) {
                    mAbandoned = true;
                }
                return mDevice;
            }
        }
    }

    /// Opens the device and blocks until it is opened, failed or timed out.
    /// Returns null on failure. Must not be called from the Android main thread.
    public static MidiDevice openDevice(Context context, MidiDeviceInfo info, int timeoutMs) {
        MidiManager midiManager = getMidiManager(context);
        if (midiManager == null || info == null) {
            return null;
        }
        OpenRequest request = new OpenRequest();
        try {
            midiManager.openDevice(info, request, getHandler());
        } catch (Exception e) {
            Log.w(TAG, "Unable to open MIDI device: " + e.toString());
            return null;
        }
        MidiDevice device = request.await(timeoutMs);
        if (device == null) {
            Log.w(TAG, "Failed to open MIDI device " + getName(info));
        } else {
            Log.i(TAG, "Opened MIDI device " + getName(info));
        }
        return device;
    }

    public static void closeDevice(MidiDevice device) {
        if (device == null) {
            return;
        }
        try {
            device.close();
        } catch (IOException e) {
            Log.w(TAG, "Unable to close MIDI device: " + e.toString());
        }
    }
}
