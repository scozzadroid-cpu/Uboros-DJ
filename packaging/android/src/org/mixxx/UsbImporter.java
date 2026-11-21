package org.mixxx;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.StatFs;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.List;

import me.jahnen.libaums.core.UsbMassStorageDevice;
import me.jahnen.libaums.core.fs.FileSystem;
import me.jahnen.libaums.core.fs.UsbFile;
import me.jahnen.libaums.core.partition.Partition;

/**
 * Copies the rekordbox export of USB drives (PIONEER/ and Contents/), or the
 * audio files of a drive without one (in their folders), to the
 * internal storage, without root and without the drive being mounted by
 * Android (e.g. FAT32 drives without a partition table, which vold ignores).
 *
 * The drive is accessed with the USB host API (the user grants the access)
 * and libaums, which reads FAT32 in user space.
 *
 * The drive is NEVER written to: files are only read with
 * UsbFile.read(offset, buffer). UsbFile.close()/flush() must not be called on
 * them, because libaums then writes the directory entry back (last access
 * time).
 *
 * The copy goes to <destRoot>/<volume label>/, which the Rekordbox library
 * feature finds. Files already copied with the same size are skipped.
 */
public class UsbImporter {
    private static final String TAG = "MixxxUsbImporter";
    private static final String ACTION_USB_PERMISSION =
            "org.mixxx.usbimporter.USB_PERMISSION";
    private static final String[] kFoldersToCopy = {"PIONEER", "Contents"};
    // Keep some room on the internal storage
    private static final long kReservedBytes = 300L * 1024 * 1024;
    private static final int kBufferSize = 1024 * 1024;

    private static volatile boolean sRunning = false;
    private static volatile boolean sCancelled = false;
    private static BroadcastReceiver sReceiver = null;
    private static final ArrayDeque<UsbMassStorageDevice> sPending = new ArrayDeque<>();
    private static AlertDialog sDialog = null;

    private static final Handler sMainHandler = new Handler(Looper.getMainLooper());

    /** Entry point, called from C++. */
    public static void start(final Context context, final String destRoot) {
        sMainHandler.post(() -> startOnMainThread(context, destRoot));
    }

    public static boolean isRunning() {
        return sRunning;
    }

    private static void startOnMainThread(Context context, String destRoot) {
        if (sRunning) {
            showMessage(context, "Importazione già in corso");
            return;
        }
        UsbMassStorageDevice[] devices;
        try {
            devices = UsbMassStorageDevice.getMassStorageDevices(context);
        } catch (Exception e) {
            Log.e(TAG, "Cannot list USB mass storage devices", e);
            showMessage(context, "Errore nella ricerca delle chiavette USB: " + e);
            return;
        }
        Log.i(TAG, "Found " + devices.length + " USB mass storage device(s)");
        if (devices.length == 0) {
            showMessage(context,
                    "Nessuna chiavetta USB trovata.\n\n"
                            + "Collegala al telefono (anche tramite hub) e riprova.");
            return;
        }
        sRunning = true;
        sCancelled = false;
        synchronized (sPending) {
            sPending.clear();
            for (UsbMassStorageDevice device : devices) {
                sPending.add(device);
            }
        }
        showProgress(context, "Chiavetta USB", "Richiesta di accesso alla chiavetta...");
        processNext(context, destRoot);
    }

    /** Asks for the permission of the next device, then imports it. */
    private static void processNext(final Context context, final String destRoot) {
        final UsbMassStorageDevice device;
        synchronized (sPending) {
            device = sPending.poll();
        }
        if (device == null) {
            finish(context, null);
            return;
        }
        final UsbManager usbManager =
                (UsbManager) context.getSystemService(Context.USB_SERVICE);
        final UsbDevice usbDevice = device.getUsbDevice();
        if (usbManager.hasPermission(usbDevice)) {
            importInBackground(context, device, destRoot);
            return;
        }
        unregisterReceiver(context);
        sReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context ctx, Intent intent) {
                if (!ACTION_USB_PERMISSION.equals(intent.getAction())) {
                    return;
                }
                unregisterReceiver(context);
                boolean granted = intent.getBooleanExtra(
                        UsbManager.EXTRA_PERMISSION_GRANTED, false);
                Log.i(TAG, "USB permission for " + usbDevice.getDeviceName()
                                + (granted ? " granted" : " denied"));
                if (granted) {
                    importInBackground(context, device, destRoot);
                } else {
                    processNext(context, destRoot);
                }
            }
        };
        IntentFilter filter = new IntentFilter(ACTION_USB_PERMISSION);
        if (Build.VERSION.SDK_INT >= 33) {
            context.registerReceiver(sReceiver, filter, Context.RECEIVER_NOT_EXPORTED);
        } else {
            context.registerReceiver(sReceiver, filter);
        }
        Intent intent = new Intent(ACTION_USB_PERMISSION);
        intent.setPackage(context.getPackageName());
        int flags = 0;
        if (Build.VERSION.SDK_INT >= 31) {
            // The system adds the result to the intent
            flags |= PendingIntent.FLAG_MUTABLE;
        }
        PendingIntent pendingIntent = PendingIntent.getBroadcast(context, 0, intent, flags);
        Log.i(TAG, "Requesting USB permission for " + usbDevice.getDeviceName());
        usbManager.requestPermission(usbDevice, pendingIntent);
    }

    private static void unregisterReceiver(Context context) {
        if (sReceiver != null) {
            try {
                context.unregisterReceiver(sReceiver);
            } catch (Exception e) {
                // Already unregistered
            }
            sReceiver = null;
        }
    }

    private static class FileToCopy {
        final UsbFile source;
        final File destination;

        FileToCopy(UsbFile source, File destination) {
            this.source = source;
            this.destination = destination;
        }
    }

    private static void importInBackground(final Context context,
            final UsbMassStorageDevice device,
            final String destRoot) {
        new Thread(() -> {
            String error = null;
            try {
                importDevice(context, device, destRoot);
            } catch (Exception e) {
                Log.e(TAG, "Import failed", e);
                error = e.toString();
            } finally {
                // Releases the USB interface only, nothing is written
                try {
                    device.close();
                } catch (Exception e) {
                    Log.w(TAG, "Closing the device failed", e);
                }
            }
            final String finalError = error;
            sMainHandler.post(() -> {
                if (finalError != null) {
                    finish(context, "Errore durante l'importazione:\n" + finalError);
                } else {
                    processNext(context, destRoot);
                }
            });
        }, "MixxxUsbImporter").start();
    }

    private static void importDevice(Context context, UsbMassStorageDevice device, String destRoot)
            throws IOException {
        updateProgress(context, "Apertura della chiavetta...");
        device.init();
        List<Partition> partitions = device.getPartitions();
        Log.i(TAG, "Device " + device.getUsbDevice().getDeviceName() + ": "
                        + partitions.size() + " partition(s)");
        for (Partition partition : partitions) {
            if (sCancelled) {
                return;
            }
            FileSystem fs = partition.getFileSystem();
            UsbFile root = fs.getRootDirectory();
            UsbFile pdb = root.search("PIONEER/rekordbox/export.pdb");
            String label = sanitize(fs.getVolumeLabel());
            Log.i(TAG, "Partition '" + label + "': export.pdb " + (pdb != null ? "found" : "missing"));
            File destDir = new File(destRoot, label);
            List<FileToCopy> all = new ArrayList<>();
            if (pdb != null) {
                // rekordbox export: its folders, as they are
                for (String folder : kFoldersToCopy) {
                    UsbFile dir = root.search(folder);
                    if (dir == null || !dir.isDirectory()) {
                        continue;
                    }
                    updateProgress(context, "Lettura delle cartelle (" + folder + ")...");
                    collect(dir, new File(destDir, folder), all);
                }
            } else {
                // Plain drive: the audio files, in their folders. The library
                // lists the copy like a plain drive plugged in.
                updateProgress(context, "Ricerca dei file audio...");
                collectAudio(root, destDir, all, 0);
                if (all.isEmpty()) {
                    Log.i(TAG, "Partition '" + label + "': no audio files");
                    continue;
                }
            }
            List<FileToCopy> files = new ArrayList<>();
            long bytesToCopy = 0;
            int skipped = 0;
            for (FileToCopy file : all) {
                if (file.destination.isFile()
                        && file.destination.length() == file.source.getLength()) {
                    skipped++;
                } else {
                    files.add(file);
                    bytesToCopy += file.source.getLength();
                }
            }
            Log.i(TAG, files.size() + " file(s) to copy (" + bytesToCopy + " bytes), "
                            + skipped + " already copied");
            if (!destDir.isDirectory() && !destDir.mkdirs()) {
                throw new IOException("Cannot create " + destDir);
            }
            long available = new StatFs(destDir.getPath()).getAvailableBytes();
            if (bytesToCopy + kReservedBytes > available) {
                throw new IOException("Spazio insufficiente: servono "
                        + (bytesToCopy / (1024 * 1024)) + " MB, liberi "
                        + (available / (1024 * 1024)) + " MB");
            }
            long copied = 0;
            long startMs = System.currentTimeMillis();
            ByteBuffer buffer = ByteBuffer.allocate(kBufferSize);
            for (int i = 0; i < files.size(); i++) {
                if (sCancelled) {
                    Log.i(TAG, "Import cancelled");
                    return;
                }
                FileToCopy file = files.get(i);
                long elapsedS = Math.max(1, (System.currentTimeMillis() - startMs) / 1000);
                updateProgress(context, String.format(
                        "Copia di \"%s\" in corso\n\nFile %d di %d\n%d / %d MB (%.1f MB/s)\n\n%s",
                        label, i + 1, files.size(), copied / (1024 * 1024),
                        bytesToCopy / (1024 * 1024),
                        copied / (1024.0 * 1024.0) / elapsedS,
                        file.source.getName()));
                copied += copyFile(file, buffer);
            }
            Log.i(TAG, "Copied " + copied + " bytes in "
                            + (System.currentTimeMillis() - startMs) + " ms to " + destDir);
            synchronized (sPending) {
                sImported.add(label + (pdb != null ? " (rekordbox, " : " (file audio, ")
                        + files.size() + " file copiati, " + skipped + " già presenti)");
            }
        }
    }

    private static final List<String> sImported = new ArrayList<>();

    private static void collect(UsbFile dir, File destDir, List<FileToCopy> out)
            throws IOException {
        for (UsbFile child : dir.listFiles()) {
            String name = child.getName();
            if (name.equals(".") || name.equals("..")) {
                continue;
            }
            File dest = new File(destDir, name);
            if (child.isDirectory()) {
                collect(child, dest, out);
            } else {
                out.add(new FileToCopy(child, dest));
            }
        }
    }

    // Audio files copied from a drive without rekordbox export (the formats
    // the app plays)
    private static final String[] kAudioExtensions = {
            "mp3", "m4a", "aac", "mp4", "flac", "wav", "aif", "aiff", "ogg", "opus", "wv"};
    // Folders of the system, of rekordbox and of the app, never music
    private static final String[] kSkippedFolders = {
            "System Volume Information", "$RECYCLE.BIN", "RECYCLER", "FOUND.000",
            "LOST.DIR", "Android", "PIONEER", "MIXXX"};
    private static final int kMaxDepth = 16;

    private static boolean isAudioFile(String name) {
        int dot = name.lastIndexOf('.');
        if (dot < 0) {
            return false;
        }
        String extension = name.substring(dot + 1).toLowerCase(java.util.Locale.ROOT);
        for (String audio : kAudioExtensions) {
            if (audio.equals(extension)) {
                return true;
            }
        }
        return false;
    }

    private static boolean isSkippedFolder(String name) {
        for (String skipped : kSkippedFolders) {
            if (skipped.equalsIgnoreCase(name)) {
                return true;
            }
        }
        return false;
    }

    /** The audio files below dir, without hidden ("._x.mp3", ".Trashes") and system folders. */
    private static void collectAudio(UsbFile dir, File destDir, List<FileToCopy> out, int depth)
            throws IOException {
        for (UsbFile child : dir.listFiles()) {
            if (sCancelled) {
                return;
            }
            String name = child.getName();
            if (name.startsWith(".")) {
                continue;
            }
            File dest = new File(destDir, name);
            if (child.isDirectory()) {
                if (depth < kMaxDepth && !isSkippedFolder(name)) {
                    collectAudio(child, dest, out, depth + 1);
                }
            } else if (isAudioFile(name)) {
                out.add(new FileToCopy(child, dest));
            }
        }
    }

    /** Copies one file. Reads only, see the class comment. */
    private static long copyFile(FileToCopy file, ByteBuffer buffer) throws IOException {
        File parent = file.destination.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IOException("Cannot create " + parent);
        }
        File partial = new File(file.destination.getPath() + ".part");
        long length = file.source.getLength();
        long offset = 0;
        try (FileOutputStream out = new FileOutputStream(partial)) {
            while (offset < length) {
                if (sCancelled) {
                    break;
                }
                buffer.clear();
                int chunk = (int) Math.min(buffer.capacity(), length - offset);
                buffer.limit(chunk);
                file.source.read(offset, buffer);
                out.write(buffer.array(), 0, chunk);
                offset += chunk;
            }
        }
        if (offset < length) {
            partial.delete();
            return offset;
        }
        if (file.destination.exists()) {
            file.destination.delete();
        }
        if (!partial.renameTo(file.destination)) {
            throw new IOException("Cannot rename " + partial);
        }
        long modified = file.source.lastModified();
        if (modified > 0) {
            file.destination.setLastModified(modified);
        }
        return length;
    }

    private static String sanitize(String label) {
        String result = label == null ? "" : label.trim().replaceAll("[^A-Za-z0-9 _.-]", "_");
        return result.isEmpty() ? "USB" : result;
    }

    // UI, always on the main thread

    private static void showProgress(final Context context, String title, String message) {
        if (!(context instanceof Activity)) {
            Log.i(TAG, message);
            return;
        }
        dismissDialog();
        sDialog = new AlertDialog.Builder(context)
                          .setTitle(title)
                          .setMessage(message)
                          .setCancelable(false)
                          .setNegativeButton("Annulla", (dialog, which) -> {
                              sCancelled = true;
                              synchronized (sPending) {
                                  sPending.clear();
                              }
                          })
                          .create();
        sDialog.show();
    }

    private static void updateProgress(final Context context, final String message) {
        Log.d(TAG, message.replace('\n', ' '));
        sMainHandler.post(() -> {
            if (sDialog != null && sDialog.isShowing()) {
                sDialog.setMessage(message);
            }
        });
    }

    private static void finish(Context context, String error) {
        unregisterReceiver(context);
        sRunning = false;
        String message;
        if (error != null) {
            message = error;
        } else if (sCancelled) {
            message = "Importazione annullata. I file già copiati restano e non verranno ricopiati.";
        } else {
            synchronized (sPending) {
                if (sImported.isEmpty()) {
                    message = "Nessuna chiavetta con un export rekordbox "
                            + "o con file audio trovata.";
                } else {
                    message = "Importato:\n" + String.join("\n", sImported)
                            + "\n\nTocca \"USB / rekordbox\" nella libreria per vederlo.";
                }
            }
        }
        synchronized (sPending) {
            sImported.clear();
        }
        Log.i(TAG, message.replace('\n', ' '));
        showMessage(context, message);
    }

    private static void showMessage(Context context, String message) {
        dismissDialog();
        if (!(context instanceof Activity)) {
            Log.i(TAG, message);
            return;
        }
        sDialog = new AlertDialog.Builder(context)
                          .setTitle("Chiavetta USB")
                          .setMessage(message)
                          .setPositiveButton("OK", null)
                          .create();
        sDialog.show();
    }

    private static void dismissDialog() {
        if (sDialog != null) {
            try {
                sDialog.dismiss();
            } catch (Exception e) {
                // Activity gone
            }
            sDialog = null;
        }
    }
}
