# CS2 WH — консольная версия (без оверлея)

Один exe, ноль зависимостей: запускаешь — в консоли летят координаты врагов.
Графический оверлей (cs2_esp.exe) — отдельная сборка, см. README_ESP.md.

## Что нужно на Windows-машине
1. Visual Studio 2022 Community → workload "Desktop development with C++"
   (даёт cl.exe + Windows SDK). Альтернатива: winget install LLVM.LLVM (clang-cl).
2. Исходники этого репа в одну папку, например C:\src\cs2_wh.
   ImGui для консольной версии НЕ нужен.

## Сборка
1. Пуск → "x64 Native Tools Command Prompt for VS 2022".
2. cd C:\src\cs2_wh
3. build_console.bat
4. Успех: [OK] cs2_wh.exe. Ошибки cl — кидай лог.

## Запуск
1. В настройках CS2 выбери режим окна Borderless (Fullscreen Windowed) —
   для консольной WH это не критично, но полезно если потом включишь GUI-оверлей.
2. Запусти свой VDC-экспортер (DLL внутри cs2.exe, пишет снапшоты в KDF-фабрику
   "CS2-VDC" по схеме offsets.hpp). Без него внешний клиент читать нечего —
   WH подключится к фабрике только когда экспортер её создаст.
3. Запусти cs2_wh.exe (права админа не нужны).
   - ждёт появления процесса cs2.exe;
   - крутится, пока не откроется фабрика (VDC появляется после map load);
   - как только attached — в консоли строки вида:
     [V] s1mple       hp 87   42.1m x:-1024 y:512 z:64 scr: 812,401 [HELM] ak47
     [V/X] — видим/невидим (spotted/shootable), scr — экранные координаты W2S.
   - ESC — выход.
4. Настройки (дистанция, visible-only, период чтения) — EspStyle и define'ы
   в config.hpp; пересобирать после правок.

## Layout-контракт (offsets.hpp)
Схема снапшота зафиксирована и проверена static_assert'ами:
  OFF_SEQNUM   0x000  UINT64 счётчик обновлений
  OFF_TICK     0x008  UINT32
  OFF_LOCAL    0x00C  LocalData (0x38): Pos+0x00 Vel+0x0C Yaw+0x18 Pitch+0x1C
                       Flags+0x20 Health+0x28 Team+0x2C
  OFF_COUNTS   0x044  INT32 total / aliveT / aliveCT
  OFF_ENTITIES 0x050  MAX_ENTS(64) записей EntityData по ENTITY_RECORD_SIZE=0x1A8
  OFF_VIEWPROJ 0x6A50 float[16] row-major viewmatrix (+reserve 0xC0)
  OFF_SIZE     0x6B10 полный размер снапшота
EntityData: Pos+0x00 Vel+0x10 Yaw+0x20 Pitch+0x24 HP+0x28 Team+0x2C Flags+0x30
  OriginOld+0x34 EyePos+0x40 HitboxPos[16]+0x4C Valid+0x10C Spotted+0x110
  Shootable+0x114 Flashing+0x118 Bullets+0x11C AbsVel+0x120 OnGround+0x124
  Reload+0x128 WeaponXuid+0x12C WeaponName+0x130 PlayerXuid+0x150 Name+0x154
  Money+0x194 ReadyToBuy+0x198 Helmet+0x19C Bomb+0x1A0 Scoped+0x1A4 (итого 0x1A8)
Любой рассинхрон с экспортером ловится компилятором (static_assert) или рантайм-
проверкой размера буфера (EspCore::Update).

## Кроссплатформенная проверка layout'а (на Linux/WSL, без MSVC)
g++ -std=c++20 _layout_check.cpp -o lc && ./lc   # должно быть LAYOUT OK

## Важно
- Консольная WH сама по себе ничего в игру не пишет и не читает — только shared-
  секцию экспортера. Если экспортной DLL нет — скажи, пишу write-side (dll+injector).
- После патчей CS2 сверяй offsets.hpp со своим дампером.
