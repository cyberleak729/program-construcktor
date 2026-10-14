package com.appforge.demo;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.util.TypedValue;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.util.Locale;

public class MainActivity extends Activity {

    private int counter = 0;

    // тексты и цвета генерирует конструктор AppForge (pc/software_designer.py)
    private static final String[][] T = {
        {"AppForge Demo", "Hello! Tap the button."},
        {"\u0414\u0435\u043c\u043e AppForge", "\u041f\u0440\u0438\u0432\u0435\u0442! \u041d\u0430\u0436\u043c\u0438 \u043a\u043d\u043e\u043f\u043a\u0443."},
    };
    private static final String[][] BTNS = {
        {"Press me", "Reset"},
        {"\u041d\u0430\u0436\u043c\u0438 \u043c\u0435\u043d\u044f", "\u0421\u0431\u0440\u043e\u0441"},
    };
    private static final int COLOR_BG   = 0xFFF3F4F6;
    private static final int COLOR_BTN1 = 0xFF2563EB;
    private static final int COLOR_BTN2 = 0xFFDC2626;
    private static final int COLOR_TEXT = 0xFF111827;

    private TextView info;
    private boolean ru;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ru = Locale.getDefault().getLanguage().equals("ru");

        ScrollView scroll = new ScrollView(this);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(24), dp(24), dp(24), dp(24));
        root.setBackgroundColor(COLOR_BG | 0xFF000000);

        TextView title = new TextView(this);
        title.setText(ru ? T[1][0] : T[0][0]);
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 22);
        title.setTextColor(COLOR_TEXT | 0xFF000000);
        root.addView(title);

        info = new TextView(this);
        info.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        info.setTextColor(COLOR_TEXT | 0xFF000000);
        info.setPadding(0, dp(12), 0, dp(12));
        root.addView(info);

        Button b1 = mkButton(ru ? BTNS[1][0] : BTNS[0][0], COLOR_BTN1);
        b1.setOnClickListener(v -> { counter++; refresh(); });
        Button b2 = mkButton(ru ? BTNS[1][1] : BTNS[0][1], COLOR_BTN2);
        b2.setOnClickListener(v -> { counter = 0; refresh(); });
        root.addView(b1);
        root.addView(b2);

        scroll.addView(root);
        setContentView(scroll);
        refresh();
    }

    private Button mkButton(String text, int argb) {
        Button b = new Button(this);
        b.setText(text);
        b.setTextColor(Color.WHITE);
        b.setAllCaps(false);
        GradientDrawable g = new GradientDrawable();
        g.setColor(argb | 0xFF000000);
        g.setCornerRadius(dp(12));
        b.setBackground(g);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.setMargins(0, dp(8), 0, dp(8));
        b.setLayoutParams(lp);
        return b;
    }

    private void refresh() {
        info.setText((ru ? "\u041d\u0430\u0436\u0430\u0442\u0438\u0439: " : "Presses: ") + counter);
    }

    private int dp(float v) {
        return Math.round(TypedValue.applyDimension(
            TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics()));
    }
}
