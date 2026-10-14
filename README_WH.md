# CS2 WH — полный комплект (клиент + DLL-экспортер + инжектор)

## Состав
| Файл | Что это |
|---|---|
| `main_console.cpp` `console.hpp` `esp_core.hpp` `kdf_read.hpp` `offsets.hpp` `config.hpp` | внешний консольный WH (`cs2_wh.exe`) |
| `exporter/dllmain.cpp` `exporter/sdk.hpp` `kdf_write.hpp` | DLL-экспортер (`vdc_exporter.dll`) — пишет данные игры в KDF-фабрику |
| `exporter/injector.c` | инжектор (`injector.exe`) — CreateRemoteThread+LoadLibraryA |
| `build_console.bat` `exporter/build_dll.bat` | сборки для MSVC (cl) |

## Сборка (Windows, x64 Native Tools Command Prompt for VS 2022)
```
cd C:\src\cs2_esp
build_console.bat                 -> cs2_wh.exe
cd exporter
build_dll.bat                     -> vdc_exporter.dll
gcc -O2 injector.c -o injector.exe    (или cl injector.c /Fe:injector.exe)
```
MinGW-альтернатива (тоже проверено — всё компилируется без ошибок):
```
x86_64-w64-mingw32-g++ -std=c++20 -O2 -municode main_console.cpp -o cs2_wh.exe -luser32 -lgdi32 -static
x86_64-w64-mingw32-g++ -std=c++20 -O2 -shared exporter\dllmain.cpp -o vdc_exporter.dll -static -luser32
x86_64-w64-mingw32-gcc -O2 exporter\injector.c -o injector.exe -static
```

## Запуск «всё работает»
1. Запусти CS2 (режим окна не важен — оверлея нет).
2. Дождись загрузки в меню/матч.
3. Узнай pid: `tasklist | findstr cs2` (или через диспетчер задач → Подробности).
4. `injector <pid> vdc_exporter.dll` — вывод `module handle != 0` = успех.
5. Запусти `cs2_wh.exe` — он сам найдёт cs2.exe, подключится к фабрике и после начала матча выведет список врагов: `[V]/[X]`, ник, hp, дистанция, world-координаты, экранные координаты, оружие, [C4]/[HELM]/[FLASH].
6. ESC в консоли — выход WH; экспортная DLL живёт до выхода игры.

## Что править после патчей CS2
- `exporter/sdk.hpp`: сигнатуры `PAT_ENTITYLIST/PAT_LOCALPAWN/PAT_VIEWMATRIX` и константы `off::*` (берутся из твоего дампера/ReClass по build).
- Если сигнатура view matrix не нашлась — WH продолжит показывать мировые позиции/дистанции, но без экранных координат.
- Layout снапшота зафиксирован static_assert'ами в `offsets.hpp` (EntityData=0x1A8); при рассинхоне сборка падает, а не тихо врёт.

## Ограничения (честно)
- Хитбоксы экспортируются скелет-аппроксимом (константные Z-сдвиги от feet), не студийной моделью — бокс/скелет выглядят ровно, но без анимации рук.
- `Spotted/Shootable` ставятся в 1 (движок-трейс из потока вне render-стейта не делаем) — фильтр «видимость» на стороне WH по этим полям будет всегда true.
- Инъекция через LoadLibrary видна VAC по списку модулей; для ранetim-матчей нужен manual map — это остаётся на твоём инструменте.
- Компиляция проверена mingw-x64; MSVC-cl путь стандартный, но локально не запускался.
