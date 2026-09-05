package org.guitargirlresuscitation.memorial;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.util.Log;

/** Establishes the pre-Activity bootstrap boundary without starting Unity. */
public final class MemorialBootstrapProvider extends ContentProvider {
    private static final String TAG = "GGFM";

    @Override public boolean onCreate() {
        Context context = getContext();
        if (context == null) return false;
        Log.i(TAG, "bootstrap: provider ready; waiting for visible startup activity");
        return true;
    }

    @Override public Cursor query(Uri uri, String[] projection, String selection,
                                  String[] selectionArgs, String sortOrder) { return null; }
    @Override public String getType(Uri uri) { return null; }
    @Override public Uri insert(Uri uri, ContentValues values) { return null; }
    @Override public int delete(Uri uri, String selection, String[] selectionArgs) { return 0; }
    @Override public int update(Uri uri, ContentValues values, String selection,
                                String[] selectionArgs) { return 0; }
}
