# CS2 ESP Overlay (VDC/KDF external read)

Внешний wallhack-оверлей для Counter-Strike 2: чтение состояния игры через
**Valve Data Collection / Keyed Data Factory** (`NtQuerySystemInformation` class `0x2D`,
magic `"KDMFD"`) + полноэкранный click-through оверлей на ImGui/D3D11.

## Архитектура

```
[cs2.exe + VDC exporter] --shared section--> [kdf::Factory] --> [Esp снапшот] --> [ImGui drawlist]
```

Никаких хуков в процесс игры, никаких `ReadProcessMemory` — только механизм обмена
данными Valve. Требуется **экспортер** (сторона, пишущая данные в KDF-фабрику по
схеме `offsets.hpp`).

## Файлы

| файл          | роль |
|---------------|------|
| `kdf_read.hpp`| KDF-клиент: open/query/close фабрики, MapViewOfFile секции |
| `offsets.hpp` | схема shared-буфера экспортера (local, entities[], viewmatrix) |
| `config.hpp`  | имя фабрики, хитбокс-скелет, дефолты ESP, интервалы |
| `esp.hpp`     | класс Esp: attach/update/W2S/box |
| `esp.cpp`     | отрисовка: боксы, скелет, head-circle, инфо, радар, snaplines |
| `overlay.hpp` | окно WS_EX_LAYERED+TRANSPARENT+TOPMOST + D3D11 + ImGui backend |
| `main.cpp`    | цикл: ждём cs2.exe → attach → рендер; конфиг-меню по INSERT |
| `build.bat`   | сборка cl (MSVC x64) |

## Получение исходников

Исходники лежат в этом репозитории (/workspace). Забери их любым способом:
- `git clone <url-репозитория>` — если пушнул куда-то;
- или скопируй файлы целиком: kdf_read.hpp, offsets.hpp, config.hpp, esp.hpp,
  esp.cpp, overlay.hpp, main.cpp, build.bat (+ README). В отдельную папку на Windows-машине.

Сборка возможна ТОЛЬКО на Windows x64 (нужен MSVC). На Linux/macOS не собирается.

## Сборка (Windows)

1. Установи Visual Studio 2022 (Community бесплатно) → workload "Desktop development with C++".
   Галочка "CMake" не нужна, нужен компилятор MSVC + Windows SDK.
   Альтернатива без VS: `winget install LLVM.LLVM` и сборка clang-cl с теми же флагами.
2. Открой "x64 Native Tools Command Prompt for VS 2022".
3. Перейди в папку с исходниками: `cd C:\src\cs2_esp`
4. Подтяни ImGui (один раз):
```bat
git clone https://github.com/ocornut/imgui && cd imgui && git checkout v1.90.1 && cd ..
```
   (без git: скачай zip https://github.com/ocornut/imgui/archive/refs/tags/v1.90.1.zip
    и переименуй распакованную папку в `imgui`)
5. Собери:
```bat
build.bat
```
Успех = `[OK] cs2_esp.exe` в папке. Ошибки типа `imgui.h: No such file` — папка imgui не рядом.

## Запуск

1. CS2 в windowed borderless (Fullscreen Windowed) — настройки видео в игре.
   Fullscreen exclusive перекрывает оверлей, работать не будет.
2. Стартуй свой VDC-экспортер — он должен писать данные в KDF-фабрику `CS2-VDC`
   по схеме из `offsets.hpp` (см. config.hpp: `KDF_FACTORY_NAME`).
   Без экспортера ESP не появится: main.cpp крутится на этапе "waiting for data".
3. `cs2_esp.exe` от администратора НЕ требуется (KDF-клиент работает из user-mode).
   ESP появится после загрузки карты.
Управление:
- `INSERT` — конфиг-меню ImGui (вкл/выкл box/skeleton/headcircle/info/radar/snaplines, цвета, distance)
- `ESC` — выход.
Меню кликабельно только когда включено; click-through окно не перехватывает мышь в игре.

## Частые проблемы

| симптом | причина / решение |
|---|---|
| `cl : command not found` | запуск не из x64 Native Tools Command Prompt |
| `imgui.h: No such file` | папка `imgui` не рядом с исходниками |
| окно есть, ESP нет | экспортер не запущен / другое имя фабрики / не та схема offsets |
| оверлей под окном игры | TOPMOST слетел — Alt+Tab; либо fullscreen exclusive → borderless |
| боксы "плывут" при движении | DPI scaling — проверь Per-Monitor V2 (уже в main.cpp), не меняй масштаб Windows |
| разьезд W2S после патча | обновить layout в offsets.hpp своим дампером |

## Настройка схемы

`offsets.hpp` подогнан под типичный public VDC-dumper (build 14285):
запись entity = `0xF0`, локальный = `0x128`, viewmatrix row-major в `+0x1540`.
Если твой экспортер использует другой layout — правь только этот файл,
`static_assert` поймает рассинхрон размера записи.

## Нюансы реализации

- **W2S**: `clip = v * M` (row-major), отсечка `w < 0.001` — точки за камерой не рисуем.
- **Снапшот consistency**: копирование буфера только при изменении `seqnum` (UINT64 в +0x00),
  иначе tearing между кадрами экспортера.
- **Прозрачность**: `LWA_COLORKEY(RGB(0,0,0))` + clear alpha=0. Нужен per-pixel alpha —
  переходи на `UpdateLayeredWindow`.
- **Units→meters**: `* 0.0254`.
- **DPI**: `PER_MONITOR_AWARE_V2` обязателен, иначе W2S разъедется с реальным разрешением.
- **Fullscreen exclusive не работает** — внешний оверлей поверх exclusive-фронта возможен
  только через DXGI desktop duplication; используй borderless.

## Детект-поверхность (CS2/VAC Live)

- Сторонний код, читающий память cs2.exe (в т.ч. VDC-экспортер внутри процесса),
  детектируется VAC Live по integrity-проверкам модулей и поведению loader'а.
- KDF-клиент сам по себе — обычный NT-call из внешнего процесса; основной риск
  несёт экспортерная сторона.
- Окно оверлея видно по классу/title через user32 hooks: меняй `OVERLAY_TITLE`
  и имя класса окна, не оставляй дефолтные.
