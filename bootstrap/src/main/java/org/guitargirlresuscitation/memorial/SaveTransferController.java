package org.guitargirlresuscitation.memorial;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.util.Log;
import android.widget.Button;
import java.io.*;
import java.text.SimpleDateFormat;
import java.util.*;

/** Storage Access Framework only: no broad storage permission, no public raw paths. */
final class SaveTransferController {
    private static final int TREE=7310, IMPORT=7311;
    private static final long LIMIT=256L*1024*1024;
    private final Activity activity;
    private final Button start, export, importSave;
    private final SaveTransferText text;
    private final SharedPreferences preferences;
    private boolean busy, frozen;
    private int pending;

    SaveTransferController(Activity activity, Button start, Button export, Button importSave) {
        this.activity=activity; this.start=start; this.export=export; this.importSave=importSave;
        text=SaveTransferText.forLocale(Locale.getDefault());
        preferences=activity.getSharedPreferences("ggfm-install-storage", 0);
        export.setText(text.export); importSave.setText(text.importSave);
        export.setOnClickListener(v -> choose(1));
        importSave.setOnClickListener(v -> choose(2));
    }
    boolean freezeForStartup() {
        if (busy) return false;
        frozen=true; updateButtons(); return true;
    }
    private void updateButtons() {
        export.setEnabled(!busy && !frozen); importSave.setEnabled(!busy && !frozen);
        if (!frozen) start.setEnabled(!busy);
    }
    private void choose(int action) {
        if (busy || frozen) return;
        pending=action;
        String stored=preferences.getString("tree", "");
        if (stored.isEmpty()) {
            busy=true; updateButtons();
            Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
            intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI,
                Uri.parse("content://com.android.externalstorage.documents/document/primary%3ADocuments"));
            activity.startActivityForResult(intent, TREE);
        } else proceed(Uri.parse(stored));
    }
    boolean onResult(int request, int result, Intent data) {
        if (request!=TREE && request!=IMPORT) return false;
        if (result!=Activity.RESULT_OK || data==null || data.getData()==null) {
            busy=false; updateButtons(); return true;
        }
        if (request==TREE) {
            try {
                Uri tree=data.getData();
                activity.getContentResolver().takePersistableUriPermission(tree,
                    data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION));
                preferences.edit().putString("tree", tree.toString()).commit();
                busy=false; updateButtons();
                proceed(tree);
            } catch (Exception failure) { failed(failure); }
        } else {
            Uri input=data.getData();
            new AlertDialog.Builder(activity).setTitle(text.importSave).setMessage(text.warning)
                .setNegativeButton(android.R.string.cancel, (d,w) -> {busy=false; updateButtons();})
                .setOnCancelListener(d -> {busy=false; updateButtons();})
                .setPositiveButton(android.R.string.ok, (d,w) -> transfer(input, true)).show();
        }
        return true;
    }
    private void proceed(Uri tree) {
        busy=true; updateButtons();
        if (pending==1) transfer(tree, false);
        else {
            Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*");
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, tree);
            activity.startActivityForResult(intent, IMPORT);
        }
    }
    private void transfer(Uri selected, boolean importing) {
        new Thread(() -> {
            File staging=null;
            Uri created=null;
            try {
                // A private staging copy is the INPUT/OUTPUT, never a backup of old saves.
                staging=new File(activity.getCacheDir(), "ggfm-transfer-"+UUID.randomUUID()+".sqlite3");
                if (importing) {
                    try (InputStream input=activity.getContentResolver().openInputStream(selected);
                         FileOutputStream output=new FileOutputStream(staging)) {
                        copy(input, output); output.getFD().sync();
                    }
                }
                int code=MemorialNative.transferSave(activity.getFilesDir().getAbsolutePath(), staging.getAbsolutePath(), importing);
                if (code!=0) throw new IOException("save transfer status="+code);
                String location;
                if (importing) {
                    // Imported SQLite is authoritative. Discard only per-USN UI caches;
                    // keep language/audio/accessibility and installation settings.
                    SharedPreferences prefs=activity.getSharedPreferences(activity.getPackageName()+".v2.playerprefs", 0);
                    SharedPreferences.Editor editor=prefs.edit();
                    for (String key:prefs.getAll().keySet()) if (key.startsWith("ggfm/usn/")) editor.remove(key);
                    if (!editor.commit()) throw new IOException("database imported; player cache reset failed");
                    location=selected.toString();
                } else {
                    Uri root=DocumentsContract.buildDocumentUriUsingTree(selected, DocumentsContract.getTreeDocumentId(selected));
                    Uri folder=directory(selected, directory(selected, root, activity.getPackageName()), "saves");
                    String name="ggfm-"+new SimpleDateFormat("yyyyMMdd-HHmmss", Locale.ROOT).format(new Date())
                        +"-"+UUID.randomUUID().toString().substring(0,8)+".sqlite3";
                    created=DocumentsContract.createDocument(activity.getContentResolver(), folder, "application/octet-stream", name);
                    if (created==null) throw new IOException("document creation failed");
                    try (InputStream input=new FileInputStream(staging);
                         OutputStream output=activity.getContentResolver().openOutputStream(created, "w")) { copy(input, output); }
                    location=DocumentsContract.getTreeDocumentId(selected)+"/"+activity.getPackageName()+"/saves/"+name;
                }
                Log.i("GGFM", importing?"save.import.committed":"save.export.published");
                final String shown=location;
                activity.runOnUiThread(() -> {
                    busy=false; updateButtons();
                    new AlertDialog.Builder(activity).setTitle(text.done).setMessage(shown)
                        .setPositiveButton(android.R.string.ok,null).show();
                });
            } catch (Exception failure) {
                // Only remove this operation's partial export, never user-selected input.
                if (created!=null) try { DocumentsContract.deleteDocument(activity.getContentResolver(), created); } catch (Exception ignored) {}
                failed(failure);
            } finally {
                if (staging!=null && staging.exists() && !staging.delete()) Log.w("GGFM", "save.transfer staging cleanup failed");
            }
        }, "ggfm-save-transfer").start();
    }
    private Uri directory(Uri tree, Uri parent, String name) throws IOException {
        Uri children=DocumentsContract.buildChildDocumentsUriUsingTree(tree, DocumentsContract.getDocumentId(parent));
        try (Cursor c=activity.getContentResolver().query(children, new String[]{
                DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_MIME_TYPE}, null,null,null)) {
            if (c==null) throw new IOException("folder query failed");
            while(c.moveToNext()) if(name.equals(c.getString(1))) {
                if (!DocumentsContract.Document.MIME_TYPE_DIR.equals(c.getString(2))) throw new IOException("folder name is occupied by a file");
                return DocumentsContract.buildDocumentUriUsingTree(tree,c.getString(0));
            }
        }
        Uri made=DocumentsContract.createDocument(activity.getContentResolver(),parent,DocumentsContract.Document.MIME_TYPE_DIR,name);
        if(made==null) throw new IOException("folder creation failed");
        return made;
    }
    private static void copy(InputStream input, OutputStream output) throws IOException {
        if(input==null || output==null) throw new IOException("document stream unavailable");
        byte[] buffer=new byte[65536]; long total=0; int count;
        while((count=input.read(buffer))!=-1) {
            total+=count; if(total>LIMIT) throw new IOException("save exceeds 256 MiB");
            output.write(buffer,0,count);
        }
        if(total==0) throw new IOException("empty save");
        output.flush();
    }
    private void failed(Exception failure) {
        Log.e("GGFM", "save.transfer.failed",failure);
        activity.runOnUiThread(() -> {
            busy=false; updateButtons();
            new AlertDialog.Builder(activity).setTitle(text.error)
                .setMessage(failure.getClass().getSimpleName()).setPositiveButton(android.R.string.ok,null).show();
        });
    }
}
