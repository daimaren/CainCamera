package com.cgfay.media.editor;

import android.view.Surface;

public class VideoEditor {

    public void resume() {
        //todo
    }
    /**
     * change effect
     * @param id effect id
     */
    public void changeEffect(int id) {
        //todo
    }
    /**
     * change effect
     * @param name effect name
     */
    public void changeEffect(String name) {
        //todo
    }

    /**
     * begin effect
     * @param name effect name
     */
    public void beginEffect(String name) {
        //todo
    }

    /**
     * end effect
     * @param name effect name
     */
    public void endEffect(String name) {
        //todo
    }

    public void setVolume(float leftVolume, float rightVolume) {
        //todo
    }
    public native void onSurfaceCreated(final Surface surface);
    public native void onSurfaceDestroyed(final Surface surface);
    public native boolean prepare(String srcFilePath, int width, int height, Surface surface, Boolean isHWDecode);
    public native void pause(); //todo
    public native void play();
    public native boolean isPlaying();//todo
    public native float getPlayProgress();
    public native void seekToPosition(float position);
    public native void seekTo(float msec); //todo
    public native void stop();
    public native void reset();//todo
}
