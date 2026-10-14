#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AppForge Designer — GUI-конструктор приложений (PC + Android).
Tkinter, stdlib-only.  Запуск:  python software_designer.py
Сохраняет project.json -> tools/generator.py генерирует проект -> build.bat / gradle собирают .exe/.apk
"""
import json, os, subprocess, sys, threading

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
import generator as G  # noqa: E402

try:
    import tkinter as tk
    from tkinter import ttk, colorchooser, filedialog, messagebox
except ImportError:
    print("Tkinter недоступен. Установи Python с tcl/tk.")
    sys.exit(1)

LANG_NAMES = {
    "en": "English", "ru": "Русский", "uk": "Українська", "de": "Deutsch",
    "fr": "Français", "es": "Español", "tr": "Türkçe", "zh": "中文",
}
PRESET_COLORS = ["#2563EB", "#DC2626", "#16A34A", "#9333EA",
                 "#F59E0B", "#0891B2", "#111827", "#F3F4F6"]


class TextRow:
    """редактируемая строка мультиязычного текста"""
    def __init__(self, parent, data, langs, on_delete=None):
        self.frame = ttk.Frame(parent)
        self.vars = {}
        for code in langs:
            ttk.Label(self.frame, text=code).pack(side=tk.LEFT, padx=(6, 0))
            v = tk.StringVar(value=data.get(code, ""))
            self.vars[code] = v
            ttk.Entry(self.frame, textvariable=v, width=22).pack(side=tk.LEFT, padx=(2, 0))
        if on_delete:
            ttk.Button(self.frame, text="x", width=2, command=on_delete).pack(side=tk.RIGHT, padx=4)

    def get(self):
        return {c: v.get() for c, v in self.vars.items()}


class BtnRow:
    def __init__(self, parent, idx, data, langs, on_delete):
        self.frame = ttk.Frame(parent)
        self.label_vars = {}
        ttk.Label(self.frame, text=f"Кнопка {idx+1}:").pack(side=tk.LEFT)
        for code in langs:
            v = tk.StringVar(value=data["label"].get(code, ""))
            self.label_vars[code] = v
            ttk.Entry(self.frame, textvariable=v, width=14).pack(side=tk.LEFT, padx=2)
        self.color = tk.StringVar(value=data.get("color", PRESET_COLORS[0]))
        self.swatch = tk.Canvas(self.frame, width=24, height=24, highlightthickness=1)
        self.swatch.pack(side=tk.LEFT, padx=4)
        self.swatch.configure(bg=self.color.get())
        self.swatch.bind("<Button-1>", self.pick)
        ttk.Button(self.frame, text="x", width=2, command=on_delete).pack(side=tk.RIGHT, padx=4)

    def pick(self, _):
        c = colorchooser.askcolor(color=self.color.get())[1]
        if c:
            self.color.set(c)
            self.swatch.configure(bg=c)

    def get(self):
        return {"label": {c: v.get() for c, v in self.label_vars.items()},
                "color": self.color.get()}


def re_ident(s):
    s = "".join(ch if ch.isalnum() else "" for ch in s.strip())
    return s or "MyApp"


class Designer(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("AppForge Designer — конструктор приложений")
        self.geometry("980x720")
        self.langs = ["en", "ru"]
        self.texts = [{"en": "My App", "ru": "Мое приложение"},
                      {"en": "Hello! Tap buttons.", "ru": "Привет! Нажимай кнопки."}]
        self.buttons = [{"label": {"en": "Press me", "ru": "Нажми меня"}, "color": PRESET_COLORS[0]},
                        {"label": {"en": "Reset", "ru": "Сброс"}, "color": PRESET_COLORS[1]}]
        self.bg_color = tk.StringVar(value="#F3F4F6")
        self.app_name = tk.StringVar(value="MyApp")
        self.pkg_id = tk.StringVar(value="com.mycompany.myapp")
        self.out_dir = tk.StringVar(value=os.path.expanduser("~"))
        self.proj_dir = None
        self.rows_texts, self.rows_btns = [], []
        self._build_ui()

    # ---------- UI ----------
    def _build_ui(self):
        nb = ttk.Notebook(self)
        nb.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)
        tab_gen = ttk.Frame(nb); nb.add(tab_gen, text=" Основное ")
        tab_txt = ttk.Frame(nb); nb.add(tab_txt, text=" Тексты (мультиязычно) ")
        tab_btn = ttk.Frame(nb); nb.add(tab_btn, text=" Кнопки и цвета ")
        tab_log = ttk.Frame(nb); nb.add(tab_log, text=" Сборка / лог ")

        g = ttk.LabelFrame(tab_gen, text="Проект")
        g.pack(fill=tk.X, padx=8, pady=8)
        self._row(g, "Имя приложения (.exe/.apk):", self.app_name)
        self._row(g, "Package id (Android):", self.pkg_id)
        r = ttk.Frame(g); r.pack(fill=tk.X, padx=6, pady=3)
        ttk.Label(r, text="Языки интерфейса:").pack(side=tk.LEFT)
        self.lang_vars = {}
        for code, name in LANG_NAMES.items():
            v = tk.BooleanVar(value=code in self.langs)
            self.lang_vars[code] = v
            ttk.Checkbutton(r, text=name, variable=v,
                            command=self._langs_changed).pack(side=tk.LEFT, padx=4)

        p = ttk.LabelFrame(tab_gen, text="Тема")
        p.pack(fill=tk.X, padx=8, pady=8)
        row = ttk.Frame(p); row.pack(fill=tk.X, padx=6, pady=3)
        ttk.Label(row, text="Фон окна (hex):").pack(side=tk.LEFT)
        e = ttk.Entry(row, textvariable=self.bg_color, width=10)
        e.pack(side=tk.LEFT, padx=4)
        e.bind("<KeyRelease>", lambda ev: self._set_bg(e.get()))
        self.bg_swatch = tk.Canvas(p, width=80, height=24, highlightthickness=1)
        self.bg_swatch.pack(anchor=tk.W, padx=6, pady=2)
        self.bg_swatch.configure(bg=self.bg_color.get())
        self.bg_swatch.bind("<Button-1>", lambda ev: self._pick_bg(e))

        act = ttk.Frame(tab_gen); act.pack(fill=tk.X, padx=8, pady=8)
        ttk.Button(act, text="Сохранить project.json", command=self._save_json).pack(side=tk.LEFT, padx=4)
        ttk.Button(act, text="Сгенерировать проект", command=self._generate).pack(side=tk.LEFT, padx=4)
        self.lang_combo = ttk.Combobox(act, values=list(G.PC_LANGS), width=8, state="readonly")
        self.lang_combo.set("cpp"); self.lang_combo.pack(side=tk.LEFT, padx=(16, 2))
        ttk.Label(act, text="язык PC").pack(side=tk.LEFT)
        self.mlang_combo = ttk.Combobox(act, values=list(G.MOBILE_LANGS), width=8, state="readonly")
        self.mlang_combo.set("java"); self.mlang_combo.pack(side=tk.LEFT, padx=(16, 2))
        ttk.Label(act, text="язык Mobile").pack(side=tk.LEFT)

        # texts tab
        t = ttk.LabelFrame(tab_txt, text="Тексты (заголовок, подзаголовок, ...)")
        t.pack(fill=tk.BOTH, expand=True, padx=8, pady=8)
        self.text_box = ttk.Frame(t); self.text_box.pack(fill=tk.X, padx=4, pady=4)
        ttk.Button(t, text="+ добавить текст", command=self._add_text).pack(anchor=tk.W, padx=8, pady=4)
        self._rebuild_texts()

        # buttons tab
        b = ttk.LabelFrame(tab_btn, text="Кнопки (цвет — клик по плашке)")
        b.pack(fill=tk.BOTH, expand=True, padx=8, pady=8)
        self.btn_box = ttk.Frame(b); self.btn_box.pack(fill=tk.X, padx=4, pady=4)
        ttk.Button(b, text="+ добавить кнопку", command=self._add_btn).pack(anchor=tk.W, padx=8, pady=4)
        self._rebuild_btns()

        # build tab
        logf = ttk.LabelFrame(tab_log, text="Действия")
        logf.pack(fill=tk.X, padx=8, pady=8)
        row1 = ttk.Frame(logf); row1.pack(fill=tk.X, padx=6, pady=4)
        ttk.Button(row1, text="Собрать EXE (Windows)", command=lambda: self._build("exe")).pack(side=tk.LEFT, padx=4)
        ttk.Button(row1, text="Собрать APK (Android)", command=lambda: self._build("apk")).pack(side=tk.LEFT, padx=4)
        ttk.Button(row1, text="Открыть папку проекта", command=self._open_out).pack(side=tk.LEFT, padx=4)

        self.log = tk.Text(tab_log, height=22, state=tk.DISABLED)
        self.log.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)

    def _row(self, parent, label, var):
        r = ttk.Frame(parent); r.pack(fill=tk.X, padx=6, pady=3)
        ttk.Label(r, text=label, width=26).pack(side=tk.LEFT)
        ttk.Entry(r, textvariable=var, width=48).pack(side=tk.LEFT, fill=tk.X, expand=True)

    def _set_bg(self, c):
        try:
            bytes.fromhex(c.lstrip("#"))
            self.bg_color.set(c)
            self.bg_swatch.configure(bg=c)
        except ValueError:
            pass

    def _pick_bg(self, entry):
        col = colorchooser.askcolor(color=self.bg_color.get())[1]
        if col:
            entry.delete(0, tk.END); entry.insert(0, col)
            self._set_bg(col)

    def _langs_changed(self):
        self.langs = [c for c, v in self.lang_vars.items() if v.get()] or ["en"]
        self._rebuild_texts(); self._rebuild_btns()

    # ---------- dynamic rows ----------
    def _rebuild_texts(self):
        for w in self.text_box.winfo_children():
            w.destroy()
        self.rows_texts = []
        for i, d in enumerate(self.texts):
            row = TextRow(self.text_box, d, self.langs,
                          on_delete=lambda n=i: self._del_text(n))
            row.frame.pack(fill=tk.X, pady=2)
            self.rows_texts.append(row)

    def _add_text(self):
        self.texts = [r.get() for r in self.rows_texts] + [{c: "" for c in self.langs}]
        self._rebuild_texts()

    def _del_text(self, i):
        cur = [r.get() for r in self.rows_texts]
        del cur[i]
        self.texts = cur or [{c: "" for c in self.langs}]
        self._rebuild_texts()

    def _rebuild_btns(self):
        for w in self.btn_box.winfo_children():
            w.destroy()
        self.rows_btns = []
        for i, d in enumerate(self.buttons):
            row = BtnRow(self.btn_box, i, d, self.langs,
                         on_delete=lambda n=i: self._del_btn(n))
            row.frame.pack(fill=tk.X, pady=2)
            self.rows_btns.append(row)

    def _add_btn(self):
        self.buttons = [r.get() for r in self.rows_btns] + \
            [{"label": {c: "" for c in self.langs},
              "color": PRESET_COLORS[len(self.buttons) % len(PRESET_COLORS)]}]
        self._rebuild_btns()

    def _del_btn(self, i):
        cur = [r.get() for r in self.rows_btns]
        del cur[i]
        self.buttons = cur or [{"label": {c: "" for c in self.langs}, "color": PRESET_COLORS[0]}]
        self._rebuild_btns()

    # ---------- config ----------
    def collect_cfg(self, language):
        return {
            "app_name": re_ident(self.app_name.get()),
            "package_id": self.pkg_id.get().strip() or "com.mycompany.myapp",
            "language": language,
            "ui_lang": "en",
            "languages": self.langs,
            "bg_color": self.bg_color.get(),
            "text_color": "#111827",
            "texts": [r.get() for r in self.rows_texts],
            "buttons": [r.get() for r in self.rows_btns],
        }

    def _save_json(self):
        path = filedialog.asksaveasfilename(defaultextension=".json",
                                            filetypes=[("project.json", "*.json")])
        if path:
            with open(path, "w", encoding="utf-8") as f:
                json.dump(self.collect_cfg(self.lang_combo.get()), f,
                          ensure_ascii=False, indent=2)
            self._log("сохранено: " + path)

    def _generate(self):
        base = filedialog.askdirectory(initialdir=self.out_dir.get(),
                                       title="Папка для нового проекта")
        if not base:
            return
        cfg = self.collect_cfg(self.lang_combo.get())
        out = os.path.join(base, cfg["app_name"] + "_" + cfg["language"])
        try:
            G.generate(cfg, out)
        except RuntimeError as e:
            messagebox.showerror("Ошибка", str(e)); return
        cfgm = self.collect_cfg(self.mlang_combo.get())
        outm = os.path.join(base, cfgm["app_name"] + "_" + cfgm["language"])
        try:
            G.generate(cfgm, outm)
            self._log("сгенерировано Mobile: " + outm)
        except RuntimeError as e:
            self._log("mobile не сгенерирован: " + str(e))
        self.proj_dir = out
        self.proj_dir_mob = outm
        self._log("сгенерировано PC: " + out)
        self._log("EXE: build.bat в PC-папке (нужен VS Build Tools / dotnet SDK / pyinstaller)")
        self._log("APK: build_apk.bat в mobile-папке (нужен JDK 17 + Android SDK)")

    def _build(self, kind):
        if not getattr(self, "proj_dir", None) or not os.path.isdir(self.proj_dir):
            messagebox.showwarning("Нет проекта", "Сначала «Сгенерировать проект».")
            return
        if kind == "exe":
            cfg = json.load(open(os.path.join(self.proj_dir, "project.json"), encoding="utf-8"))
            target = self.proj_dir
        else:
            target = getattr(self, "proj_dir_mob", None)
            if not target or not os.path.isdir(target):
                messagebox.showwarning("Нет mobile-проекта", "Сначала «Сгенерировать проект».")
                return
            cfg = json.load(open(os.path.join(target, "project.json"), encoding="utf-8"))
        def worker():
            log = []
            ok = G.build_exe(cfg, target, log) if kind == "exe" else G.build_apk(cfg, target, log)
            tail = "\n".join("\n".join(log).splitlines()[-40:])
            self.after(0, lambda: (self._log(tail),
                                   messagebox.showinfo(
                                       "Сборка",
                                       ("готово: " + target) if ok
                                       else "не удалось — см. лог (не хватает тулчейна?)")))
        threading.Thread(target=worker, daemon=True).start()
        self._log("сборка запущена...")

    def _open_out(self):
        d = self.proj_dir or self.out_dir.get()
        if os.name == "nt":
            os.startfile(d)
        else:
            subprocess.Popen(["xdg-open", d])

    def _log(self, msg):
        self.log.configure(state=tk.NORMAL)
        self.log.insert(tk.END, msg.rstrip() + "\n\n")
        self.log.see(tk.END)
        self.log.configure(state=tk.DISABLED)


if __name__ == "__main__":
    Designer().mainloop()
