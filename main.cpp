// main.cpp — точка входа ESP-оверлея для CS2 (VDC/KDF external read).
// Сборка: x64 Release, /MT. См. build.bat.
#include <windows.h>
#include <tlhelp32.h>

#include "overlay.hpp"
#include "esp.hpp"
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

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  Overlay ov;
  if (!ov.Init(OVERLAY_TITLE)) return 1;

  Esp esp;
  EspStyle st;              // дефолты из config.hpp

  // ждём запуска игры, потом подключаемся к фабрике экспортера
  while (!IsGameRunning(L"cs2.exe") && !ov.Quit()) Sleep(500);
  for (;;) {
    if (esp.Attach()) break;
    if (ov.Quit()) return 1;
    Sleep(1000);            // VDC-фабрика появляется после map load
  }

  ULONGLONG lastRead = 0;
  while (!ov.Quit()) {
    ov.BeginFrame();

    ULONGLONG now = GetTickCount64();
    if (now - lastRead >= READ_INTERVAL_MS) { esp.Update(); lastRead = now; }

    if (ov.ShowMenu()) {
      ImGui::SetNextWindowPos({ 20, 20 }, ImGuiCond_FirstUseEver);
      ImGui::Begin("ESP Config");
      ImGui::Checkbox("Boxes", &st.Boxes);
      ImGui::Combo("Box type", &st.BoxType, "Corners\0Filled\0Skeleton only\0");
      ImGui::Checkbox("Skeleton", &st.Skeleton);
      ImGui::Checkbox("Head circle", &st.HeadCircle);
      ImGui::Checkbox("Names", &st.Names);
      ImGui::Checkbox("Health", &st.Health);
      ImGui::Checkbox("Distance", &st.Distance);
      ImGui::Checkbox("Weapon", &st.Weapon);
      ImGui::Checkbox("Flags", &st.Flags);
      ImGui::Checkbox("Snaplines", &st.Snaplines);
      ImGui::Checkbox("Radar", &st.Radar);
      ImGui::Checkbox("Visible only", &st.VisibleOnly);
      ImGui::SliderFloat("Max dist (m)", &st.MaxDist, 20.f, 200.f, "%.0f");
      ImGui::SliderFloat("Line thickness", &st.LineThick, 0.5f, 4.f, "%.1f");
      ImGui::Separator();
      ImGui::Text("%s | seq fresh=%d total=%d",
                  esp.Attached() ? "KDF attached" : "KDF detached",
                  (int)esp.Fresh(), esp.Attached() ? esp.TotalCount() : -1);
      ImGui::End();
    }

    esp.Draw(&ov, st);
    ov.EndFrame();

    Sleep(1); // vsync в Present держит 60Гц; тут только уступаем квант
  }

  esp.Detach();
  return 0;
}
