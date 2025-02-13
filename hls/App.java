package com.example.hls;

import android.app.Activity;
import android.app.PictureInPictureParams;
import android.content.res.Configuration;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.util.Rational;
import android.view.Gravity;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;
import android.widget.FrameLayout;

public class App extends Activity {
    private MediaPlayer player;
    private SurfaceView surfaceView;
    private int videoWidth;
    private int videoHeight;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        FrameLayout frameLayout = new FrameLayout(this);
        frameLayout.setLayoutParams(new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        frameLayout.setBackgroundColor(0xFF000000);

        surfaceView = new SurfaceView(this);
        FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER);
        surfaceView.setLayoutParams(layoutParams);

        frameLayout.addView(surfaceView);
        setContentView(frameLayout);

        initializePlayer();
    }

    private void initializePlayer() {
        SurfaceHolder surfaceHolder = surfaceView.getHolder();
        surfaceHolder.addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                player = new MediaPlayer();
                player.setDisplay(holder);

                player.setOnVideoSizeChangedListener((mp, width, height) -> {
                    videoWidth = width;
                    videoHeight = height;
                    updateVideoSize(surfaceView.getWidth(), surfaceView.getHeight());
                });

                player.setOnPreparedListener(p -> {
                    videoWidth = p.getVideoWidth();
                    videoHeight = p.getVideoHeight();
                    updateVideoSize(surfaceView.getWidth(), surfaceView.getHeight());
                    p.start();
                });

                player.setOnCompletionListener(mp -> releaseMediaPlayer());

                try {
                    player.setDataSource(App.this, Uri.parse("http://192.168.1.187:8000/live/stream.m3u8"));
                    player.prepareAsync();
                } catch (Exception e) {
                    Log.e("HLS", "Failed to set data source or prepare MediaPlayer", e);
                    releaseMediaPlayer();
                }
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                if (player != null) {
                    updateVideoSize(width, height);
                }
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                releaseMediaPlayer();
            }
        });
    }

    private void updateVideoSize(int surfaceWidth, int surfaceHeight) {
        if (player == null || videoWidth == 0 || videoHeight == 0 || surfaceWidth == 0 || surfaceHeight == 0) {
            return;
        }

        float videoAspect = (float) videoWidth / videoHeight;
        float surfaceAspect = (float) surfaceWidth / surfaceHeight;

        int newWidth, newHeight;

        if (videoAspect > surfaceAspect) {
            newWidth = surfaceWidth;
            newHeight = (int) (surfaceWidth / videoAspect);
        } else {
            newHeight = surfaceHeight;
            newWidth = (int) (surfaceHeight * videoAspect);
        }

        ViewGroup.LayoutParams layoutParams = surfaceView.getLayoutParams();
        layoutParams.width = newWidth;
        layoutParams.height = newHeight;
        surfaceView.setLayoutParams(layoutParams);
    }

    private void releaseMediaPlayer() {
        if (player != null) {
            try {
                player.stop();
            } catch (Exception ignored) {
            }
            player.release();
            player = null;
        }
    }


    @Override
    protected void onUserLeaveHint() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            enterPipMode();
        }
    }

    private void enterPipMode() {
        if (player != null && player.isPlaying()) {
            try {
                Rational aspectRatio = new Rational(videoWidth > 0 ? videoWidth : 16, videoHeight > 0 ? videoHeight : 9);
                PictureInPictureParams.Builder pipBuilder = new PictureInPictureParams.Builder();
                pipBuilder.setAspectRatio(aspectRatio);
                enterPictureInPictureMode(pipBuilder.build());
            } catch (Exception e) {
                Log.e("HLS", "Failed to enter Picture-in-Picture mode", e);
            }
        }
    }

    @Override
    public void onPictureInPictureModeChanged(boolean isInPictureInPictureMode, Configuration newConfig) {
        super.onPictureInPictureModeChanged(isInPictureInPictureMode, newConfig);
        if (isInPictureInPictureMode) {
            surfaceView.setKeepScreenOn(false);
        } else {
            surfaceView.setKeepScreenOn(true);
        }
    }
}
