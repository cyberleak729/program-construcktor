// console.hpp — примитивный "консоль-ESP": вывод координат врагов в консоль
// вместо графического оверлея. Без ImGui/D3D, только WriteConsoleW.
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstdio>

#include "esp_core.hpp"
#include "config.hpp"

class ConsoleEsp {
public:
  bool Open() {
    AllocConsole();                      // если стартовали без консоли — создадим
    SetConsoleOutputCP(CP_UTF8);
    m_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!m_hOut || m_hOut == INVALID_HANDLE_VALUE) return false;
    CONSOLE_SCREEN_BUFFER_INFO si{};
    GetConsoleScreenBufferInfo(m_hOut, &si);
    m_Cols = si.dwSize.X ? si.dwSize.X : 120;
    return true;
  }

  void Draw(const EspCore& esp, const EspStyle& st) {
    if (!esp.Fresh()) return;
    const LocalData* loc = esp.Local();

    // курсор в начало + затирание строки заголовка
    COORD home{ 0, 0 };
    SetConsoleCursorPosition(m_hOut, home);

    char line[256];
    int n = sprintf_s(line, "CS2 WH | T:%d HP:%d pos %.0f %.0f %.0f | enemies:%d",
                      loc->Team, loc->Health, loc->Pos.x, loc->Pos.y, loc->Pos.z,
                      CountEnemies(esp, st));
    Pad(line, n);
    WriteA(line);

    INT32 total = esp.TotalCount();
    if (total > MAX_ENTITIES) total = MAX_ENTITIES;
    for (INT32 i = 0; i < total; ++i) {
      const EntityData* e = esp.Entity(i);
      if (!DrawOne(esp, *e, loc, st, line)) continue;
      Pad(line, (int)strlen(line));
      WriteA(line);
    }
  }

private:
  static int CountEnemies(const EspCore& esp, const EspStyle& st) {
    INT32 total = esp.TotalCount(); if (total > MAX_ENTITIES) total = MAX_ENTITIES;
    const LocalData* loc = esp.Local();
    int cnt = 0;
    for (INT32 i = 0; i < total; ++i) {
      const EntityData* e = esp.Entity(i);
      if (!IsEnemy(e, loc, st)) continue;
      ++cnt;
    }
    return cnt;
  }

  static bool IsEnemy(const EntityData* e, const LocalData* loc, const EspStyle& st) {
    if (!e->Valid || e->Health <= 0) return false;
    if (e->Team == loc->Team) return false;
    if (st.VisibleOnly && !e->Spotted && !e->Shootable) return false;
    float d = DistMeters(e->Pos, loc->Pos);
    if (d > st.MaxDist) return false;
    return true;
  }

  static float DistMeters(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return sqrtf(dx * dx + dy * dy + dz * dz) * UNIT_TO_METER;
  }

  bool DrawOne(const EspCore& esp, const EntityData& e, const LocalData* loc,
               const EspStyle& st, char* out) {
    if (!IsEnemy(&e, loc, st)) return false;
    float dist = DistMeters(e.Pos, loc->Pos);

    // экранная позиция через W2S из viewmatrix (реальное положение на экране)
    float sx = -1, sy = -1;
    RECT rc{};
    GetClientRect(GetDesktopWindow(), &rc);
    bool onScreen = esp.W2S(e.Pos, sx, sy, rc.right, rc.bottom);

    int n = sprintf_s(out, 256,
        "[%c] %-16s hp%3d %6.1fm x:%7.0f y:%7.0f z:%7.0f scr:%4d,%4d %s%s%s",
        (e.Spotted || e.Shootable) ? 'V' : 'X',
        e.PlayerName[0] ? e.PlayerName : "?",
        e.Health, dist, e.Pos.x, e.Pos.y, e.Pos.z,
        onScreen ? (int)sx : -1, onScreen ? (int)sy : -1,
        e.HasBomb ? "[C4]" : "",
        e.HasHelmet ? "[HELM]" : "",
        e.Flashing ? "[FLASH]" : "");
    if (e.WeaponName[0])
      sprintf_s(out + n, 256 - n, "%s", e.WeaponName); // дозаписать оружие
    return true;
  }

  void Pad(char* line, int len) {
    while (len < m_Cols - 1 && len < 254) line[len++] = ' ';
    line[len] = 0;
  }

  void WriteA(const char* s) {
    wchar_t w[512];
    int wn = MultiByteToWideChar(CP_UTF8, 0, s, -1, w, 512);
    if (wn > 0) WriteConsoleW(m_hOut, w, wn - 1, nullptr, nullptr);
  }

  HANDLE m_hOut = nullptr;
  int m_Cols = 120;
};
