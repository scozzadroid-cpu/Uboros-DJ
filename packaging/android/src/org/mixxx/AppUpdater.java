package org.mixxx;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageInstaller;
import android.net.ConnectivityManager;
import android.net.NetworkCapabilities;
import android.net.Uri;
import android.os.Build;
import android.os.Process;
import android.provider.Settings;
import android.util.Log;

import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;

/**
 * Over-the-air update: reads update.json from the website, downloads the new
 * APK (resumable, on an unmetered network unless the user asks), checks its
 * sha256 and size and installs it with PackageInstaller.
 *
 * The only thing sent to the server is a normal HTTPS request with the
 * User-Agent "UborosDJ/<versionName>": no device identifier, no other header.
 * Everything is static and driven from C++ (qmlappupdater.cpp), which polls
 * {@link #status()}. The state names are the ones of the QML AppUpdater.
 */
public class AppUpdater {
    private static final String TAG = "MixxxUpdate";
    private static final String ACTION_STATUS = "org.mixxx.UPDATE_STATUS";
    private static final int MAX_REDIRECTS = 5;
    private static final int TIMEOUT_MS = 20000;
    private static final long MAX_MANIFEST_BYTES = 256 * 1024;
    private static final char SEP = '\u001f';
    private static final long MAX_APK_BYTES = 1024L * 1024 * 1024;
    private static final long FREE_MARGIN = 50L * 1024 * 1024;
    private static final long INSTALL_TIMEOUT_MS = 2 * 60 * 1000;
    private static final long CAN_INSTALL_TTL_MS = 2000;

    private static final Object LOCK = new Object();
    private static String sState = "idle";
    private static String sErrorCode = "";
    private static String sErrorDetail = "";
    private static long sLastCheck = 0;
    private static int sAvailableCode = 0;
    private static String sAvailableName = "";
    private static String sNotesEn = "";
    private static String sNotesIt = "";
    private static String sApkUrl = "";
    private static String sApkSha = "";
    private static long sApkSize = 0;
    private static boolean sMetered = false;
    private static boolean sMeteredBlocked = false;
    private static long sDone = 0;
    private static long sTotal = 0;
    private static boolean sAllowHttp = false;
    private static volatile boolean sCancel = false;
    private static Thread sWorker = null;
    private static BroadcastReceiver sReceiver = null;
    private static Context sApp = null;
    private static long sInstallDeadline = 0;
    private static int sSessionId = -1;
    // Binder results cached: status() is polled by the GUI thread
    private static int sCachedCode = -1;
    private static String sCachedName = null;
    private static boolean sCanInstall = false;
    private static long sCanInstallAt = 0;

    // ---------------------------------------------------------------- public

    /** Called once at start: remembers the context and deletes old APKs. */
    public static void init(Context context) {
        if (context == null) {
            return;
        }
        synchronized (LOCK) {
            sApp = context.getApplicationContext();
        }
        new Thread(() -> {
            Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);
            cleanup(false);
        }, "update-cleanup").start();
    }

    /** The installed version cannot change while the process lives: cached. */
    private static synchronized void loadInstalled(Context context) {
        if (sCachedName != null) {
            return;
        }
        try {
            PackageInfo pi = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
            sCachedCode = (int) pi.getLongVersionCode();
            sCachedName = pi.versionName == null ? "" : pi.versionName;
        } catch (Exception e) {
            sCachedCode = 0;
            sCachedName = "";
        }
    }

    public static int installedVersionCode(Context context) {
        loadInstalled(context);
        return sCachedCode;
    }

    public static String installedVersionName(Context context) {
        loadInstalled(context);
        return sCachedName;
    }

    /**
     * state, progress(0-1), versionName, versionCode, notes en, notes it,
     * error code, error detail, last check (ms), metered blocked, total bytes,
     * can install, installed versionName, installed versionCode
     * joined with the unit separator.
     */
    public static String status() {
        synchronized (LOCK) {
            double progress = sTotal > 0 ? Math.min(1.0, (double) sDone / (double) sTotal) : 0;
            long now = System.currentTimeMillis();
            if ("installing".equals(sState) && now > sInstallDeadline) {
                // The confirmation was ignored and no broadcast came: back to ready
                sState = "ready";
                final int sid = sSessionId;
                sSessionId = -1;
                if (sid >= 0 && sApp != null) {
                    final Context app = sApp;
                    new Thread(() -> abandonSession(app, sid), "update-abandon").start();
                }
            }
            if (sApp != null && now - sCanInstallAt > CAN_INSTALL_TTL_MS) {
                sCanInstall = canInstallPackages(sApp);
                sCanInstallAt = now;
            }
            boolean canInstall = sApp != null && sCanInstall;
            StringBuilder b = new StringBuilder();
            b.append(sState).append(SEP)
                    .append(progress).append(SEP)
                    .append(sAvailableName).append(SEP)
                    .append(sAvailableCode).append(SEP)
                    .append(sNotesEn).append(SEP)
                    .append(sNotesIt).append(SEP)
                    .append(sErrorCode).append(SEP)
                    .append(sErrorDetail).append(SEP)
                    .append(sLastCheck).append(SEP)
                    .append(sMeteredBlocked ? 1 : 0).append(SEP)
                    .append(sApkSize).append(SEP)
                    .append(canInstall ? 1 : 0).append(SEP)
                    .append(sApp == null ? "" : installedVersionName(sApp)).append(SEP)
                    .append(sApp == null ? 0 : installedVersionCode(sApp));
            return b.toString();
        }
    }

    /**
     * Fetches update.json. If a newer version exists and autoDownload is set
     * and the network is not metered, the download starts by itself.
     */
    public static void checkNow(final String url, final boolean allowHttp, final boolean autoDownload) {
        synchronized (LOCK) {
            if (sApp == null || isBusy()) {
                return;
            }
            sAllowHttp = allowHttp;
            sState = "checking";
            sErrorCode = "";
            sErrorDetail = "";
            sCancel = false;
            sWorker = new Thread(() -> {
                Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);
                runCheck(url, autoDownload);
            }, "update-check");
            sWorker.start();
        }
    }

    /** Explicit request of the user: also on a metered network. */
    public static void download(final boolean allowMetered) {
        synchronized (LOCK) {
            if (sApp == null || !"available".equals(sState) && !"error".equals(sState)
                    || sApkUrl.isEmpty() || sAvailableCode <= 0) {
                return;
            }
            startDownload(allowMetered);
        }
    }

    public static void cancel() {
        sCancel = true;
    }

    public static boolean canInstallPackages(Context context) {
        return context.getPackageManager().canRequestPackageInstalls();
    }

    /**
     * Installs the downloaded APK. Without the "install unknown apps"
     * permission opens the system page for it instead (error code
     * "permission") and returns false.
     */
    public static boolean install(final Activity activity) {
        if (activity == null) {
            return false;
        }
        final Context app = activity.getApplicationContext();
        final File apk;
        final String expectedSha;
        synchronized (LOCK) {
            if (!"ready".equals(sState) && !"error".equals(sState)) {
                return false;
            }
            apk = apkFile(app, sAvailableCode);
            if (!apk.isFile() || sApkSize <= 0 || apk.length() != sApkSize) {
                setError("missing", "");
                return false;
            }
            if (!canInstallPackages(app)) {
                sState = "ready";
                sErrorCode = "permission";
                openUnknownSourcesSettings(activity);
                return false;
            }
            sState = "installing";
            sErrorCode = "";
            sErrorDetail = "";
            sInstallDeadline = System.currentTimeMillis() + INSTALL_TIMEOUT_MS;
            sSessionId = -1;
            expectedSha = sApkSha;
        }
        final String sha = expectedSha;
        new Thread(() -> runInstall(app, apk, sha), "update-install").start();
        return true;
    }

    // --------------------------------------------------------------- helpers

    private static boolean isBusy() {
        return "checking".equals(sState) || "downloading".equals(sState)
                || "installing".equals(sState);
    }

    private static void abandonSession(Context app, int id) {
        try {
            app.getPackageManager().getPackageInstaller().abandonSession(id);
        } catch (Exception e) {
            Log.w(TAG, "abandonSession " + id + ": " + e);
        }
    }

    private static void setError(String code, String detail) {
        synchronized (LOCK) {
            sState = "error";
            sErrorCode = code;
            sErrorDetail = detail == null ? "" : detail;
        }
        Log.w(TAG, "error " + code + " " + detail);
    }

    private static File updatesDir(Context c) {
        File base = c.getExternalFilesDir(null);
        if (base == null) {
            base = c.getFilesDir();
        }
        File d = new File(base, "updates");
        d.mkdirs();
        return d;
    }

    private static File apkFile(Context c, int code) {
        return new File(updatesDir(c), "update-" + code + ".apk");
    }

    private static File partFile(Context c, int code) {
        return new File(updatesDir(c), "update-" + code + ".apk.part");
    }

    /** Deletes the APKs of versions that are installed already (or all of them). */
    private static void cleanup(boolean everything) {
        Context c;
        synchronized (LOCK) {
            c = sApp;
        }
        if (c == null) {
            return;
        }
        int installed = installedVersionCode(c);
        File[] files = updatesDir(c).listFiles();
        if (files == null) {
            return;
        }
        for (File f : files) {
            String n = f.getName();
            if (!n.startsWith("update-")) {
                continue;
            }
            int code = 0;
            try {
                code = Integer.parseInt(n.substring(7, n.indexOf('.')));
            } catch (Exception e) {
                // unknown file: leave it
                continue;
            }
            if (everything || code <= installed) {
                if (f.delete()) {
                    Log.i(TAG, "deleted " + n);
                }
            }
        }
    }

    private static boolean isMetered(Context c) {
        ConnectivityManager cm = (ConnectivityManager) c.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (cm == null) {
            return true;
        }
        NetworkCapabilities nc = cm.getNetworkCapabilities(cm.getActiveNetwork());
        if (nc == null) {
            return true;
        }
        return !nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED);
    }

    private static boolean hasNetwork(Context c) {
        ConnectivityManager cm = (ConnectivityManager) c.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (cm == null) {
            return false;
        }
        NetworkCapabilities nc = cm.getNetworkCapabilities(cm.getActiveNetwork());
        return nc != null && nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET);
    }

    /**
     * Opens the connection following up to 5 redirects (also to another
     * host, https only; plain http only when the manifest URL came from the
     * test override). The Range header is sent again after each redirect.
     */
    private static HttpURLConnection open(String url, long rangeFrom, boolean allowHttp) throws Exception {
        String current = url;
        for (int hop = 0; hop <= MAX_REDIRECTS; hop++) {
            URL u = new URL(current);
            String proto = u.getProtocol();
            if (!"https".equals(proto) && !("http".equals(proto) && allowHttp)) {
                throw new SecurityException("not https: " + proto);
            }
            HttpURLConnection conn = (HttpURLConnection) u.openConnection();
            conn.setInstanceFollowRedirects(false);
            conn.setConnectTimeout(TIMEOUT_MS);
            conn.setReadTimeout(TIMEOUT_MS);
            conn.setRequestProperty("User-Agent", "UborosDJ/" + installedVersionName(sApp));
            conn.setRequestProperty("Accept-Encoding", "identity");
            if (rangeFrom > 0) {
                conn.setRequestProperty("Range", "bytes=" + rangeFrom + "-");
            }
            int code = conn.getResponseCode();
            if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
                String loc = conn.getHeaderField("Location");
                conn.disconnect();
                if (loc == null) {
                    throw new java.io.IOException("redirect without Location");
                }
                current = new URL(u, loc).toString();
                continue;
            }
            return conn;
        }
        throw new java.io.IOException("too many redirects");
    }

    // ----------------------------------------------------------------- check

    private static void runCheck(String url, boolean autoDownload) {
        Context c = sApp;
        try {
            if (!hasNetwork(c)) {
                throw new java.io.IOException("offline");
            }
            HttpURLConnection conn = open(url, 0, sAllowHttp);
            int code = conn.getResponseCode();
            if (code != 200) {
                conn.disconnect();
                throw new java.io.IOException("HTTP " + code);
            }
            String text;
            try (InputStream in = conn.getInputStream()) {
                java.io.ByteArrayOutputStream bo = new java.io.ByteArrayOutputStream();
                byte[] buf = new byte[8192];
                int n;
                while ((n = in.read(buf)) > 0) {
                    bo.write(buf, 0, n);
                    if (bo.size() > MAX_MANIFEST_BYTES) {
                        throw new java.io.IOException("manifest too big");
                    }
                }
                text = bo.toString("UTF-8");
            } finally {
                conn.disconnect();
            }
            JSONObject j = new JSONObject(text);
            int newCode = j.getInt("versionCode");
            String newName = j.optString("versionName", String.valueOf(newCode));
            int minCode = j.optInt("minVersionCode", 1);
            JSONObject apk = j.getJSONObject("apk");
            String apkUrl = apk.getString("url");
            String sha = apk.getString("sha256").trim().toLowerCase();
            long size = apk.getLong("size");
            JSONObject notes = j.optJSONObject("notes");
            String en = notes == null ? "" : notes.optString("en", "");
            String it = notes == null ? "" : notes.optString("it", "");
            int installed = installedVersionCode(c);
            // An APK downloaded in an earlier session counts only if its sha256 matches
            boolean cached = false;
            if (newCode > installed && sha.matches("[0-9a-f]{64}") && size > 0 && size <= MAX_APK_BYTES) {
                File prev = apkFile(c, newCode);
                if (prev.isFile() && prev.length() == size) {
                    if (sha256(prev).equals(sha)) {
                        cached = true;
                    } else {
                        prev.delete();
                    }
                }
            }
            synchronized (LOCK) {
                sLastCheck = System.currentTimeMillis();
                if (newCode <= installed) {
                    sState = "idle";
                    sAvailableCode = 0;
                    sAvailableName = "";
                    Log.i(TAG, "up to date: installed " + installed + ", site " + newCode);
                    return;
                }
                if (minCode > installed) {
                    Log.w(TAG, "minVersionCode " + minCode + " > installed " + installed);
                }
                if (!sha.matches("[0-9a-f]{64}") || size <= 0 || size > MAX_APK_BYTES) {
                    setError("manifest", "sha256/size");
                    return;
                }
                sAvailableCode = newCode;
                sAvailableName = newName;
                sNotesEn = en;
                sNotesIt = it;
                sApkUrl = apkUrl;
                sApkSha = sha;
                sApkSize = size;
                sDone = 0;
                sTotal = size;
                sState = "available";
                // An APK already downloaded (earlier session) and valid
                if (cached) {
                    sDone = size;
                    sState = "ready";
                    return;
                }
                sMetered = isMetered(c);
                sMeteredBlocked = sMetered;
                if (autoDownload && !sMetered) {
                    startDownload(false);
                }
            }
        } catch (Exception e) {
            String code = e instanceof java.net.UnknownHostException
                    || e instanceof java.net.SocketTimeoutException
                    || "offline".equals(e.getMessage()) ? "network" : "check";
            String detail = e.getClass().getSimpleName() + ": " + e.getMessage();
            synchronized (LOCK) {
                sLastCheck = System.currentTimeMillis();
                // A failed check must not hide an update that is already known
                // or downloaded: the error stays informational
                if (sAvailableCode > 0 && !sApkUrl.isEmpty()) {
                    File have = apkFile(c, sAvailableCode);
                    boolean ready = sApkSize > 0 && have.isFile() && have.length() == sApkSize;
                    sState = ready ? "ready" : "available";
                    sErrorCode = code;
                    sErrorDetail = detail;
                    Log.w(TAG, "check failed (state kept " + sState + ") " + code + " " + detail);
                    return;
                }
            }
            setError(code, detail);
        }
    }

    // -------------------------------------------------------------- download

    /** Call with LOCK held. */
    private static void startDownload(final boolean allowMetered) {
        if ("downloading".equals(sState)) {
            return;
        }
        sState = "downloading";
        sMeteredBlocked = false;
        sErrorCode = "";
        sErrorDetail = "";
        sCancel = false;
        final int code = sAvailableCode;
        final String url = sApkUrl;
        final String sha = sApkSha;
        final long size = sApkSize;
        final boolean allowHttp = sAllowHttp;
        sWorker = new Thread(() -> {
            Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);
            runDownload(code, url, sha, size, allowMetered, allowHttp);
        }, "update-download");
        sWorker.start();
    }

    private static void runDownload(int code, String url, String sha, long size,
            boolean allowMetered, boolean allowHttp) {
        Context c = sApp;
        File part = partFile(c, code);
        File done = apkFile(c, code);
        try {
            long have = part.isFile() ? part.length() : 0;
            if (have > size) {
                part.delete();
                have = 0;
            }
            boolean restarted = false;
            int stalls = 0;
            if (size <= 0 || size > MAX_APK_BYTES) {
                throw new java.io.IOException("implausible size " + size);
            }
            long free = updatesDir(c).getUsableSpace();
            if (free < size - have + FREE_MARGIN) {
                throw new java.io.IOException("not enough free space");
            }
            while (have < size) {
                if (sCancel) {
                    throw new InterruptedException("cancelled");
                }
                if (!allowMetered && isMetered(c)) {
                    setPaused("metered");
                    return;
                }
                HttpURLConnection conn = open(url, have, allowHttp);
                int status = conn.getResponseCode();
                boolean append;
                if (status == 206) {
                    append = true;
                } else if (status == 200) {
                    // The server ignored the Range: start over (once)
                    if (have > 0) {
                        if (restarted) {
                            conn.disconnect();
                            throw new java.io.IOException("server ignores Range");
                        }
                        restarted = true;
                    }
                    append = false;
                    have = 0;
                } else if (status == 416) {
                    conn.disconnect();
                    part.delete();
                    have = 0;
                    if (++stalls >= 3) {
                        throw new java.io.IOException("HTTP 416 repeated");
                    }
                    continue;
                } else {
                    conn.disconnect();
                    throw new java.io.IOException("HTTP " + status);
                }
                boolean progressed = false;
                try (InputStream in = conn.getInputStream();
                        OutputStream out = new FileOutputStream(part, append)) {
                    byte[] buf = new byte[64 * 1024];
                    int n;
                    long lastPublish = 0;
                    while ((n = in.read(buf)) > 0) {
                        if (sCancel) {
                            throw new InterruptedException("cancelled");
                        }
                        out.write(buf, 0, n);
                        have += n;
                        progressed = true;
                        if (have > size) {
                            // More data than declared: stop before filling the disk
                            part.delete();
                            throw new SecurityException("size");
                        }
                        long now = System.nanoTime();
                        if (now - lastPublish > 250_000_000L) {
                            lastPublish = now;
                            synchronized (LOCK) {
                                sDone = have;
                                sTotal = size;
                            }
                            if (!allowMetered && isMetered(c)) {
                                setPaused("metered");
                                return;
                            }
                        }
                    }
                } finally {
                    conn.disconnect();
                }
                synchronized (LOCK) {
                    sDone = have;
                }
                if (have < size) {
                    // Connection closed early: ask for the rest (bounded)
                    Log.i(TAG, "short read at " + have + "/" + size);
                    if (progressed) {
                        stalls = 0;
                    } else if (++stalls >= 3) {
                        throw new java.io.IOException("no progress at " + have + "/" + size);
                    } else {
                        Thread.sleep(1000);
                    }
                }
            }
            // Verify size and sha256 (reads the whole file once)
            if (part.length() != size) {
                part.delete();
                throw new SecurityException("size");
            }
            String actual = sha256(part);
            if (!actual.equalsIgnoreCase(sha)) {
                part.delete();
                Log.w(TAG, "sha256 mismatch: " + actual);
                throw new SecurityException("sha256");
            }
            if (done.exists()) {
                done.delete();
            }
            if (!part.renameTo(done)) {
                throw new java.io.IOException("rename failed");
            }
            synchronized (LOCK) {
                sDone = size;
                sState = "ready";
            }
            Log.i(TAG, "update " + code + " downloaded and verified");
        } catch (InterruptedException e) {
            // Cancelled: the partial file stays, a later download resumes it
            synchronized (LOCK) {
                sState = "available";
            }
        } catch (SecurityException e) {
            setError("hash", e.getMessage());
            synchronized (LOCK) {
                sDone = 0;
            }
        } catch (Exception e) {
            setError("download", e.getClass().getSimpleName() + ": " + e.getMessage());
        }
    }

    /** Back to "available" with the metered flag (download waits for Wi-Fi). */
    private static void setPaused(String why) {
        synchronized (LOCK) {
            sState = "available";
            sMeteredBlocked = "metered".equals(why);
        }
    }

    private static String sha256(File f) throws Exception {
        MessageDigest md = MessageDigest.getInstance("SHA-256");
        try (InputStream in = new FileInputStream(f)) {
            byte[] buf = new byte[256 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) {
                md.update(buf, 0, n);
            }
        }
        StringBuilder sb = new StringBuilder();
        for (byte b : md.digest()) {
            sb.append(String.format("%02x", b));
        }
        return sb.toString();
    }

    // --------------------------------------------------------------- install

    private static void openUnknownSourcesSettings(Activity activity) {
        try {
            Intent i = new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                    Uri.parse("package:" + activity.getPackageName()));
            i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            activity.startActivity(i);
        } catch (Exception e) {
            Log.w(TAG, "cannot open the unknown sources page", e);
        }
    }

    private static void runInstall(final Context app, File apk, String expectedSha) {
        Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);
        int id = -1;
        try {
            registerReceiver(app);
            PackageInstaller pi = app.getPackageManager().getPackageInstaller();
            PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(
                    PackageInstaller.SessionParams.MODE_FULL_INSTALL);
            params.setAppPackageName(app.getPackageName());
            params.setSize(apk.length());
            if (Build.VERSION.SDK_INT >= 31) {
                params.setRequireUserAction(PackageInstaller.SessionParams.USER_ACTION_NOT_REQUIRED);
            }
            id = pi.createSession(params);
            synchronized (LOCK) {
                sSessionId = id;
            }
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            try (PackageInstaller.Session session = pi.openSession(id)) {
                try (InputStream in = new FileInputStream(apk);
                        OutputStream out = session.openWrite("base.apk", 0, apk.length())) {
                    byte[] buf = new byte[256 * 1024];
                    int n;
                    while ((n = in.read(buf)) > 0) {
                        out.write(buf, 0, n);
                        md.update(buf, 0, n);
                    }
                    session.fsync(out);
                }
                // The file is checked against the manifest while it is copied
                StringBuilder hex = new StringBuilder();
                for (byte b : md.digest()) {
                    hex.append(String.format("%02x", b));
                }
                if (!hex.toString().equalsIgnoreCase(expectedSha)) {
                    session.abandon();
                    apk.delete();
                    Log.w(TAG, "sha256 mismatch at install: " + hex);
                    throw new SecurityException("sha256");
                }
                Intent intent = new Intent(ACTION_STATUS).setPackage(app.getPackageName());
                PendingIntent pending = PendingIntent.getBroadcast(app, id, intent,
                        PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_MUTABLE);
                synchronized (LOCK) {
                    sInstallDeadline = System.currentTimeMillis() + INSTALL_TIMEOUT_MS;
                }
                session.commit(pending.getIntentSender());
            }
        } catch (SecurityException e) {
            setError("hash", e.getMessage());
        } catch (Exception e) {
            if (id >= 0) {
                abandonSession(app, id);
            }
            setError("install", e.getClass().getSimpleName() + ": " + e.getMessage());
        }
    }

    private static synchronized void registerReceiver(final Context app) {
        if (sReceiver != null) {
            return;
        }
        sReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                int status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS, -999);
                String msg = intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE);
                Log.i(TAG, "install status " + status + " " + msg);
                if (status == PackageInstaller.STATUS_PENDING_USER_ACTION) {
                    synchronized (LOCK) {
                        sInstallDeadline = System.currentTimeMillis() + INSTALL_TIMEOUT_MS;
                    }
                    Intent confirm = intent.getParcelableExtra(Intent.EXTRA_INTENT, Intent.class);
                    if (confirm != null) {
                        confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        try {
                            context.startActivity(confirm);
                        } catch (Exception e) {
                            setError("install", "confirm: " + e.getMessage());
                        }
                    }
                } else if (status == PackageInstaller.STATUS_SUCCESS) {
                    // The process is replaced right after this
                    cleanup(true);
                } else {
                    // Failed or cancelled by the user: the APK stays for a retry
                    synchronized (LOCK) {
                        sState = "ready";
                    }
                    if (status != PackageInstaller.STATUS_FAILURE_ABORTED) {
                        setError("install", String.valueOf(msg));
                        synchronized (LOCK) {
                            sState = "error";
                        }
                    }
                }
            }
        };
        IntentFilter f = new IntentFilter(ACTION_STATUS);
        if (Build.VERSION.SDK_INT >= 33) {
            app.registerReceiver(sReceiver, f, Context.RECEIVER_NOT_EXPORTED);
        } else {
            app.registerReceiver(sReceiver, f);
        }
    }
}
