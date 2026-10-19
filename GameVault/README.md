# GameVault — сборка и запуск

## Требования (Windows 10/11 x64)
| Компонент | Зачем | Скачать |
|---|---|---|
| **.NET 8 SDK** | компилятор + WPF | https://dotnet.microsoft.com/download/dotnet/8.0 |
| Visual Studio 2022 (17.8+) с workload *.NET desktop development* | IDE, опционально | installer VS |
| **WebView2 Runtime** | встроенный браузер для HTML-игр | обычно уже есть в Win11; иначе https://developer.microsoft.com/microsoft-edge/webview2/ |

Проверка SDK в «Терминал» внутри VS или cmd:
```
dotnet --version        # должно быть 8.x
```

## Вариант A — сборка одной командой (рекомендую)
1. Скопируйте папку `GameVault` (например, в `C:\Dev\GameVault`).
2. `Win+R` → `cmd`, затем:
```bat
cd /d C:\Dev\GameVault
dotnet restore
dotnet build -c Release
dotnet run  -c Release          # запуск из консоли (видны ошибки)
```
3. Готовый exe: `bin\Release\net8.0-windows\GameVault.exe`.

Развернутая версия без .NET Runtime на других машинах:
```bat
dotnet publish -c Release -r win-x64 --self-contained true /p:PublishSingleFile=false
:: результат в bin\Release\net8.0-windows\win-x64\publish\
```

## Вариант B — через Visual Studio
1. Файл → Открыть → Проект/Решение → выбрать **`GameVault.csproj`**.
2. При первом открытии VS сама восстановит NuGet-пакеты (MonoTorrent, WebView2, Newtonsoft.Json, Microsoft.Win32.Registry). Статус: меню *Проект → Восстановить NuGet-пакеты*.
3. Конфигурация: **Release**, платформа **Any CPU** (или x64), F5 — запуск с отладчиком.
4. Если ругается на TargetFramework — см. «Смена версии .NET» ниже.

## Частые проблемы
| Симптом | Причина / решение |
|---|---|
| `error NETSDK1045: The current .NET SDK does not support targeting .NET 8.0` | установлен только .NET 6/7 SDK → поставьте .NET 8 SDK **или** замените в csproj `net8.0-windows` → `net6.0-windows` |
| XDG/ошибки WebView2 при старте HTML-игры | доустановите WebView2 Runtime (Evergreen) |
| `NU1101: Unable to find package MonoTorrent` | корпоративный прокси/NuGet.config без nuget.org → `dotnet nuget add source https://api.nuget.org/v3/index.json -n nugetorg` |
| Обложки не грузятся | нет доступа к `cdn.cloudflare.steamstatic.com` (проверьте браузером); кэш лежит в `%LOCALAPPDATA%\GameVault\covers\` |
| Поиск торрентов пуст | в Настройках добавьте Torznab-эндпоинт Jackett/Prowlarr, формат: `http://127.0.0.1:9117/api/v2.0/indexers/xxx/results/torznab/api?apikey=KEY&t=search&cat=5000&q={query}` |
| Steam/Epic игра не запускается | лаунчер платформы должен быть установлен и залогинен (используются протоколы `steam://` / `com.epicgames.launcher://`) |

## Где что живёт во время работы
```
%LOCALAPPDATA%\GameVault\config.json      — настройки (папки, лимиты скорости, индексаторы)
%LOCALAPPDATA%\GameVault\library.json     — библиотека игр
%LOCALAPPDATA%\GameVault\covers\          — кэш обложек
%USERPROFILE%\Downloads\GameVault\        — торрент-загрузки (папка меняется в Настройках)
```

## Как пользоваться
1. **Обновить библиотеку** — автоскан Steam/Epic/GOG/Amazon (реестр + manifest'ы), обложки подтягиваются сами.
2. **+ Игра** — любая локальная `.exe`; **+ HTML-игра** — указали папку с `index.html` → стартует во встроенном WebView2 (F11 — фуллскрин).
3. **Поиск (торрент)** — введите название игры → список раздач (сортировка по сидам) → «Скачать» → прогресс во вкладке «Загрузки» → после завершения кнопка «В библиотеку» (лаунчер найдёт exe внутри распакованной папки).
