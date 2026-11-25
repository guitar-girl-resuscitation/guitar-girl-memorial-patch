package org.guitargirlresuscitation.memorial;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.Arrays;
import java.util.Comparator;

/** Private journal. Constructing it, listing history, and exporting are read-only. */
final class DiagnosticSessions {
    private static final int HISTORY = 5;
    private static final int PART_BYTES = 65536;
    private final File root;
    private File active;

    DiagnosticSessions(File root) { this.root = root; }

    private File[] runs() {
        File[] runs = root.listFiles(f -> f.isDirectory() && f.getName().matches("[0-9]{1,18}"));
        if (runs == null) return new File[0];
        Arrays.sort(runs, Comparator.comparingLong((File f) -> Long.parseLong(f.getName())).reversed());
        return runs;
    }

    synchronized void start(String initial, long now) throws IOException {
        if (active != null) return;
        File[] previous = runs();
        long next = previous.length == 0 ? 1 : Long.parseLong(previous[0].getName()) + 1;
        File dir = new File(root, Long.toString(next));
        if (!dir.mkdirs()) throw new IOException("cannot create diagnostic session");
        active = dir;
        appendFile(new File(dir, "startup"), "session.started unix_ms=" + now + "\n" + initial);
        for (int i = HISTORY; i < previous.length; i++) {
            for (String name : new String[]{"startup", "older", "recent"})
                Files.deleteIfExists(new File(previous[i], name).toPath());
            Files.deleteIfExists(previous[i].toPath());
        }
    }

    private static void appendFile(File file, String value) throws IOException {
        try (FileOutputStream out = new FileOutputStream(file, true)) {
            out.write(value.getBytes(StandardCharsets.UTF_8));
        }
    }

    synchronized void startupLine(String line) throws IOException {
        if (active == null) return;
        File part = new File(active, "startup");
        if (part.length() < PART_BYTES) appendFile(part, line + "\n");
    }

    synchronized void event(String message) throws IOException {
        if (active == null) return;
        File recent = new File(active, "recent");
        if (recent.length() >= PART_BYTES) Files.move(recent.toPath(), new File(active,"older").toPath(),
                StandardCopyOption.REPLACE_EXISTING);
        appendFile(recent, System.currentTimeMillis() + " " +
                message.substring(0, Math.min(220, message.length())) + "\n");
    }

    private static String read(File dir, String part) throws IOException {
        File file = new File(dir, part);
        if (!file.isFile()) return "";
        try (RandomAccessFile in = new RandomAccessFile(file, "r")) {
            int size = (int)Math.min(in.length(), PART_BYTES + 2048);
            in.seek(in.length() - size);
            byte[] bytes = new byte[size];
            in.readFully(bytes);
            return new String(bytes, StandardCharsets.UTF_8);
        }
    }

    private static String events(File dir) throws IOException {
        return read(dir, "older") + read(dir, "recent");
    }

    synchronized String current() throws IOException { return active == null ? "" : events(active); }

    synchronized String history(String legacy) throws IOException {
        StringBuilder report = new StringBuilder();
        int count = 0;
        for (File run : runs()) {
            if (run.equals(active)) continue;
            report.append("=== Session ").append(run.getName()).append(" ===\n")
                    .append(read(run, "startup")).append('\n').append(events(run)).append('\n');
            if (++count == HISTORY) break;
        }
        if (count < HISTORY && !legacy.isEmpty()) report.append("=== Legacy startup ===\n").append(legacy);
        return report.toString();
    }
}
