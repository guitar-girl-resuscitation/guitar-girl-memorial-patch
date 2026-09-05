package org.guitargirlresuscitation.memorial;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import java.util.ArrayList;

/** Offline price-query endpoint; never starts Billing or submits a purchase. */
public final class LocalPriceActivity extends Activity {
    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        try {
            // The stock bridge encodes an empty non-null price dictionary and
            // delivers the registered onPriceLocalization callback. An empty
            // list also skips its retired remote price-cache upload branch.
            // Memorial purchases already use the separate local server flow;
            // do not invent real-money prices or receipts here.
            Class<?> bridge = Class.forName("com.pmangplus.ui.internal.JSONManager",
                    true, getClassLoader());
            bridge.getMethod("invokeOnLocalPrice", ArrayList.class, String.class)
                    .invoke(null, new ArrayList<String>(), "");
            Log.i("GGFM", "sdk: offline price query completed; no Billing activity or remote price cache");
        } catch (ReflectiveOperationException failure) {
            Log.e("GGFM", "sdk: offline price callback contract failed", failure);
        } finally {
            finish();
            overridePendingTransition(0, 0);
        }
    }
}
