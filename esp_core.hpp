// esp_core.hpp — чистая логика ESP без ImGui: attach к KDF, снапшот, W2S, боксы.
// Используется и GUI-оверлеем (esp.cpp), и консольным WH (console.hpp).
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cmath>

#include "kdf_read.hpp"
#include "offsets.hpp"
#include "config.hpp"

#define UNIT_TO_METER 0.0254f // source units -> meters

struct ScreenBox { float x, y, w, h; };

class EspCore {
public:
  bool Attach(const char* factory = KDF_FACTORY_NAME) { return m_Kdf.Open(factory); }
  void Detach() { m_Kdf.Close(); }
  bool Attached() const { return m_Kdf.Data() != nullptr; }

  // Копируем снапшот, только если seqnum изменился (read-consistency).
  void Update() {
    if (!Attached()) return;
    auto base = reinterpret_cast<const BYTE*>(m_Kdf.Data());
    UINT64 seq = *reinterpret_cast<const UINT64*>(base + OFF_SEQNUM);
    if (seq == m_LastSeq) return;
    if (m_Kdf.Size() < OFF_SIZE) return; // буфер короче схемы — не читаем за пределы
    memcpy(&m_Snap, base, OFF_SIZE);
    m_LastSeq = seq;
    m_LastUpdate = GetTickCount64();
  }

  bool Fresh() const { return Attached() &&
    (GetTickCount64() - m_LastUpdate) < STALE_TIMEOUT_MS; }

  const LocalData* Local() const { return reinterpret_cast<const LocalData*>(&m_Snap[OFF_LOCAL]); }
  const float* ViewProj() const { return reinterpret_cast<const float*>(&m_Snap[OFF_VIEWPROJ]); }
  INT32 TotalCount() const { return *reinterpret_cast<const INT32*>(&m_Snap[OFF_COUNTS]); }
  UINT64 Seq() const { return m_LastSeq; }

  const EntityData* Entity(INT32 i) const {
    return reinterpret_cast<const EntityData*>(&m_Snap[OFF_ENTITIES]) + i;
  }

  // world -> screen. Возврат false, если точка за камерой.
  bool W2S(const Vec3& w, float& sx, float& sy, int sw, int sh) const {
    const float* m = ViewProj();
    float px = w.x * m[0] + w.y * m[4] + w.z * m[8]  + m[12];
    float py = w.x * m[1] + w.y * m[5] + w.z * m[9]  + m[13];
    float pw = w.x * m[3] + w.y * m[7] + w.z * m[11] + m[15];
    if (pw < 0.001f) return false;
    sx = (sw / 2.f) * (1.f + px / pw);
    sy = (sh / 2.f) * (1.f - py / pw);
    return true;
  }

  // Бокс по хитбоксам feet->skull с запасом по ширине.
  bool BoxOf(const EntityData* e, ScreenBox& out, int sw, int sh) const {
    float fx, fy, hx, hy;
    Vec3 feet{ e->HitboxPos[HB_FOOT][0],  e->HitboxPos[HB_FOOT][1],  e->HitboxPos[HB_FOOT][2] };
    Vec3 head{ e->HitboxPos[HB_SKULL][0], e->HitboxPos[HB_SKULL][1], e->HitboxPos[HB_SKULL][2] };
    if (!W2S(feet, fx, fy, sw, sh)) return false;
    if (!W2S(head, hx, hy, sw, sh)) return false;
    float h = fabsf(fy - hy);
    if (h < 4.f) return false;
    out.w = h * 0.35f;
    out.x = (fx + hx) * 0.5f - out.w * 0.5f;
    out.y = hy - h * 0.06f; // верх шапки чуть выше skull
    out.h = h * 1.12f;
    return true;
  }

private:
  kdf::Factory m_Kdf;
  BYTE   m_Snap[OFF_SIZE]{};
  UINT64 m_LastSeq = 0;
  ULONGLONG m_LastUpdate = 0;
};
