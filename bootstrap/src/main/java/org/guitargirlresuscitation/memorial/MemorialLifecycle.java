package org.guitargirlresuscitation.memorial;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;

import java.util.concurrent.atomic.AtomicBoolean;
import java.lang.ref.WeakReference;

/** Flushes the transactional store whenever the app leaves the foreground. */
final class MemorialLifecycle implements Application.ActivityLifecycleCallbacks {
    private static final MemorialLifecycle INSTANCE = new MemorialLifecycle();
    private final AtomicBoolean registered = new AtomicBoolean();
    private int startedActivities;
    private static WeakReference<Activity> gameActivity = new WeakReference<>(null);

    static Activity gameActivity() { return gameActivity.get(); }

    static void register(Application application) {
        if (INSTANCE.registered.compareAndSet(false, true)) {
            application.registerActivityLifecycleCallbacks(INSTANCE);
        }
    }

    @Override public synchronized void onActivityStarted(Activity activity) {
        startedActivities += 1;
    }

    @Override public synchronized void onActivityStopped(Activity activity) {
        startedActivities = Math.max(0, startedActivities - 1);
        if (startedActivities == 0 && !activity.isChangingConfigurations()) {
            new Thread(MemorialNative::prepareShutdown, "ggfm-flush").start();
        }
    }

    @Override public void onActivityCreated(Activity activity, Bundle state) {}
    @Override public void onActivityResumed(Activity activity) {
        if (activity.getClass().getName().equals("com.google.firebase.MessagingUnityPlayerActivity")) {
            gameActivity = new WeakReference<>(activity);
        }
    }
    @Override public void onActivityPaused(Activity activity) {}
    @Override public void onActivitySaveInstanceState(Activity activity, Bundle state) {}
    @Override public void onActivityDestroyed(Activity activity) {
        if (gameActivity.get() == activity) {
            MemorialPanel.dismiss();
            gameActivity.clear();
        }
    }

    private MemorialLifecycle() {}
}
