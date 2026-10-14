# AppForge — конструктор приложений (PC + телефон)

GUI-программа, в которой ты собираешь **своё приложение**: выбираешь тексты на
нескольких языках, кнопки и их цвета (тему/«текстуры» интерфейса), имя приложения —
и получаешь готовый проект с кнопками сборки **.exe** (Windows) или **.apk** (Android).

## Запуск конструктора

```bash
cd pc
python software_designer.py
```

Нужен только Python 3.8+ с Tkinter (входит в стандартный установщик Python на Windows).

## Что можно выбрать в конструкторе

| Вкладка | Настройки |
|---|---|
| Основное | имя приложения, package id (Android), языки интерфейса (EN/RU/UK/DE/FR/ES/TR/ZH), фон окна (hex или пипетка), язык кодогенерации для PC и Mobile |
| Тексты | любое количество мультиязычных текстов (заголовок, подзаголовок…) |
| Кнопки | любое количество кнопок, подпись на каждом языке, цвет (клик по плашке → выбор цвета) |
| Сборка / лог | «Собрать EXE», «Собрать APK», открытие папки проекта, лог компиляции |

## Языки программирования

**PC (.exe):**
- `cpp` — чистый WinAPI (Microsoft C++ Build Tools: `cl`)
- `csharp` — WinForms (.NET SDK: `dotnet publish`)
- `python` — Tkinter (`pyinstaller --onefile`)

**Мобильные:**
- `java` — Android (Gradle + Android SDK)
- `kotlin` — Android (Gradle + Android SDK + Kotlin plugin)
- `swift` — iOS/SwiftUI (проект открывается в Xcode; сборка .ipa через Product→Archive)

## Как это работает

1. «Сгенерировать проект» → `tools/generator.py` создаёт полноценную папку проекта
   (исходники, build.gradle/AndroidManifest для Android, build.bat для exe, project.json).
2. «Собрать EXE» запускает нужный тулчейн (`cl` / `dotnet` / `pyinstaller`).
3. «Собрать APK» запускает `gradlew assembleDebug` (нужны JDK 17 + Android SDK;
   проще всего поставить Android Studio — она всё это содержит).
4. Готовые файлы: `<Имя>.exe` (или `dist/<Имя>.exe`) и `<Имя>-debug.apk`.

## Требования для сборки

| Цель | Что должно быть установлено |
|---|---|
| EXE (cpp) | Visual Studio Build Tools (cl.exe) |
| EXE (csharp) | .NET SDK 8 |
| EXE (python) | `pip install pyinstaller` |
| APK | JDK 17 + Android SDK (Android Studio) + интернет для Gradle |
| IPA (swift) | macOS + Xcode |

## Структура репозитория

```
pc/software_designer.py   — GUI конструктор
tools/generator.py        — генератор проектов + функции сборки exe/apk (CLI тоже умеет)
app/, build.gradle, ...   — демо-шаблон Android-проекта (иконка/gradlew берутся отсюда)
gradlew                   — обёртка Gradle
```

CLI-генерация без GUI:

```bash
python tools/generator.py my_project.json ./MyProject
```
