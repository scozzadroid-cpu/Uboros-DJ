package org.mixxx;

import android.content.Context;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;

import java.io.File;
import java.util.List;

/**
 * Lists the mounted storage volumes (internal, SD card, USB drive) as
 * "path|type|readonly|label" lines, one per volume, for the library folder
 * chooser. Types: internal, sd, usb.
 */
public class StorageHelper {
    public static String[] listVolumes(Context context) {
        StorageManager sm = context.getSystemService(StorageManager.class);
        if (sm == null) {
            return new String[0];
        }
        List<StorageVolume> volumes = sm.getStorageVolumes();
        java.util.ArrayList<String> out = new java.util.ArrayList<>();
        for (StorageVolume v : volumes) {
            String state = v.getState();
            boolean ro = android.os.Environment.MEDIA_MOUNTED_READ_ONLY.equals(state);
            if (!ro && !android.os.Environment.MEDIA_MOUNTED.equals(state)) {
                continue;
            }
            File dir = v.getDirectory();
            if (dir == null) {
                continue;
            }
            String type;
            if (v.isPrimary()) {
                type = "internal";
            } else {
                // Removable: a USB drive has no "SD" in its description
                String desc = String.valueOf(v.getDescription(context)).toLowerCase();
                type = desc.contains("sd") ? "sd" : "usb";
            }
            out.add(dir.getAbsolutePath() + "|" + type + "|" + (ro ? 1 : 0) + "|"
                    + v.getDescription(context));
        }
        return out.toArray(new String[0]);
    }
}
