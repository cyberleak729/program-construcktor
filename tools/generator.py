#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AppForge generator — project scaffolding for PC (C++/C#/Python) and mobile (Java/Kotlin/Swift).
Pure stdlib. CLI:  python3 generator.py <project.json> <out_dir>
"""
import json, os, sys, shutil, subprocess

LANGS = ["cpp", "csharp", "python", "java", "kotlin", "swift"]

def jstr(s):
    # \uXXXX escapes -> safe inside Java/C++/Swift string literals
    return json.dumps(str(s), ensure_ascii=True)

jsstr = jstr

def write(path, content):
    d = os.path.dirname(path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)

def hex2rgb(hexs):
    v = int(hexs.lstrip("#"), 16)
    return (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF

def tr(cfg, table, idx, key=None, lang=None):
    """localized string from dict {lang: text}"""
    lang = lang or cfg.get("ui_lang", "en")
    item = table[idx]
    d = item[key] if key else item
    return d.get(lang, d.get("en", ""))

# ---------------- PC: C++ WinAPI ----------------
def gen_cpp(cfg, out):
    app = cfg["app_name"]
    texts = [tr(cfg, cfg["texts"], i) for i in range(len(cfg["texts"]))]
    btns = [(tr(cfg, cfg["buttons"], i, "label"), hex2rgb(b.get("color", "#2563EB")))
            for i, b in enumerate(cfg["buttons"])]
    lines = []
    lines.append('#include <windows.h>\n\n')
    lines.append('static const wchar_t* TITLE    = L%s;\n' % jstr(texts[0]))
    lines.append('static const wchar_t* SUBTITLE = L%s;\n' % jstr(texts[1] if len(texts) > 1 else ""))
    lines.append('static const wchar_t* COUNT_FMT = L"Presses: %%d";\n\n')
    lines.append('struct BtnDef { const wchar_t* label; COLORREF color; };\n')
    lines.append('static BtnDef g_btns[] = {\n')
    for lbl, (r, g, b) in btns:
        lines.append('    { L%s, RGB(%d,%d,%d) },\n' % (jstr(lbl), r, g, b))
    lines.append('};\n')
    lines.append('static const int NBTN = (int)(sizeof(g_btns)/sizeof(g_btns[0]));\n')
    lines.append('static int  g_counter = 0;\n')
    lines.append('static HWND g_info = 0;\n\n')
    lines.append(r'''
static void Refresh(){
    wchar_t buf[128];
    wsprintfW(buf, COUNT_FMT, g_counter);
    SetWindowTextW(g_info, buf);
}
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE: {
        HFONT f = CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,
                              CLEARTYPE_QUALITY,0,L"Segoe UI");
        HWND t = CreateWindowExW(0,L"STATIC",SUBTITLE,WS_CHILD|WS_VISIBLE,
                                 24,64,500,28,h,0,GetModuleHandleW(0),0);
        SendMessageW(t,WM_SETFONT,(WPARAM)f,TRUE);
        g_info = CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE,
                                 24,96,500,24,h,0,GetModuleHandleW(0),0);
        SendMessageW(g_info,WM_SETFONT,(WPARAM)f,TRUE);
        for(int i=0;i<NBTN;i++){
            HWND b = CreateWindowExW(0,L"BUTTON",g_btns[i].label,
                WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
                24, 132+i*56, 300, 44, h, (HMENU)(LONG_PTR)(1001+i),
                GetModuleHandleW(0), 0);
            SendMessageW(b,WM_SETFONT,(WPARAM)f,TRUE);
        }
        Refresh();
        return 0; }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT d = (LPDRAWITEMSTRUCT)l;
        BtnDef& bd = g_btns[d->CtlID-1001];
        HBRUSH br = CreateSolidBrush(bd.color);
        FillRect(d->hDC,&d->rcItem,br);
        DeleteObject(br);
        SetBkMode(d->hDC,TRANSPARENT);
        SetTextColor(d->hDC,RGB(255,255,255));
        DrawTextW(d->hDC,bd.label,-1,&d->rcItem,
                  DT_CENTER|DT_SINGLELINE|DT_VCENTER);
        return TRUE; }
    case WM_COMMAND: {
        int id = (int)LOWORD(w);
        g_counter = (id==1001) ? g_counter+1 : 0;
        Refresh();
        return 0; }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE); }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR,int){
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hi;
    wc.hCursor       = LoadCursorW(0,IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszClassName = L"AppForgeWnd";
    RegisterClassExW(&wc);
    int hgt = 200 + 56*NBTN;
    HWND h = CreateWindowExW(0,wc.lpszClassName,TITLE,WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,560,hgt,0,0,hi,0);
    ShowWindow(h,SW_SHOW);
    MSG msg;
    while(GetMessageW(&msg,0,0,0)){ TranslateMessage(&msg); DispatchMessageW(&msg); }
    return 0;
}
''')
    write(os.path.join(out, "src", "main.cpp"), "".join(lines))
    write(os.path.join(out, "build.bat"),
          '@echo off\r\ncl /nologo /EHsc /MT /DUNICODE /D_UNICODE src\\main.cpp '
          'user32.lib gdi32.lib /Fe:%s.exe /link /SUBSYSTEM:WINDOWS\r\n'
          'if errorlevel 1 exit /b 1\r\necho [OK] %s.exe\r\n' % (app, app))

# ---------------- PC: C# WinForms ----------------
def esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")

def gen_csharp(cfg, out):
    app = cfg["app_name"]
    texts = [tr(cfg, cfg["texts"], i) for i in range(len(cfg["texts"]))]
    cs = ['using System;\nusing System.Drawing;\nusing System.Windows.Forms;\n\n',
          'static class T {\n']
    cs.append('    public const string Title = "%s";\n' % esc(texts[0]))
    cs.append('    public const string Subtitle = "%s";\n' % esc(texts[1] if len(texts) > 1 else ""))
    cs.append('    public const string Presses = "Presses: ";\n}\n\n')
    cs.append('public class MainForm : Form {\n    int counter = 0; readonly Label info;\n')
    cs.append('    public MainForm() {\n')
    cs.append('        Text = T.Title; ClientSize = new Size(560, %d);\n' % (200 + 56 * len(cfg["buttons"])))
    cs.append('        var title = new Label { Text = T.Title, Font = new Font("Segoe UI", 14, FontStyle.Bold), Location = new Point(24, 16), AutoSize = true }; Controls.Add(title);\n')
    cs.append('        var sub = new Label { Text = T.Subtitle, Font = new Font("Segoe UI", 10), Location = new Point(24, 52), AutoSize = true }; Controls.Add(sub);\n')
    cs.append('        info = new Label { Text = "", Font = new Font("Segoe UI", 10), Location = new Point(24, 80), AutoSize = true }; Controls.Add(info);\n')
    for i, b in enumerate(cfg["buttons"]):
        lbl = esc(tr(cfg, cfg["buttons"], i, "label"))
        col = b.get("color", "#2563EB")
        act = "counter++" if i == 0 else "counter = 0"
        cs.append('        var b%d = new Button { Text = "%s", BackColor = ColorTranslator.FromHtml("%s"), ForeColor = Color.White, FlatStyle = FlatStyle.Flat, Location = new Point(24, %d), Size = new Size(300, 44) };\n' % (i, lbl, col, 110 + i * 56))
        cs.append('        b%d.Click += (s, e) => { %s; info.Text = T.Presses + counter; }; Controls.Add(b%d);\n' % (i, act, i))
    cs.append('        info.Text = T.Presses + counter;\n    }\n')
    cs.append('    [STAThread] static void Main() { Application.EnableVisualStyles(); Application.Run(new MainForm()); }\n}\n')
    write(os.path.join(out, "Program.cs"), "".join(cs))
    write(os.path.join(out, "%s.csproj" % app),
          '<Project Sdk="Microsoft.NET.Sdk">\n  <PropertyGroup>\n'
          '    <OutputType>WinExe</OutputType>\n    <TargetFramework>net8.0-windows</TargetFramework>\n'
          '    <UseWindowsForms>true</UseWindowsForms>\n    <Nullable>disable</Nullable>\n'
          '    <ImplicitUsings>disable</ImplicitUsings>\n  </PropertyGroup>\n</Project>\n')
    write(os.path.join(out, "build.bat"),
          '@echo off\r\ndotnet publish -c Release -r win-x64 --self-contained -o dist\r\n'
          'echo [OK] dist\\%s.exe\r\n' % app)

# ---------------- PC: Python Tkinter ----------------
def gen_python(cfg, out):
    app = cfg["app_name"]
    texts = [tr(cfg, cfg["texts"], i) for i in range(len(cfg["texts"]))]
    buttons = [(tr(cfg, cfg["buttons"], i, "label"), b.get("color", "#2563EB"))
               for i, b in enumerate(cfg["buttons"])]
    py = 'import tkinter as tk\nfrom tkinter import font as tkfont\n\n'
    py += 'APP_TITLE = %r\n' % texts[0]
    py += 'SUBTITLE  = %r\n' % (texts[1] if len(texts) > 1 else "")
    py += 'COUNT_FMT = "Presses: "\n'
    py += 'BUTTONS = %r\n\n' % (buttons,)
    py += '''class App:
    def __init__(self, root):
        self.counter = 0
        root.title(APP_TITLE)
        root.geometry("560x%d")
        f_big = tkfont.Font(family="Segoe UI", size=14, weight="bold")
        f_med = tkfont.Font(family="Segoe UI", size=10)
        tk.Label(root, text=APP_TITLE, font=f_big).pack(pady=(16, 2))
        tk.Label(root, text=SUBTITLE, font=f_med).pack()
        self.info = tk.Label(root, text="", font=f_med)
        self.info.pack(pady=6)
        for i, (lbl, col) in enumerate(BUTTONS):
            b = tk.Button(root, text=lbl, bg=col, fg="white", width=30, relief="flat",
                          command=lambda n=i: self.click(n))
            b.pack(pady=5)
        self.refresh()

    def click(self, i):
        self.counter = self.counter + 1 if i == 0 else 0
        self.refresh()

    def refresh(self):
        self.info.config(text=COUNT_FMT + str(self.counter))

root = tk.Tk()
App(root)
root.mainloop()
''' % (200 + 56 * len(cfg["buttons"]),)
    write(os.path.join(out, "app.py"), py)
    write(os.path.join(out, "requirements.txt"), "pyinstaller>=6.0\n")
    write(os.path.join(out, "build.bat"),
          '@echo off\r\npip install pyinstaller -q\r\n'
          'pyinstaller --onefile --windowed --name %s app.py\r\n'
          'echo [OK] dist\\%s.exe\r\n' % (app, app))

# ---------------- Mobile: Android scaffold (Java / Kotlin) ----------------
def _android_scaffold(cfg, out):
    app = cfg["app_name"]
    pkg = cfg.get("package_id", "com.appforge.demo")
    ns = pkg.split(".")
    base = os.path.join(out, "app", "src", "main")
    write(os.path.join(out, "settings.gradle"),
          'pluginManagement { repositories { google(); mavenCentral(); gradlePluginPortal() } }\n'
          'dependencyResolutionManagement { repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS); '
          'repositories { google(); mavenCentral() } }\n'
          'rootProject.name = "%s"\ninclude \':app\'\n' % app)
    kt = cfg["language"] == "kotlin"
    plugins = ("plugins { id 'com.android.application'; "
               "id 'org.jetbrains.kotlin.android' version '1.9.24' }\n") if kt else \
              "plugins { id 'com.android.application' }\n"
    extra = "  kotlinOptions { jvmTarget = '17' }\n" if kt else ""
    write(os.path.join(out, "app/build.gradle"),
          plugins + "android {\n"
          "  namespace '%s'\n  compileSdk 34\n"
          "  defaultConfig { applicationId \"%s\"; minSdk 21; targetSdk 34; versionCode 1; versionName \"1.0\" }\n"
          "  buildTypes { release { minifyEnabled false } }\n"
          "  compileOptions { sourceCompatibility JavaVersion.VERSION_17; targetCompatibility JavaVersion.VERSION_17 }\n%s}\n"
          % (pkg, pkg, extra))
    write(os.path.join(out, "build.gradle"),
          "plugins { id 'com.android.application' version '8.5.2' apply false%s }\n"
          % ("; id 'org.jetbrains.kotlin.android' version '1.9.24' apply false" if kt else ""))
    write(os.path.join(out, "gradle.properties"),
          "org.gradle.jvmargs=-Xmx2048m\nandroid.useAndroidX=true\nandroid.nonTransitiveRClass=true\n")
    write(os.path.join(base, "AndroidManifest.xml"),
          '<?xml version="1.0" encoding="utf-8"?>\n'
          '<manifest xmlns:android="http://schemas.android.com/apk/res/android">\n'
          '  <application android:label="@string/app_name" android:icon="@drawable/icon" '
          'android:theme="@android:style/Theme.Material.Light.NoActionBar">\n'
          '    <activity android:name=".MainActivity" android:exported="true">\n'
          '      <intent-filter><action android:name="android.intent.action.MAIN"/>'
          '<category android:name="android.intent.category.LAUNCHER"/></intent-filter>\n'
          '    </activity>\n  </application>\n</manifest>\n')
    write(os.path.join(base, "res", "values", "strings.xml"),
          '<?xml version="1.0" encoding="utf-8"?>\n<resources>\n  <string name="app_name">%s</string>\n</resources>\n' % app)
    icon = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "app",
                        "src", "main", "res", "drawable", "icon.png")
    dst = os.path.join(base, "res", "drawable", "icon.png")
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if os.path.exists(icon):
        shutil.copy(icon, dst)
    gw = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gradlew")
    if os.path.exists(gw):
        shutil.copy(gw, os.path.join(out, "gradlew"))
        os.chmod(os.path.join(out, "gradlew"), 0o755)
    write(os.path.join(out, "build_apk.bat"),
          "@echo off\r\ncall gradlew assembleDebug\r\n"
          "if exist app\\build\\outputs\\apk\\debug\\app-debug.apk copy "
          "app\\build\\outputs\\apk\\debug\\app-debug.apk %s-debug.apk /y\r\n"
          "echo [OK] %s-debug.apk\r\n" % (app, app))
    return pkg, ns, base

def _sec_lang(cfg):
    langs = cfg.get("languages", ["en"])
    return langs[1] if len(langs) > 1 else "ru"

def gen_java_mobile(cfg, out):
    pkg, ns, base = _android_scaffold(cfg, out)
    sec = _sec_lang(cfg)
    tc = cfg.get("text_color", "#111827").lstrip("#").upper()
    bg = cfg.get("bg_color", "#F3F4F6").lstrip("#").upper()
    j = ['package %s;\n\n' % pkg,
         'import android.app.Activity;\nimport android.graphics.Color;\n',
         'import android.graphics.drawable.GradientDrawable;\nimport android.os.Bundle;\n',
         'import android.util.TypedValue;\nimport android.widget.Button;\n',
         'import android.widget.LinearLayout;\nimport android.widget.ScrollView;\n',
         'import android.widget.TextView;\nimport java.util.Locale;\n\n',
         'public class MainActivity extends Activity {\n',
         '    private int counter = 0;\n    private TextView info;\n    private boolean alt;\n\n',
         '    @Override protected void onCreate(Bundle s) {\n        super.onCreate(s);\n',
         '        alt = Locale.getDefault().getLanguage().equals("%s");\n' % sec,
         '        ScrollView scroll = new ScrollView(this);\n',
         '        LinearLayout root = new LinearLayout(this);\n',
         '        root.setOrientation(LinearLayout.VERTICAL);\n',
         '        root.setPadding(dp(24), dp(24), dp(24), dp(24));\n',
         '        root.setBackgroundColor(0xFF%s);\n' % bg,
         '        TextView title = new TextView(this);\n',
         '        title.setText(alt ? %s : %s);\n' % (jstr(tr(cfg, cfg["texts"], 0, None, sec)), jstr(tr(cfg, cfg["texts"], 0))),
         '        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 22); title.setTextColor(0xFF%s);\n' % tc,
         '        root.addView(title);\n',
         '        TextView sub = new TextView(this);\n',
         '        sub.setText(alt ? %s : %s);\n' % (
             jstr(tr(cfg, cfg["texts"], 1, None, sec) if len(cfg["texts"]) > 1 else ""),
             jstr(tr(cfg, cfg["texts"], 1) if len(cfg["texts"]) > 1 else "")),
         '        sub.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14); sub.setTextColor(0xFF%s);\n' % tc,
         '        sub.setPadding(0, dp(8), 0, dp(8)); root.addView(sub);\n',
         '        info = new TextView(this); info.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);\n',
         '        info.setTextColor(0xFF%s); info.setPadding(0, dp(8), 0, dp(12)); root.addView(info);\n' % tc]
    for i, b in enumerate(cfg["buttons"]):
        col = b.get("color", "#2563EB").lstrip("#").upper()
        act = "counter++" if i == 0 else "counter = 0"
        j.append('        Button b%d = mkButton(alt ? %s : %s, 0xFF%s);\n' % (
            i, jstr(tr(cfg, cfg["buttons"], i, "label", sec)), jstr(tr(cfg, cfg["buttons"], i, "label")), col))
        j.append('        b%d.setOnClickListener(v -> { %s; refresh(); }); root.addView(b%d);\n' % (i, act, i))
    j.append('        scroll.addView(root); setContentView(scroll); refresh();\n    }\n\n')
    j.append('    private Button mkButton(String t, int argb) {\n')
    j.append('        Button b = new Button(this); b.setText(t); b.setTextColor(Color.WHITE); b.setAllCaps(false);\n')
    j.append('        GradientDrawable g = new GradientDrawable(); g.setColor(argb); g.setCornerRadius(dp(12)); b.setBackground(g);\n')
    j.append('        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);\n')
    j.append('        lp.setMargins(0, dp(8), 0, dp(8)); b.setLayoutParams(lp); return b;\n    }\n\n')
    cnt_lbl = tr(cfg, cfg["texts"], 0) and "Нажатий: "
    j.append('    private void refresh() {\n        info.setText((alt ? %s : "Presses: ") + counter);\n    }\n\n' % jstr(cnt_lbl))
    j.append('    private int dp(float v) {\n        return Math.round(TypedValue.applyDimension(\n')
    j.append('            TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics()));\n    }\n}\n')
    write(os.path.join(base, "java", *ns, "MainActivity.java"), "".join(j))

def gen_kotlin_mobile(cfg, out):
    pkg, ns, base = _android_scaffold(cfg, out)
    sec = _sec_lang(cfg)
    tc = cfg.get("text_color", "#111827").lstrip("#").upper()
    bg = cfg.get("bg_color", "#F3F4F6").lstrip("#").upper()
    k = ['package %s\n\n' % pkg,
         'import android.app.Activity\nimport android.graphics.Color\n',
         'import android.graphics.drawable.GradientDrawable\nimport android.os.Bundle\n',
         'import android.util.TypedValue\nimport android.widget.Button\n',
         'import android.widget.LinearLayout\nimport android.widget.ScrollView\n',
         'import android.widget.TextView\nimport java.util.Locale\n\n',
         'class MainActivity : Activity() {\n',
         '    private var counter = 0\n    private lateinit var info: TextView\n',
         '    private var alt = false\n\n',
         '    override fun onCreate(s: Bundle?) {\n        super.onCreate(s)\n',
         '        alt = Locale.getDefault().language == "%s"\n' % sec,
         '        val scroll = ScrollView(this)\n        val root = LinearLayout(this)\n',
         '        root.orientation = LinearLayout.VERTICAL\n',
         '        root.setPadding(dp(24f), dp(24f), dp(24f), dp(24f))\n',
         '        root.setBackgroundColor(0xFF%s.toInt())\n' % bg,
         '        val title = TextView(this)\n',
         '        title.text = if (alt) %s else %s\n' % (jstr(tr(cfg, cfg["texts"], 0, None, sec)), jstr(tr(cfg, cfg["texts"], 0))),
         '        title.textSize = 22f; title.setTextColor(0xFF%s.toInt()); root.addView(title)\n' % tc,
         '        val sub = TextView(this)\n',
         '        sub.text = if (alt) %s else %s\n' % (
             jstr(tr(cfg, cfg["texts"], 1, None, sec) if len(cfg["texts"]) > 1 else ""),
             jstr(tr(cfg, cfg["texts"], 1) if len(cfg["texts"]) > 1 else "")),
         '        sub.textSize = 14f; sub.setTextColor(0xFF%s.toInt()); sub.setPadding(0, dp(8f), 0, dp(8f)); root.addView(sub)\n' % tc,
         '        info = TextView(this); info.textSize = 16f; info.setTextColor(0xFF%s.toInt())\n' % tc,
         '        info.setPadding(0, dp(8f), 0, dp(12f)); root.addView(info)\n']
    for i, b in enumerate(cfg["buttons"]):
        col = b.get("color", "#2563EB").lstrip("#").upper()
        act = "counter++" if i == 0 else "counter = 0"
        k.append('        val b%d = mkButton(if (alt) %s else %s, 0xFF%s.toInt())\n' % (
            i, jstr(tr(cfg, cfg["buttons"], i, "label", sec)), jstr(tr(cfg, cfg["buttons"], i, "label")), col))
        k.append('        b%d.setOnClickListener { %s; refresh() }; root.addView(b%d)\n' % (i, act, i))
    k.append('        scroll.addView(root); setContentView(scroll); refresh()\n    }\n\n')
    k.append('    private fun mkButton(t: String, argb: Int): Button {\n')
    k.append('        val b = Button(this); b.text = t; b.setTextColor(Color.WHITE); b.isAllCaps = false\n')
    k.append('        val g = GradientDrawable(); g.setColor(argb); g.cornerRadius = dp(12f).toFloat(); b.background = g\n')
    k.append('        val lp = LinearLayout.LayoutParams(-1, -2); lp.setMargins(0, dp(8f), 0, dp(8f))\n')
    k.append('        b.layoutParams = lp; return b\n    }\n\n')
    k.append('    private fun refresh() { info.text = (if (alt) %s else "Presses: ") + counter }\n\n' % jstr("Нажатий: "))
    k.append('    private fun dp(v: Float): Int = Math.round(\n')
    k.append('        TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, resources.displayMetrics))\n}\n')
    write(os.path.join(base, "java", *ns, "MainActivity.kt"), "".join(k))

# ---------------- Mobile: iOS SwiftUI ----------------
def gen_swift(cfg, out):
    app = cfg["app_name"]
    sec = _sec_lang(cfg)
    s = ['import SwiftUI\n\n',
         'private func isAlt() -> Bool { Locale.current.language.languageCode?.identifier == "%s" }\n\n' % sec,
         'struct ContentView: View {\n    @State private var counter = 0\n\n',
         '    var body: some View {\n        VStack(spacing: 16) {\n']
    s.append('            Text(isAlt() ? %s : %s).font(.title).bold()\n' % (
        jstr(tr(cfg, cfg["texts"], 0, None, sec)), jstr(tr(cfg, cfg["texts"], 0))))
    if len(cfg["texts"]) > 1:
        s.append('            Text(isAlt() ? %s : %s)\n' % (
            jstr(tr(cfg, cfg["texts"], 1, None, sec)), jstr(tr(cfg, cfg["texts"], 1))))
    s.append('            Text("Presses: \\(counter)")\n')
    for i, b in enumerate(cfg["buttons"]):
        col = b.get("color", "#2563EB").lstrip("#")
        act = "counter += 1" if i == 0 else "counter = 0"
        s.append('            Button(action: { %s }) { Text(isAlt() ? %s : %s)\n' % (
            act, jstr(tr(cfg, cfg["buttons"], i, "label", sec)), jstr(tr(cfg, cfg["buttons"], i, "label"))))
        s.append('                .frame(maxWidth: .infinity).padding()\n')
        s.append('                .background(Color(hex: "%s")).foregroundColor(.white).cornerRadius(12) }\n' % col)
    s.append('        }.padding()\n    }\n}\n\n')
    s.append('extension Color { init(hex: String) {\n')
    s.append('    let v = UInt(hex, radix: 16) ?? 0\n')
    s.append('    self.init(.sRGB, red: Double((v>>16)&0xFF)/255, green: Double((v>>8)&0xFF)/255, blue: Double(v&0xFF)/255)\n} }\n\n')
    ident = "".join(w.capitalize() for w in app.replace("-", " ").split() if w.isalnum()) or "App"
    s.append('@main struct %sApp: App { var body: some Scene { WindowGroup { ContentView() } } }\n' % ident)
    write(os.path.join(out, "Sources", "ContentView.swift"), "".join(s))
    write(os.path.join(out, "README_BUILD_IOS.txt"),
          "Открой Xcode -> New Project (iOS App, SwiftUI) -> замени ContentView.swift этим файлом.\n"
          "Сборка ipa: Product > Archive.\n")

GENERATORS = {"cpp": gen_cpp, "csharp": gen_csharp, "python": gen_python,
              "java": gen_java_mobile, "kotlin": gen_kotlin_mobile, "swift": gen_swift}

PC_LANGS = ("cpp", "csharp", "python")
MOBILE_LANGS = ("java", "kotlin", "swift")

def generate(cfg, out):
    if os.path.exists(out) and os.listdir(out):
        raise RuntimeError("output dir not empty: " + out)
    GENERATORS[cfg["language"]](cfg, out)
    write(os.path.join(out, "project.json"), json.dumps(cfg, ensure_ascii=False, indent=2))

def build_exe(cfg, out, log):
    lang = cfg["language"]
    if lang == "cpp":
        r = subprocess.run(["cmd", "/c", "build.bat"], cwd=out, capture_output=True, text=True)
    elif lang == "csharp":
        r = subprocess.run(["dotnet", "publish", "-c", "Release", "-r", "win-x64",
                            "--self-contained", "-o", "dist"], cwd=out, capture_output=True, text=True)
    elif lang == "python":
        r = subprocess.run(["pyinstaller", "--onefile", "--windowed",
                            "--name", cfg["app_name"], "app.py"], cwd=out, capture_output=True, text=True)
    else:
        log.append("build_exe: язык %s не PC" % lang)
        return False
    log.append((r.stdout or "") + (r.stderr or ""))
    exe = os.path.join(out, cfg["app_name"] + ".exe") if lang == "cpp" \
        else os.path.join(out, "dist", cfg["app_name"] + ".exe")
    return os.path.exists(exe)

def build_apk(cfg, out, log):
    gradlew = os.path.join(out, "gradlew.bat" if os.name == "nt" else "./gradlew")
    cmd = [gradlew, "assembleDebug"] if os.path.exists(gradlew) else ["gradle", "assembleDebug"]
    try:
        r = subprocess.run(cmd, cwd=out, capture_output=True, text=True, shell=(os.name == "nt"))
        log.append((r.stdout or "") + (r.stderr or ""))
    except FileNotFoundError:
        log.append("gradle не найден в PATH. Установи Android Studio или SDK cmdline-tools.")
        return False
    apk = os.path.join(out, "app", "build", "outputs", "apk", "debug", "app-debug.apk")
    if os.path.exists(apk):
        shutil.copy(apk, os.path.join(out, cfg["app_name"] + "-debug.apk"))
        return True
    return False

if __name__ == "__main__":
    cfg = json.load(open(sys.argv[1], encoding="utf-8"))
    generate(cfg, sys.argv[2])
    print("[OK] generated:", cfg["language"], "->", sys.argv[2])
