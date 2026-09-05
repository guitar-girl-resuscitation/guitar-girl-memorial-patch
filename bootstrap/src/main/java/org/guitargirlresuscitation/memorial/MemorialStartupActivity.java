package org.guitargirlresuscitation.memorial;

import android.app.Activity;
import android.app.Application;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.view.Gravity;
import android.view.DisplayCutout;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.HorizontalScrollView;
import android.widget.TextView;
import android.widget.Switch;

import java.security.SecureRandom;
import java.io.File;
import java.util.Locale;
import java.util.TimeZone;
import java.util.concurrent.atomic.AtomicBoolean;

/** Visible, fail-closed startup gate. Unity is not created until every line succeeds. */
public final class MemorialStartupActivity extends Activity {
    private static final String TAG = "GGFM";
    // Preserve the game's original Firebase Unity lifecycle bridge. Remote
    // Firebase behavior is still retired by the native policy and loopback
    // network boundary; replacing this class with a bare UnityPlayerActivity
    // leaves Firebase C++ with a null Java host and crashes on Android 16.
    private static final String UNITY_ACTIVITY = "com.google.firebase.MessagingUnityPlayerActivity";
    private static final long MINIMUM_VISIBLE_MILLIS = 1_800L;
    private static final AtomicBoolean STARTED = new AtomicBoolean();
    private static volatile boolean UNITY_RELEASED;
    private static final int MAX_VISIBLE_LOG_CHARS = 24_000;

    private TextView logView;
    private TerminalBannerView bannerView;
    private HorizontalScrollView bannerScroll;
    private ScrollView logScroll;
    private Button startButton;
    private Switch fastStartSwitch;
    private boolean fastStart;
    private SaveTransferController saveTransfer;
    private UpdateController updates;
    private final View[] launcherPages = new View[3];
    private final Button[] navigation = new Button[3];
    private Words words;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final SpannableStringBuilder visibleLog = new SpannableStringBuilder();
    private volatile boolean logPumpRunning;
    private long logEpochMillis;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        // The launcher Activity is deliberately the diagnostic gate. If the
        // same process already released Unity, tapping the app icon must bring
        // the existing game Activity forward instead of recreating the gate
        // over the running game.
        if (UNITY_RELEASED) {
            bringUnityToFront();
            return;
        }
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        configureWindow();
        words = Words.forLocale(Locale.getDefault());
        setContentView(buildView());
        append("bootstrap: diagnostic gate ready; waiting for Start");
        startButton.setOnClickListener(view -> startBootstrap());
        if (getSharedPreferences("ggfm_startup_options", MODE_PRIVATE).getBoolean("check_updates", true))
            updates.check(true);
        // Request document access only after an explicit Import/Export action.
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (saveTransfer != null && saveTransfer.onResult(requestCode, resultCode, data)) return;
        super.onActivityResult(requestCode, resultCode, data);
    }

    private void configureWindow() {
        final int background = Color.rgb(20, 17, 24);
        Window window = getWindow();
        window.setBackgroundDrawable(new ColorDrawable(background));
        window.setStatusBarColor(background);
        window.setNavigationBarColor(background);
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        window.getDecorView().setBackgroundColor(background);
        window.getDecorView().setSystemUiVisibility(ViewGroup.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    private View buildView() {
        int pad = Math.round(28 * getResources().getDisplayMetrics().density);
        int smallPad = Math.round(12 * getResources().getDisplayMetrics().density);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER_HORIZONTAL);
        root.setPadding(pad, smallPad, pad, smallPad);
        root.setBackgroundColor(Color.rgb(20, 17, 24));

        TextView title = new TextView(this);
        title.setText(words.title);
        title.setTextColor(Color.rgb(255, 111, 160));
        title.setTextSize(23);
        title.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        title.setGravity(Gravity.CENTER);
        title.setMaxLines(2);
        root.addView(title, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        LinearLayout pageHost = new LinearLayout(this);
        pageHost.setOrientation(LinearLayout.VERTICAL);
        root.addView(pageHost, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));
        LinearLayout home = new LinearLayout(this);
        home.setOrientation(LinearLayout.VERTICAL);
        launcherPages[0] = home;
        pageHost.addView(home, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        logScroll = new ScrollView(this);
        logScroll.setFillViewport(true);
        logScroll.setBackgroundColor(Color.rgb(10, 9, 13));
        logView = new TextView(this);
        logView.setTextColor(Color.rgb(218, 238, 220));
        logView.setTextSize(10);
        logView.setTypeface(terminalTypeface());
        logView.setLetterSpacing(0.0f);
        logView.setTextScaleX(1.0f);
        logView.setFontFeatureSettings("'liga' 0, 'calt' 0");
        logView.setLineSpacing(1.5f * getResources().getDisplayMetrics().density, 1.05f);
        logView.setPadding(smallPad, smallPad, smallPad, smallPad);
        LinearLayout logContent = new LinearLayout(this);
        logContent.setOrientation(LinearLayout.VERTICAL);
        bannerScroll = new HorizontalScrollView(this);
        bannerScroll.setVisibility(View.GONE);
        bannerView = new TerminalBannerView(this, logView.getTypeface());
        bannerScroll.addView(bannerView, new HorizontalScrollView.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        logContent.addView(bannerScroll, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        logContent.addView(logView, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        logScroll.addView(logContent, new ScrollView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        LinearLayout.LayoutParams logParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1);
        logParams.topMargin = smallPad;
        home.addView(logScroll, logParams);

        // Save tools are a separate page; controller and storage semantics stay unchanged.
        LinearLayout extensionArea = new LinearLayout(this);
        extensionArea.setOrientation(LinearLayout.VERTICAL);
        extensionArea.setBackgroundColor(Color.rgb(24, 20, 29));
        LinearLayout transferRow = new LinearLayout(this);
        transferRow.setOrientation(LinearLayout.HORIZONTAL);
        Button exportButton = new Button(this);
        Button importButton = new Button(this);
        exportButton.setAllCaps(false); importButton.setAllCaps(false);
        exportButton.setTextSize(13); importButton.setTextSize(13);
        transferRow.addView(exportButton, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        transferRow.addView(importButton, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        extensionArea.addView(transferRow);
        TextView savePath = new TextView(this);
        savePath.setText("Documents/" + getPackageName() + "/saves");
        savePath.setTextColor(Color.rgb(180, 173, 190));
        savePath.setTextSize(11);
        savePath.setPadding(smallPad, smallPad, smallPad, smallPad);
        extensionArea.addView(savePath);
        fastStart = getSharedPreferences("ggfm_startup_options", MODE_PRIVATE).getBoolean("fast_start", false);
        fastStartSwitch = new Switch(this);
        fastStartSwitch.setText(TerminalText.fastStartLabel(Locale.getDefault()));
        fastStartSwitch.setTextColor(Color.rgb(218, 238, 220));
        fastStartSwitch.setTextSize(13);
        fastStartSwitch.setPadding(smallPad, smallPad, smallPad, smallPad);
        fastStartSwitch.setChecked(fastStart);
        fastStartSwitch.setOnCheckedChangeListener((button, checked) -> {
            fastStart = checked;
            getSharedPreferences("ggfm_startup_options", MODE_PRIVATE).edit().putBoolean("fast_start", checked).apply();
        });
        home.addView(fastStartSwitch);
        ScrollView extensionScroll = new ScrollView(this);
        extensionScroll.addView(extensionArea);
        LinearLayout.LayoutParams extensionParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1.0f);
        extensionParams.topMargin = smallPad;
        launcherPages[1] = extensionScroll;
        extensionScroll.setVisibility(View.GONE);
        pageHost.addView(extensionScroll, extensionParams);

        TextView homeUpdate = new TextView(this);
        homeUpdate.setTextColor(Color.rgb(255, 190, 105));
        homeUpdate.setTextSize(12);
        homeUpdate.setVisibility(View.GONE);
        homeUpdate.setOnClickListener(view -> selectLauncherPage(2));
        home.addView(homeUpdate);

        startButton = new Button(this);
        startButton.setAllCaps(false);
        startButton.setText(startAction(Locale.getDefault()));
        startButton.setTextSize(16);
        startButton.setTextColor(Color.WHITE);
        startButton.setBackgroundColor(Color.rgb(234, 78, 126));
        LinearLayout.LayoutParams buttonParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        buttonParams.topMargin = smallPad;
        home.addView(startButton, buttonParams);
        saveTransfer = new SaveTransferController(this, startButton, exportButton, importButton);

        LinearLayout about = new LinearLayout(this);
        about.setOrientation(LinearLayout.VERTICAL);
        ScrollView aboutScroll = new ScrollView(this);
        aboutScroll.addView(about);
        launcherPages[2] = aboutScroll;
        aboutScroll.setVisibility(View.GONE);
        pageHost.addView(aboutScroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        TextView updateStatus = launcherLabel("", smallPad);
        updates = new UpdateController(this, (message, available) -> {
            updateStatus.setText(message);
            homeUpdate.setText(message);
            homeUpdate.setVisibility(available ? View.VISIBLE : View.GONE);
        });
        about.addView(launcherLabel(updates.description(), smallPad));
        about.addView(updateStatus);
        about.addView(launcherButton(LauncherText.get(LauncherText.CHECK), view -> updates.check(false)));
        Button website = launcherButton(LauncherText.get(LauncherText.WEBSITE), view -> updates.openWebsite());
        website.setEnabled(updates.hasSource());
        about.addView(website);
        about.addView(launcherLabel(LauncherText.get(LauncherText.UPDATE_NOTE), smallPad));
        Switch automatic = new Switch(this);
        automatic.setText(LauncherText.get(LauncherText.AUTO));
        automatic.setTextColor(Color.rgb(218, 238, 220));
        automatic.setChecked(getSharedPreferences("ggfm_startup_options", MODE_PRIVATE).getBoolean("check_updates", true));
        automatic.setOnCheckedChangeListener((button, checked) ->
            getSharedPreferences("ggfm_startup_options", MODE_PRIVATE).edit().putBoolean("check_updates", checked).apply());
        about.addView(automatic);
        about.addView(launcherLabel(LauncherText.get(LauncherText.REPOSITORIES), smallPad));
        about.addView(launcherButton("Guitar Girl Resuscitation", view -> UpdateController.openLink(this, "https://github.com/guitar-girl-resuscitation")));
        for (String repo : new String[]{"server", "patch", "patcher"}) {
            about.addView(launcherButton("memorial-" + repo, view -> UpdateController.openLink(this,
                    "https://github.com/guitar-girl-resuscitation/guitar-girl-memorial-" + repo)));
        }
        LinearLayout tabs = new LinearLayout(this);
        tabs.setOrientation(LinearLayout.HORIZONTAL);
        for (int i = 0; i < 3; i++) {
            final int page = i;
            navigation[i] = launcherButton(LauncherText.get(i), view -> selectLauncherPage(page));
            navigation[i].setTextSize(12);
            tabs.addView(navigation[i], new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        }
        root.addView(tabs);
        selectLauncherPage(0);
        root.setOnApplyWindowInsetsListener((view, insets) -> {
            int top = insets.getSystemWindowInsetTop();
            int bottom = insets.getSystemWindowInsetBottom();
            if (Build.VERSION.SDK_INT >= 28) {
                DisplayCutout cutout = insets.getDisplayCutout();
                if (cutout != null) top = Math.max(top, cutout.getSafeInsetTop());
            }
            view.setPadding(pad, smallPad + top, pad, smallPad + bottom);
            return insets;
        });
        root.requestApplyInsets();
        return root;
    }

    private TextView launcherLabel(String text, int padding) {
        TextView label = new TextView(this);
        label.setText(text);
        label.setTextSize(13);
        label.setTextColor(Color.rgb(218, 238, 220));
        label.setPadding(padding, padding, padding, padding);
        return label;
    }

    private Button launcherButton(String text, View.OnClickListener click) {
        Button button = new Button(this);
        button.setAllCaps(false);
        button.setText(text);
        button.setTextSize(13);
        button.setOnClickListener(click);
        return button;
    }

    private void selectLauncherPage(int selected) {
        for (int i = 0; i < launcherPages.length; i++) {
            launcherPages[i].setVisibility(i == selected ? View.VISIBLE : View.GONE);
            navigation[i].setTextColor(i == selected ? Color.rgb(234, 78, 126) : Color.DKGRAY);
        }
    }

    private void startBootstrap() {
        if (!saveTransfer.freezeForStartup()) return;
        if (!STARTED.compareAndSet(false, true)) {
            appendTerminal("[WARN] bootstrap: startup already running");
            return;
        }
        logEpochMillis = System.currentTimeMillis();
        startButton.setEnabled(false);
        fastStartSwitch.setEnabled(false);
        startButton.setText(words.startingServer);
        try {
            appendBanner();
            mainHandler.postDelayed(() -> {
                logPumpRunning = true;
                mainHandler.post(logPump);
                new Thread(this::bootstrap, "ggfm-visible-bootstrap").start();
            }, cosmeticDelay(900L));
        } catch (Throwable failure) {
            Log.e(TAG, "bootstrap: native banner initialization failed", failure);
            appendError("bootstrap: " + failure.getClass().getSimpleName());
        }
    }

    private void bootstrap() {
        long visibleSince = System.currentTimeMillis();
        try {
            Application app = (Application) getApplicationContext();
            long nowMillis = System.currentTimeMillis();
            long nowSeconds = nowMillis / 1_000L;
            int offsetMinutes = TimeZone.getDefault().getOffset(nowMillis) / 60_000;
            String capability = newCapability();

            append("server: startup requested before Unity initialization");
            Log.i(TAG, "bootstrap: starting embedded server before Unity");
            int result = MemorialNative.startServer(
                    app.getFilesDir().getAbsolutePath(), app.getAssets(), nowSeconds,
                    offsetMinutes, capability);
            if (result != 0) {
                throw new IllegalStateException("native startup result " + result);
            }
            drainServerLogs();
            appendBlankLine();
            pauseMilestone("link    ...... ok");
            pauseMilestone("[INFO] AIRISUTEK TRANSPORT ON / loopback ready");
            appendBlankLine();
            pauseMilestone("asset   ...... ok");
            pauseMilestone("table   ...... ok");
            pauseMilestone("[INFO] AIRISUTEK MEMORY ONLINE / master mounted");
            appendBlankLine();
            pauseMilestone("server  ...... ok");
            pauseMilestone("save    ...... ok");
            pauseMilestone("");
            pauseMilestone("FOUND");
            append("server: startServer returned success; database, identity and loopback ready");
            Log.i(TAG, "bootstrap: server, database, login and USN are ready");

            int sdkResult = MemorialNative.prepareRetiredNativeSdks();
            if (sdkResult != 0) {
                throw new IllegalStateException("retired native SDK Hook result " + sdkResult);
            }
            append("sdk: retired native SDK compatibility installed");
            Log.i(TAG, "bootstrap: retired native SDK compatibility is ready");

            MemorialAdminClient.initialize(MemorialNative.endpoint(), capability);
            MemorialLifecycle.register(app);
            runOnUiThread(() -> patchRuntimeAndLaunch(visibleSince));
        } catch (Throwable failure) {
            Log.e(TAG, "bootstrap failed", failure);
            appendError(words.failed + "\n" + failure.getClass().getSimpleName()
                    + ": " + String.valueOf(failure.getMessage()));
        }
    }

    private void patchRuntimeAndLaunch(long visibleSince) {
        try {
            append("runtime: arming Unity-owned IL2CPP load observer");
            Log.i(TAG, "bootstrap: arming Hooks for Unity-owned IL2CPP load");
            // Unity remains the owner of libil2cpp loading. The one-shot
            // observer installs the complete fail-closed compatibility set
            // as soon as that library becomes available.
            int result = MemorialNative.armRuntimeHooks(false);
            if (result != 0) {
                throw new IllegalStateException("runtime Hook observer result " + result);
            }
            append("runtime: observer armed; gameplay Hooks await Unity IL2CPP load");
            Log.i(TAG, "bootstrap: runtime observer armed; gameplay Hooks are not installed yet");
            appendTerminal("[SCENE] AIRISUTEK PATCH LINK ... ARMED");
            drainServerLogs();
            logPumpRunning = false;
            mainHandler.removeCallbacks(logPump);
            appendBlankLine();
            mainHandler.postDelayed(() -> runFinalRitual(visibleSince, 0), cosmeticDelay(650L));
        } catch (Throwable failure) {
            Log.e(TAG, "runtime patch failed", failure);
            appendError(words.failed + "\n" + failure.getClass().getSimpleName()
                    + ": " + String.valueOf(failure.getMessage()));
        }
    }

    private void scheduleUnityLaunch(long visibleSince) {
        long remaining = cosmeticDelay(MINIMUM_VISIBLE_MILLIS)
                - (System.currentTimeMillis() - visibleSince);
        // Always leave the final welcome line visible for a full second.
        getWindow().getDecorView().postDelayed(this::launchUnity,
                fastStart ? 0L : Math.max(cosmeticDelay(1_000L), remaining));
    }

    private void launchUnity() {
        logPumpRunning = false;
        UNITY_RELEASED = true;
        bringUnityToFront();
    }

    private void bringUnityToFront() {
        Intent intent = new Intent();
        intent.setClassName(this, UNITY_ACTIVITY);
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION
                | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT
                | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        startActivity(intent);
        overridePendingTransition(0, 0);
        finish();
    }

    private void append(String line) {
        Log.i(TAG, line);
        appendTerminal("[INFO] " + line);
    }

    private void appendError(String line) {
        Log.e(TAG, line);
        logPumpRunning = false;
        appendTerminal("[ERROR] " + line);
        runOnUiThread(() -> {
            startButton.setText(words.failed);
            startButton.setEnabled(false);
        });
    }

    private void pauseMilestone(String line) throws InterruptedException {
        if (line.isEmpty()) {
            appendBlankLine();
        } else {
            appendTerminal(line);
        }
        if (!fastStart) Thread.sleep(cosmeticDelay(400L));
    }

    private long cosmeticDelay(long millis) { return TerminalText.cosmeticDelay(millis, fastStart); }

    private void runFinalRitual(long visibleSince, int stage) {
        final String[][] ritual = TerminalText.RITUAL;
        if (fastStart) {
            // This branch is reached only after all real readiness checks succeed.
            for (String[] command : ritual) {
                appendTerminal(command[0]);
                appendTerminal(command[1]);
                appendBlankLine();
            }
            drainServerLogs();
            appendTerminal("Welcome home, Lily!");
            scheduleUnityLaunch(visibleSince);
            return;
        }
        if (stage < ritual.length) {
            typeCommand(visibleSince, stage, visibleLog.length(), 0);
            return;
        }
        drainServerLogs();
        logPumpRunning = false;
        mainHandler.removeCallbacks(logPump);
        appendBlankLine();
        animateWelcomeDivider(visibleSince, visibleLog.length(), 0);
    }

    private void replaceStreamLine(int start, String line) {
        visibleLog.replace(start, visibleLog.length(), line);
        visibleLog.setSpan(new ForegroundColorSpan(Color.rgb(203, 166, 247)),
                start, visibleLog.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        logView.setText(visibleLog);
        logScroll.post(() -> logScroll.fullScroll(View.FOCUS_DOWN));
    }

    private void typeCommand(long since, int stage, int start, int count) {
        String command = TerminalText.RITUAL[stage][0];
        int end = Math.min(command.length(), count + 2);
        replaceStreamLine(start, command.substring(0, end));
        if (end < command.length()) {
            mainHandler.postDelayed(() -> typeCommand(since, stage, start, end), cosmeticDelay(TerminalText.TYPE_FRAME_MS));
        } else {
            appendBlankLine();
            mainHandler.postDelayed(() -> spinCommand(since, stage, visibleLog.length(), 0),
                    cosmeticDelay(TerminalText.COMMAND_OUTPUT_DELAY_MS));
        }
    }

    private void spinCommand(long since, int stage, int start, int frame) {
        if (frame < TerminalText.SPINNER_FRAMES) {
            replaceStreamLine(start, "  " + TerminalText.spinner(frame) + " ...");
            mainHandler.postDelayed(() -> spinCommand(since, stage, start, frame + 1),
                    cosmeticDelay(TerminalText.SPINNER_FRAME_MS));
            return;
        }
        replaceStreamLine(start, TerminalText.RITUAL[stage][1] + "\n\n");
        mainHandler.postDelayed(() -> runFinalRitual(since, stage + 1), cosmeticDelay(TerminalText.NEXT_COMMAND_DELAY_MS));
    }

    private void animateWelcomeDivider(long visibleSince, int start, int step) {
        // Ceremony only, after real readiness checks; never a fabricated server percentage.
        replaceStreamLine(start, TerminalText.progress(step));
        if (step < TerminalText.PROGRESS_STEPS) {
            mainHandler.postDelayed(() -> animateWelcomeDivider(visibleSince, start, step + 1), cosmeticDelay(65L));
        } else {
            mainHandler.postDelayed(() -> {
                appendBlankLine(); appendBlankLine();
                appendTerminal("Welcome home, Lily!");
                scheduleUnityLaunch(visibleSince);
            }, cosmeticDelay(350L));
        }
    }

    private void appendBlankLine() {
        runOnUiThread(() -> {
            visibleLog.append('\n');
            logView.setText(visibleLog);
            logScroll.post(() -> logScroll.fullScroll(View.FOCUS_DOWN));
        });
    }

    private final Runnable logPump = new Runnable() {
        @Override public void run() {
            drainServerLogs();
            if (logPumpRunning) mainHandler.postDelayed(this, 100L);
        }
    };

    private void drainServerLogs() {
        String batch = MemorialNative.drainServerLogs();
        if (batch == null || batch.isEmpty()) return;
        for (String line : batch.split("\\n")) {
            if (!line.isEmpty()) appendTerminal("│ " + line);
        }
    }

    private void appendTerminal(String line) {
        long elapsed = logEpochMillis == 0L
                ? 0L : Math.max(0L, System.currentTimeMillis() - logEpochMillis);
        String timestamp = String.format(Locale.ROOT, "[%02d:%02d.%03d] ",
                elapsed / 60_000L, (elapsed / 1_000L) % 60L, elapsed % 1_000L);
        runOnUiThread(() -> {
            int start = visibleLog.length();
            visibleLog.append(timestamp).append(line).append('\n');
            String severity = TerminalText.severity(line);
            int color = "ERROR".equals(severity) ? Color.rgb(255, 116, 116)
                    : "WARN".equals(severity) ? Color.rgb(255, 203, 107)
                    : "INFO".equals(severity) ? Color.rgb(156, 220, 254)
                    : "WELCOME".equals(severity) ? Color.rgb(203, 166, 247)
                    : Color.rgb(195, 232, 174);
            visibleLog.setSpan(new ForegroundColorSpan(color), start, visibleLog.length(),
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            if (visibleLog.length() > MAX_VISIBLE_LOG_CHARS) {
                int trim = visibleLog.toString().indexOf("\n", visibleLog.length() - MAX_VISIBLE_LOG_CHARS);
                if (trim > 0) visibleLog.delete(0, trim + 1);
            }
            logView.setText(visibleLog);
            logScroll.post(() -> logScroll.fullScroll(View.FOCUS_DOWN));
        });
    }

    private void appendBanner() {
        int columns = bannerView.availableColumns(logScroll.getWidth());
        bannerView.setBanner(MemorialNative.startupBanner(columns));
        bannerScroll.setVisibility(View.VISIBLE);
        visibleLog.clear();
        logView.setText(visibleLog);
        Log.i(TAG, "terminal: Rust ANSI Shadow banner; columns=" + columns
                + " layout=" + (columns >= 66 ? "66-single" : "46-split"));
    }

    private Typeface terminalTypeface() {
        // Some OEM theme engines replace the named MONOSPACE family with a
        // proportional user font. An explicit system font is a custom typeface
        // and does not require bundling a font or any original game resources.
        File font = new File("/system/fonts/DroidSansMono.ttf");
        if (font.isFile() && font.canRead()) {
            try {
                Typeface face = Typeface.createFromFile(font);
                Log.i(TAG, "terminal: explicit DroidSansMono loaded");
                return face;
            } catch (RuntimeException failure) {
                Log.w(TAG, "terminal: explicit monospace unavailable; using platform family", failure);
            }
        }
        return Typeface.MONOSPACE;
    }

    private static String startAction(Locale locale) {
        String language = locale.getLanguage();
        if ("zh".equals(language)) {
            boolean traditional = "TW".equals(locale.getCountry())
                    || "HK".equals(locale.getCountry()) || "MO".equals(locale.getCountry())
                    || "Hant".equals(locale.getScript());
            return traditional ? "啟動紀念版" : "启动纪念版";
        }
        if ("ko".equals(language)) return "메모리얼 에디션 시작";
        if ("ja".equals(language)) return "メモリアル版を起動";
        if ("vi".equals(language)) return "Khởi động bản lưu niệm";
        if ("es".equals(language)) return "Iniciar edición memorial";
        if ("it".equals(language)) return "Avvia edizione memoriale";
        if ("id".equals(language)) return "Mulai edisi memorial";
        if ("th".equals(language)) return "เริ่มฉบับอนุสรณ์";
        if ("pt".equals(language)) return "Iniciar edição memorial";
        if ("hi".equals(language)) return "स्मारक संस्करण शुरू करें";
        return "Start memorial build";
    }

    private static String newCapability() {
        byte[] random = new byte[32];
        new SecureRandom().nextBytes(random);
        StringBuilder value = new StringBuilder(64);
        for (byte item : random) value.append(String.format(Locale.ROOT, "%02x", item & 0xff));
        return value.toString();
    }

    private static final class Words {
        final String title;
        final String preparing;
        final String startingServer;
        final String databaseReady;
        final String loadingRuntime;
        final String hooksReady;
        final String launching;
        final String failed;
        final String alreadyRunning;

        Words(String title, String preparing, String startingServer, String databaseReady,
              String loadingRuntime, String hooksReady, String launching, String failed,
              String alreadyRunning) {
            this.title = title;
            this.preparing = preparing;
            this.startingServer = startingServer;
            this.databaseReady = databaseReady;
            this.loadingRuntime = loadingRuntime;
            this.hooksReady = hooksReady;
            this.launching = launching;
            this.failed = failed;
            this.alreadyRunning = alreadyRunning;
        }

        static Words forLocale(Locale locale) {
            String language = locale.getLanguage();
            if ("zh".equals(language)) {
                boolean traditional = "TW".equals(locale.getCountry())
                        || "HK".equals(locale.getCountry()) || "MO".equals(locale.getCountry());
                return traditional
                    ? new Words("Guitar Girl Fan Memorial Build", "正在準備紀念版環境…",
                        "正在啟動內建伺服器…", "資料庫、存檔身份與本機連接埠已就緒",
                        "正在載入遊戲執行環境…", "客戶端補丁與網路邊界已就緒", "正在進入 Guitar Girl…",
                        "啟動失敗；已停止進入遊戲", "啟動流程已在執行")
                    : new Words("Guitar Girl Fan Memorial Build", "正在准备纪念版环境…",
                        "正在启动内建服务端…", "数据库、存档身份与本机端口已就绪",
                        "正在加载游戏运行环境…", "客户端补丁与网络边界已就绪", "正在进入 Guitar Girl…",
                        "启动失败；已停止进入游戏", "启动流程已在运行");
            }
            if ("ko".equals(language)) return new Words("Guitar Girl Fan Memorial Build",
                "메모리얼 환경 준비 중…", "내장 서버 시작 중…", "데이터베이스와 저장 ID 준비 완료",
                "게임 런타임 로드 중…", "클라이언트 패치와 네트워크 경계 준비 완료", "Guitar Girl 시작 중…",
                "시작 실패; 게임 실행을 중단했습니다", "시작 절차가 이미 실행 중입니다");
            if ("ja".equals(language)) return new Words("Guitar Girl Fan Memorial Build",
                "メモリアル環境を準備中…", "内蔵サーバーを起動中…", "データベースとセーブIDの準備完了",
                "ゲームランタイムを読み込み中…", "クライアントパッチと通信境界の準備完了", "Guitar Girlを起動中…",
                "起動に失敗したためゲームを停止しました", "起動処理は既に実行中です");
            if ("vi".equals(language)) return new Words("Guitar Girl Fan Memorial Build",
                "Đang chuẩn bị bản lưu niệm…", "Đang khởi động máy chủ tích hợp…",
                "Cơ sở dữ liệu và danh tính bản lưu đã sẵn sàng", "Đang tải môi trường trò chơi…", "Bản vá và giới hạn mạng đã sẵn sàng",
                "Đang mở Guitar Girl…", "Khởi động thất bại; trò chơi chưa được mở", "Đang khởi động");
            if ("es".equals(language)) return new Words("Guitar Girl Fan Memorial Build",
                "Preparando la edición memorial…", "Iniciando el servidor integrado…",
                "Base de datos e identidad de guardado listas", "Cargando el entorno del juego…", "Parche y límite de red listos",
                "Iniciando Guitar Girl…", "Error de inicio; el juego no se abrió", "El inicio ya está en curso");
            if ("it".equals(language)) return new Words("Guitar Girl Fan Memorial Build",
                "Preparazione dell'edizione memoriale…", "Avvio del server integrato…",
                "Database e identità di salvataggio pronti", "Caricamento dell'ambiente di gioco…", "Patch e limite di rete pronti",
                "Avvio di Guitar Girl…", "Avvio non riuscito; gioco non aperto", "Avvio già in corso");
            if ("id".equals(language)) return new Words("Guitar Girl Fan Memorial Build",
                "Menyiapkan edisi memorial…", "Memulai server bawaan…",
                "Basis data dan identitas simpanan siap", "Memuat runtime game…", "Patch dan batas jaringan siap",
                "Membuka Guitar Girl…", "Gagal memulai; game tidak dibuka", "Proses mulai sedang berjalan");
            if ("th".equals(language)) return new Words("Guitar Girl Fan Memorial Build",
                "กำลังเตรียมฉบับอนุสรณ์…", "กำลังเริ่มเซิร์ฟเวอร์ในตัว…",
                "ฐานข้อมูลและตัวตนเซฟพร้อมแล้ว", "กำลังโหลดระบบเกม…", "แพตช์และขอบเขตเครือข่ายพร้อมแล้ว",
                "กำลังเปิด Guitar Girl…", "เริ่มไม่สำเร็จ; ยังไม่เปิดเกม", "กำลังเริ่มระบบอยู่");
            if ("pt".equals(language)) return new Words("Guitar Girl Fan Memorial Build",
                "Preparando a edição memorial…", "Iniciando o servidor integrado…",
                "Banco de dados e identidade do save prontos", "Carregando o ambiente do jogo…", "Patch e limite de rede prontos",
                "Abrindo Guitar Girl…", "Falha na inicialização; jogo não aberto", "Inicialização em andamento");
            if ("hi".equals(language)) return new Words("Guitar Girl Fan Memorial Build",
                "स्मारक संस्करण तैयार हो रहा है…", "अंतर्निहित सर्वर शुरू हो रहा है…",
                "डेटाबेस और सेव पहचान तैयार हैं", "गेम रनटाइम लोड हो रहा है…", "क्लाइंट पैच और नेटवर्क सीमा तैयार हैं",
                "Guitar Girl शुरू हो रहा है…", "शुरुआत विफल; गेम नहीं खोला गया", "शुरुआत पहले से चल रही है");
            return new Words("Guitar Girl Fan Memorial Build", "Preparing the memorial runtime…",
                "Starting the embedded server…", "Database, save identity and local port are ready", "Loading the game runtime…",
                "Client patches and network boundary are ready", "Starting Guitar Girl…",
                "Startup failed; the game was not opened", "Startup is already in progress");
        }
    }
}
