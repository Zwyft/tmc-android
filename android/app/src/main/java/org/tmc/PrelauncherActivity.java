package org.tmc;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.OpenableColumns;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.ScrollView;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.Drawable;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.security.MessageDigest;

public class PrelauncherActivity extends Activity {
    private static final String PREFS_NAME = "tmc_prefs";
    private static final String KEY_ROM_PATH = "rom_path";
    private static final String KEY_ROM_SHA1 = "rom_sha1";

    private static final int PICK_ROM_REQUEST = 1;

    private TextView statusText;
    private TextView romInfoText;
    private Button selectRomButton;
    private Button playButton;
    private LinearLayout container;
    private ProgressBar progressBar;
    private TextView appTitle;
    private View statusIndicator;

    private String selectedRomPath;
    private String selectedRomSha1;

    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER_HORIZONTAL);
        root.setPadding(dp(32), dp(48), dp(32), dp(32));
        root.setBackgroundColor(Color.parseColor("#1a1a2e"));

        appTitle = new TextView(this);
        appTitle.setText("Minish Cap");
        appTitle.setTextSize(32);
        appTitle.setTypeface(null, Typeface.BOLD);
        appTitle.setTextColor(Color.parseColor("#e0d8b0"));
        appTitle.setGravity(Gravity.CENTER);
        appTitle.setPadding(0, 0, 0, dp(4));

        TextView subtitle = new TextView(this);
        subtitle.setText("Android Port");
        subtitle.setTextSize(14);
        subtitle.setTextColor(Color.parseColor("#8a8468"));
        subtitle.setGravity(Gravity.CENTER);
        subtitle.setPadding(0, 0, 0, dp(32));

        container = new LinearLayout(this);
        container.setOrientation(LinearLayout.VERTICAL);
        container.setGravity(Gravity.CENTER);
        container.setBackgroundColor(Color.parseColor("#16213e"));
        container.setPadding(dp(24), dp(24), dp(24), dp(24));
        {
            GradientDrawable bg = new GradientDrawable();
            bg.setColor(Color.parseColor("#16213e"));
            bg.setCornerRadius(dp(12));
            container.setBackground(bg);
        }
        LinearLayout.LayoutParams containerLp = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        );
        containerLp.setMargins(0, 0, 0, dp(16));

        statusIndicator = new View(this);
        statusIndicator.setLayoutParams(new LinearLayout.LayoutParams(dp(12), dp(12)));
        {
            GradientDrawable circle = new GradientDrawable();
            circle.setShape(GradientDrawable.OVAL);
            circle.setColor(Color.parseColor("#555555"));
            statusIndicator.setBackground(circle);
        }

        statusText = new TextView(this);
        statusText.setText("No ROM selected");
        statusText.setTextSize(14);
        statusText.setTextColor(Color.parseColor("#888888"));

        LinearLayout statusRow = new LinearLayout(this);
        statusRow.setOrientation(LinearLayout.HORIZONTAL);
        statusRow.setGravity(Gravity.CENTER_VERTICAL);
        statusRow.setPadding(0, 0, 0, dp(16));
        statusRow.addView(statusIndicator);
        {
            LinearLayout.LayoutParams margin = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            );
            margin.setMargins(dp(8), 0, 0, 0);
            statusRow.addView(statusText, margin);
        }
        container.addView(statusRow);

        romInfoText = new TextView(this);
        romInfoText.setTextSize(13);
        romInfoText.setTextColor(Color.parseColor("#a0a0a0"));
        romInfoText.setGravity(Gravity.CENTER);
        romInfoText.setPadding(0, 0, 0, dp(16));
        romInfoText.setVisibility(View.GONE);
        container.addView(romInfoText);

        progressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progressBar.setVisibility(View.GONE);
        LinearLayout.LayoutParams progLp = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, dp(4)
        );
        progLp.setMargins(0, 0, 0, dp(16));
        container.addView(progressBar, progLp);

        selectRomButton = new Button(this);
        selectRomButton.setText("SELECT ROM FILE");
        styleButton(selectRomButton, "#0f3460", "#e0d8b0");
        selectRomButton.setOnClickListener(v -> openRomPicker());
        container.addView(selectRomButton);

        {
            LinearLayout.LayoutParams margin = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            );
            margin.setMargins(0, dp(12), 0, 0);
            playButton = new Button(this);
            playButton.setText("PLAY");
            styleButton(playButton, "#533483", "#e0d8b0");
            playButton.setEnabled(false);
            playButton.setAlpha(0.4f);
            playButton.setOnClickListener(v -> launchGame());
            container.addView(playButton, margin);
        }

        root.addView(appTitle);
        root.addView(subtitle);
        root.addView(container, containerLp);

        setContentView(root);
        hideSystemBars();

        checkExistingRom();
    }

    private void styleButton(Button btn, String bgColor, String textColor) {
        btn.setTextColor(Color.parseColor(textColor));
        btn.setTextSize(15);
        btn.setTypeface(null, Typeface.BOLD);
        btn.setPadding(dp(20), dp(14), dp(20), dp(14));
        btn.setAllCaps(true);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.parseColor(bgColor));
        bg.setCornerRadius(dp(8));
        btn.setBackground(bg);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, dp(48)
        );
        btn.setLayoutParams(lp);
    }

    private void hideSystemBars() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController ctrl = getWindow().getDecorView().getWindowInsetsController();
            if (ctrl != null) {
                ctrl.hide(WindowInsets.Type.systemBars());
                ctrl.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            );
        }
    }

    private void checkExistingRom() {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String savedPath = prefs.getString(KEY_ROM_PATH, null);
        if (savedPath != null && new File(savedPath).exists()) {
            selectedRomPath = savedPath;
            selectedRomSha1 = prefs.getString(KEY_ROM_SHA1, null);
            updateRomInfo();
            setStatus("ROM ready", "#4ecca3", "#4ecca3");
            enablePlayButton();
        }
    }

    private void openRomPicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        String[] mimeTypes = {"application/octet-stream", "application/gba", "*/*"};
        intent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes);
        startActivityForResult(intent, PICK_ROM_REQUEST);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == PICK_ROM_REQUEST && resultCode == RESULT_OK && data != null) {
            Uri uri = data.getData();
            if (uri != null) {
                int takeFlags = Intent.FLAG_GRANT_READ_URI_PERMISSION;
                try {
                    getContentResolver().takePersistableUriPermission(uri, takeFlags);
                } catch (SecurityException e) {
                }
                copyRomToInternal(uri);
            }
        }
    }

    private void copyRomToInternal(Uri uri) {
        setStatus("Copying ROM...", "#f0c040", "#f0c040");
        progressBar.setVisibility(View.VISIBLE);
        progressBar.setIndeterminate(true);
        selectRomButton.setEnabled(false);

        new Thread(() -> {
            try {
                String displayName = getDisplayName(uri);
                if (!displayName.toLowerCase().endsWith(".gba")) {
                    displayName = "baserom.gba";
                }

                File outFile = new File(getFilesDir(), displayName);
                try (InputStream in = getContentResolver().openInputStream(uri);
                     FileOutputStream out = new FileOutputStream(outFile)) {
                    byte[] buf = new byte[8192];
                    int len;
                    long total = 0;
                    while ((len = in.read(buf)) > 0) {
                        out.write(buf, 0, len);
                        total += len;
                    }
                    out.flush();
                }

                String sha1 = computeSha1(outFile);
                String detectedRegion = detectRegion(sha1);
                if (detectedRegion == null) {
                    detectedRegion = detectRegionBySize(new File(getFilesDir(), displayName));
                }

                mainHandler.post(() -> {
                    selectedRomPath = outFile.getAbsolutePath();
                    selectedRomSha1 = sha1;
                    saveRomPrefs(selectedRomPath, sha1);
                    progressBar.setVisibility(View.GONE);
                    selectRomButton.setEnabled(true);
                    updateRomInfo();
                    setStatus("ROM ready", "#4ecca3", "#4ecca3");
                    enablePlayButton();
                });

                if (!displayName.equals("baserom.gba")) {
                    File target = new File(getFilesDir(), "baserom.gba");
                    copyFile(outFile, target);
                }

            } catch (Exception e) {
                mainHandler.post(() -> {
                    progressBar.setVisibility(View.GONE);
                    selectRomButton.setEnabled(true);
                    setStatus("Error: " + e.getMessage(), "#e74c3c", "#e74c3c");
                });
            }
        }).start();
    }

    private void copyFile(File src, File dst) throws IOException {
        try (FileInputStream in = new FileInputStream(src);
             FileOutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[8192];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
        }
    }

    private void updateRomInfo() {
        if (selectedRomPath != null) {
            File f = new File(selectedRomPath);
            long sizeMB = f.length() / (1024 * 1024);
            String region = "Unknown";
            if (selectedRomSha1 != null) {
                String r = detectRegion(selectedRomSha1);
                if (r != null) region = r;
            }
            if ("Unknown".equals(region)) {
                region = detectRegionBySize(f);
            }
            romInfoText.setText(String.format("%s  •  %d MB  •  %s",
                f.getName(), sizeMB, region));
            romInfoText.setVisibility(View.VISIBLE);
        }
    }

    private String detectRegion(String sha1) {
        if (sha1 == null) return null;
        String s = sha1.toLowerCase();
        if (s.equals("b4bd50e4131b027c334547b4524e2dbbd4227130")) return "USA";
        if (s.equals("cff199b36ff173fb6faf152653d1bccf87c26fb7")) return "EU";
        if (s.equals("6c5404a1effb17f481f352181d0f1c61a2765c5d")) return "JP";
        return null;
    }

    private String detectRegionBySize(File f) {
        long len = f.length();
        if (len == 0x1000000 || len == 0x2000000) return "USA/EU";
        if (len == 0x1000000) return "JP";
        return String.format("ROM (%d MB)", len / (1024 * 1024));
    }

    private String computeSha1(File f) throws Exception {
        MessageDigest md = MessageDigest.getInstance("SHA-1");
        try (FileInputStream in = new FileInputStream(f)) {
            byte[] buf = new byte[8192];
            int len;
            while ((len = in.read(buf)) > 0) {
                md.update(buf, 0, len);
            }
        }
        byte[] digest = md.digest();
        StringBuilder sb = new StringBuilder();
        for (byte b : digest) sb.append(String.format("%02x", b));
        return sb.toString();
    }

    private String getDisplayName(Uri uri) {
        try (Cursor cursor = getContentResolver().query(
                uri, null, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int idx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (idx >= 0) return cursor.getString(idx);
            }
        } catch (Exception e) {
        }
        String path = uri.getPath();
        return path != null ? path.substring(path.lastIndexOf('/') + 1) : "baserom.gba";
    }

    private void setStatus(String text, String indicatorColor, String textColor) {
        GradientDrawable circle = new GradientDrawable();
        circle.setShape(GradientDrawable.OVAL);
        circle.setColor(Color.parseColor(indicatorColor));
        statusIndicator.setBackground(circle);
        statusText.setText(text);
        statusText.setTextColor(Color.parseColor(textColor));
    }

    private void enablePlayButton() {
        playButton.setEnabled(true);
        playButton.setAlpha(1.0f);
    }

    private void saveRomPrefs(String path, String sha1) {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
            .edit()
            .putString(KEY_ROM_PATH, path)
            .putString(KEY_ROM_SHA1, sha1)
            .apply();
    }

    private void launchGame() {
        if (selectedRomPath == null) return;
        Intent intent = new Intent(this, GameActivity.class);
        intent.putExtra("rom_path", selectedRomPath);
        startActivity(intent);
    }

    private int dp(int dp) {
        DisplayMetrics metrics = getResources().getDisplayMetrics();
        return (int) (dp * metrics.density + 0.5f);
    }
}
