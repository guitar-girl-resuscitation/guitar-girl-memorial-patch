package org.guitargirlresuscitation.memorial;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.EditText;
import android.text.InputFilter;
import android.text.InputType;

/** Independent, resource-free memorial panel hosted by the existing Unity Activity. */
final class MemorialPanel {
    private static Dialog panel;
    private static AlertDialog inputDialog;
    private static Activity owner;
    private static final String[] ACTIONS = {
        "OnUIEventLogin_GooglePlay", "OnUIEventRestoreProducts",
        "OnUIEventHelp", "OnUIEventPrivacy_GDPR"
    };
    private static final int INK = Color.rgb(82, 48, 61);
    private static final int ACCENT = Color.rgb(218, 67, 113);

    private static void game(String method) {
        try {
            Class.forName("com.unity3d.player.UnityPlayer")
                .getMethod("UnitySendMessage", String.class, String.class, String.class)
                .invoke(null, "GgUIInGameSettingManager", method, "");
        } catch (ReflectiveOperationException error) {
            Log.e("GGFM", "ui: cannot dispatch memorial action " + method, error);
        }
    }

    private static int dp(Activity activity, int value) {
        return Math.round(activity.getResources().getDisplayMetrics().density * value);
    }

    private static GradientDrawable background(int color, int radius) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color);
        drawable.setCornerRadius(radius);
        return drawable;
    }

    static void show(String title, String[] labels, String close) {
        render(title, labels, close, null, null, 0);
    }

    static void showList(String title, String[] labels, boolean[] enabled,
                         String back, String close, long generation) {
        if (enabled.length != labels.length || generation <= 0) return;
        render(title, labels, close, enabled, back, generation);
    }

    private static void render(String title, String[] labels, String close,
                               boolean[] enabled, String back, long generation) {
        Activity activity = MemorialLifecycle.gameActivity();
        if (activity == null || activity.isFinishing() || activity.isDestroyed()) return;
        activity.runOnUiThread(() -> {
            if (activity.isFinishing() || activity.isDestroyed()) return;
            // Reuse one window across pages; no Activity launch/pause/resume cycle.
            if (panel == null || owner != activity) {
                if (panel != null) panel.dismiss();
                panel = new Dialog(activity);
                owner = activity;
                panel.requestWindowFeature(Window.FEATURE_NO_TITLE);
                panel.setCanceledOnTouchOutside(false);
                panel.setOnCancelListener(ignored -> game("OnUIEventClose"));
            }
            LinearLayout content = new LinearLayout(activity);
            content.setOrientation(LinearLayout.VERTICAL);
            int pad = dp(activity, 20);
            content.setPadding(pad, pad, pad, pad);
            content.setBackground(background(Color.rgb(255, 247, 250), dp(activity, 20)));
            TextView heading = new TextView(activity);
            heading.setText(title);
            heading.setTextColor(ACCENT);
            heading.setTextSize(22);
            heading.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
            content.addView(heading);
            TextView brand = new TextView(activity);
            brand.setText("Guitar Girl · Fan Memorial Build");
            brand.setTextColor(INK);
            brand.setTextSize(12);
            brand.setPadding(0, dp(activity, 6), 0, dp(activity, 18));
            content.addView(brand);
            if (enabled != null) {
                Button previous = button(activity, "‹  " + back);
                previous.setOnClickListener(view -> game(ACTIONS[0]));
                content.addView(previous, new LinearLayout.LayoutParams(-1, -2));
            }
            ScrollView scroll = new ScrollView(activity);
            LinearLayout rows = new LinearLayout(activity);
            rows.setOrientation(LinearLayout.VERTICAL);
            for (int i = 0; i < labels.length && (enabled != null || i < ACTIONS.length); ++i) {
                // The independent panel already has one persistent close button.
                if (labels[i].equals(close)) continue;
                final int index = i;
                final String method = enabled == null ? ACTIONS[i] : ACTIONS[2];
                Button button = button(activity, labels[i]);
                button.setEnabled(enabled == null || enabled[i]);
                button.setAlpha(enabled == null || enabled[i] ? 1.0f : 0.5f);
                button.setOnClickListener(view -> {
                    if (enabled != null && !MemorialNative.queueLegacySelection(generation, index)) return;
                    // Block duplicate submissions while Unity is handling this action.
                    for (int j = 0; j < rows.getChildCount(); ++j) rows.getChildAt(j).setEnabled(false);
                    game(method);
                    rows.postDelayed(() -> {
                        for (int j = 0; j < rows.getChildCount(); ++j)
                            rows.getChildAt(j).setEnabled(enabled == null || enabled[j]);
                    }, 1000);
                });
                LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, -2);
                params.bottomMargin = dp(activity, 10);
                rows.addView(button, params);
            }
            scroll.addView(rows);
            content.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
            Button exit = button(activity, "×  " + close);
            exit.setOnClickListener(view -> { dismiss(); game("OnUIEventClose"); });
            content.addView(exit, new LinearLayout.LayoutParams(-1, -2));
            panel.setContentView(content);
            panel.show();
            Window window = panel.getWindow();
            if (window != null) {
                window.setBackgroundDrawableResource(android.R.color.transparent);
                window.setGravity(Gravity.CENTER);
                window.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
                window.setDimAmount(0.5f);
                int width = Math.min(dp(activity, 500), activity.getResources().getDisplayMetrics().widthPixels - dp(activity, 32));
                int height = (int)(activity.getResources().getDisplayMetrics().heightPixels * 0.56f);
                window.setLayout(width, height);
            }
            Log.i("GGFM", "ui: independent memorial page visible");
        });
    }

    private static Button button(Activity activity, String label) {
        Button button = new Button(activity);
        button.setText(label);
        button.setAllCaps(false);
        button.setTextSize(16);
        button.setTextColor(INK);
        button.setMinHeight(dp(activity, 54));
        button.setPadding(dp(activity, 12), dp(activity, 10), dp(activity, 12), dp(activity, 10));
        button.setBackground(background(Color.WHITE, dp(activity, 12)));
        return button;
    }

    static void currencyInput(String currency, CurrencyText text, long generation) {
        Activity activity = MemorialLifecycle.gameActivity();
        int index = CurrencyText.index(currency);
        if (activity == null || index < 0 || generation <= 0) return;
        activity.runOnUiThread(() -> {
            if (activity.isFinishing() || activity.isDestroyed()) return;
            if (inputDialog != null) inputDialog.dismiss();
            EditText input = new EditText(activity);
            input.setSingleLine(true);
            input.setFilters(new InputFilter[]{new InputFilter.LengthFilter(64)});
            input.setInputType(InputType.TYPE_CLASS_NUMBER);
            input.setHint(CurrencyText.isMultiplier(currency) ? "1" : "100");
            LinearLayout content = new LinearLayout(activity);
            content.setOrientation(LinearLayout.VERTICAL);
            int pad = dp(activity, 20);
            content.setPadding(pad, pad, pad, pad);
            TextView description = new TextView(activity);
            description.setText(CurrencyText.isMultiplier(currency) ? text.multiplier : text.count);
            content.addView(description);
            content.addView(input);
            AlertDialog dialog = new AlertDialog.Builder(activity)
                .setTitle(text.names[index]).setView(content)
                .setPositiveButton(text.send, null)
                .setNegativeButton(text.back, (ignored, which) -> game(ACTIONS[0]))
                .create();
            inputDialog = dialog;
            dialog.setCanceledOnTouchOutside(false);
            dialog.setOnCancelListener(ignored -> game(ACTIONS[0]));
            dialog.setOnDismissListener(ignored -> { if (inputDialog == dialog) inputDialog = null; });
            dialog.setOnShowListener(ignored -> dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(view -> {
                String amount = input.getText().toString().trim();
                if (!CurrencyText.validSyntax(currency, amount)) { input.setError(text.invalid); return; }
                if (!MemorialNative.queueCurrencyAmount(generation, amount)) { input.setError(text.invalid); return; }
                dialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(false);
                dialog.dismiss();
                game(ACTIONS[2]);
            }));
            dialog.show();
        });
    }

    static void message(String title, String body, String close) {
        Activity activity = MemorialLifecycle.gameActivity();
        if (activity == null || activity.isFinishing() || activity.isDestroyed()) return;
        activity.runOnUiThread(() -> new AlertDialog.Builder(activity)
            .setTitle(title).setMessage(body).setPositiveButton(close, null).show());
    }

    static void dismiss() {
        Activity activity = MemorialLifecycle.gameActivity();
        if (activity != null) activity.runOnUiThread(() -> {
            if (inputDialog != null) { inputDialog.dismiss(); inputDialog = null; }
            if (panel != null) { panel.dismiss(); panel = null; owner = null; }
        });
    }

    private MemorialPanel() {}
}
