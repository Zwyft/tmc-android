package org.tmc;

import android.app.Activity;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class TMCActivity extends Activity implements SurfaceHolder.Callback {
    private SurfaceView surfaceView;

    static {
        System.loadLibrary("tmc");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        surfaceView = new SurfaceView(this);
        surfaceView.getHolder().addCallback(this);
        surfaceView.setFocusable(true);
        surfaceView.setFocusableInTouchMode(true);
        setContentView(surfaceView);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        nativeSetSurface(holder.getSurface());
        nativeSetAssetManager(getAssets());
        nativeSetFilesDir(getFilesDir().getAbsolutePath());
        nativeSetApkPath(getApplicationInfo().sourceDir);
        new Thread(() -> nativeMain()).start();
    }

    @Override public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        nativeOnResize(w, h);
    }

    @Override public void surfaceDestroyed(SurfaceHolder holder) {
        nativeOnSurfaceDestroyed();
    }

    @Override
    protected void onPause() {
        super.onPause();
        nativeOnPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        nativeOnResume();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        final int idx = event.getActionIndex();
        final int id = event.getPointerId(idx);
        final float x = event.getX(idx);
        final float y = event.getY(idx);
        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                nativeTouchDown(id, (int)x, (int)y);
                break;
            case MotionEvent.ACTION_MOVE:
                for (int i = 0; i < event.getPointerCount(); i++) {
                    nativeTouchMove(event.getPointerId(i),
                        (int)event.getX(i), (int)event.getY(i));
                }
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                nativeTouchUp(id, (int)x, (int)y);
                break;
        }
        return true;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        nativeKeyDown(keyCode);
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        nativeKeyUp(keyCode);
        return true;
    }

    private static native void nativeSetSurface(Surface surface);
    private static native void nativeSetAssetManager(AssetManager mgr);
    private static native void nativeSetFilesDir(String path);
    private static native void nativeSetApkPath(String path);
    private static native void nativeMain();
    private static native void nativeOnPause();
    private static native void nativeOnResume();
    private static native void nativeOnResize(int w, int h);
    private static native void nativeOnSurfaceDestroyed();
    private static native void nativeTouchDown(int id, int x, int y);
    private static native void nativeTouchMove(int id, int x, int y);
    private static native void nativeTouchUp(int id, int x, int y);
    private static native void nativeKeyDown(int keyCode);
    private static native void nativeKeyUp(int keyCode);
}
