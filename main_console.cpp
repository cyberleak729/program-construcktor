// main_console.cpp — "запустил и WH работает": консольная версия без оверлея.
// Подключаемся к KDF-фабрике экспортера, печатаем координаты врагов в консоль.
// Сборка: x64 Release /MT, зависимостей нет (см. build_console.bat).
#include <windows.h>
#include <tlhelp32.h>

#include "esp_core.hpp"
#include "console.hpp"
#include "config.hpp"

static bool IsGameRunning(const wchar_t* procName) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32W pe{ .dwSize = sizeof(pe) };
  bool found = false;
  if (Process32FirstW(snap, &pe)) {
    do { if (_wcsicmp(pe.szExeFile, procName) == 0) { found = true; break; } }
    while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  return found;
}

static void Say(const char* s) {
  wchar_t w[256];
  MultiByteToWideChar(CP_UTF8, 0, s, -1, w, 256);
  WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), w, (DWORD)wcslen(w), nullptr, nullptr);
  WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), L"\n", 1, nullptr, nullptr);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
  ConsoleEsp cons;
  if (!cons.Open()) return 1;               // AllocConsole внутри
  Say("CS2 WH console | ждём cs2.exe ...");

  EspCore esp;
  EspStyle st;                               // дефолты из config.hpp

  while (!IsGameRunning(L"cs2.exe")) Sleep(500);
  Say("cs2.exe найден | подключаюсь к KDF-фабрике ...");

  // VDC-фабрика появляется после map load — крутимся, пока не откроемся
  while (!esp.Attach()) {
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) return 0;
    Sleep(1000);
  }
  Say("KDF attached | WH активен | ESC — выход");

  ULONGLONG lastRead = 0;
  while (true) {
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) break;

    ULONGLONG now = GetTickCount64();
    if (now - lastRead >= READ_INTERVAL_MS) { esp.Update(); lastRead = now; }

    cons.Draw(esp, st);                      // перерисовка консоли in-place
    Sleep(33);                               // ~30 FPS, меньше долбит консоль
  }

  esp.Detach();
  return 0;
}
