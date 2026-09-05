package org.guitargirlresuscitation.memorial;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.view.View;

/** Fixed-cell drawing prevents fallback box glyphs from changing column widths. */
final class TerminalBannerView extends View {
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private String[] rows = new String[0];
    private final float cell, line, baseline;
    private int columns;

    TerminalBannerView(Context context, Typeface font) {
        super(context);
        paint.setTypeface(font);
        paint.setTextSize(10 * getResources().getDisplayMetrics().scaledDensity);
        paint.setColor(0xffcba6f7);
        cell = paint.measureText("0");
        Paint.FontMetrics metrics = paint.getFontMetrics();
        line = metrics.descent - metrics.ascent;
        baseline = -metrics.ascent;
        int padding = Math.round(12 * getResources().getDisplayMetrics().density);
        setPadding(padding, padding, padding, 0);
    }

    int availableColumns(int width) {
        return Math.max(0, (int) ((width - getPaddingLeft() - getPaddingRight() - 2) / cell));
    }

    void setBanner(String text) {
        rows = text.split("\n", -1);
        columns = 0;
        for (String row : rows) columns = Math.max(columns, row.length());
        requestLayout();
        invalidate();
    }

    @Override protected void onMeasure(int widthSpec, int heightSpec) {
        setMeasuredDimension(resolveSize((int) Math.ceil(columns * cell) + getPaddingLeft()
                + getPaddingRight(), widthSpec), resolveSize((int) Math.ceil(rows.length * line)
                + getPaddingTop() + getPaddingBottom(), heightSpec));
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        for (int row = 0; row < rows.length; row++) {
            for (int col = 0; col < rows[row].length(); col++) {
                String glyph = rows[row].substring(col, col + 1);
                if (" ".equals(glyph)) continue;
                float advance = paint.measureText(glyph);
                canvas.save();
                canvas.translate(getPaddingLeft() + col * cell, getPaddingTop() + row * line);
                // Never widen a thin glyph; compress wide fallback blocks into one cell.
                canvas.scale(Math.min(1.0f, cell / Math.max(1.0f, advance)), 1.0f);
                canvas.drawText(glyph, 0, baseline, paint);
                canvas.restore();
            }
        }
    }
}
