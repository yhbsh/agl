package com.example.activity;

import android.app.Activity;
import android.os.Bundle;
import android.content.Context;
import android.view.Gravity;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.SurfaceHolder;

public class MainActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(new CustomSurfaceView(this));
        // setContentView(new CustomTextureView(this));
    }
}

class CustomSurfaceView extends SurfaceView implements SurfaceHolder.Callback {
    private boolean running = false;
    private Thread renderThread;

    static {
        System.loadLibrary("engine");
    }

    public CustomSurfaceView(Context context) {
        super(context);
        getHolder().addCallback(this);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        init(holder.getSurface());
        startRendering();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        stopRendering();
        deinit();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}

    private void startRendering() {
        running = true;
        renderThread = new Thread(() -> {
            while (running) {
                step(getWidth(), getHeight());
            }
        });
        renderThread.start();
    }

    private void stopRendering() {
        running = false;
        if (renderThread != null) {
            try {
                renderThread.join();
            } catch (InterruptedException ignored) {}
            renderThread = null;
        }
    }

    private native void init(Surface surface);
    private native void step(int width, int height);
    private native void deinit();
}
