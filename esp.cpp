// esp.cpp — реализация GUI-отрисовки ESP (отдельный TU, чтобы не тянуть ImGui в заголовки).
#include <cstdio>

#include "esp.hpp"
#include "overlay.hpp"

void Esp::Draw(Overlay* ov, const EspStyle& st) {
  if (!Fresh()) return;
  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  const int W = ov->Width(), H = ov->Height();
  const LocalData* loc = Local();

  // ---- мини-радар -----------------------------------------------------------
  if (st.Radar) {
    const float R = 110.f, cx = (float)W - R - 16.f, cy = R + 16.f;
    dl->AddCircleFilled(ImVec2(cx, cy), R, IM_COL32(0, 0, 0, 90));
    dl->AddCircle(ImVec2(cx, cy), R, IM_COL32(255, 255, 255, 60));
    for (INT32 i = 0; i < MAX_ENTITIES; ++i) {
      const EntityData* e = Entity(i);
      if (!e->Valid || e->Health <= 0 || e->Team == loc->Team) continue;
      float dx = e->Pos.x - loc->Pos.x, dy = e->Pos.y - loc->Pos.y;
      float yaw = loc->Yaw * (3.14159265f / 180.f);
      // перевод в систему "спереди = вверх" относительно взгляда локального
      float rx = dx * cosf(yaw) + dy * sinf(yaw);
      float ry = dx * sinf(yaw) - dy * cosf(yaw);
      float scale = R / 2000.f;
      if (rx * rx + ry * ry > (R / scale) * (R / scale)) continue;
      dl->AddCircleFilled(ImVec2(cx + rx * scale, cy - ry * scale), 3.f,
                          IM_COL32(255, 60, 60, 220));
    }
  }

  // ---- сущности -------------------------------------------------------------
  INT32 total = TotalCount();
  if (total > MAX_ENTITIES) total = MAX_ENTITIES;

  for (INT32 i = 0; i < total; ++i) {
    const EntityData* e = Entity(i);
    if (!e->Valid) continue;
    if (e->Health <= 0) continue;
    if (e->Team == loc->Team) continue;                 // тиммейты не рисуем
    if (st.VisibleOnly && !e->Spotted && !e->Shootable) continue;

    Vec3 c{ e->Pos.x, e->Pos.y, e->Pos.z };
    float distM = sqrtf((c.x - loc->Pos.x) * (c.x - loc->Pos.x) +
                        (c.y - loc->Pos.y) * (c.y - loc->Pos.y) +
                        (c.z - loc->Pos.z) * (c.z - loc->Pos.z)) * UNIT_TO_METER;
    if (distM > st.MaxDist) continue;

    ScreenBox sb;
    if (!BoxOf(e, sb, W, H)) continue;                  // за камерой/слишком мелко

    bool visible = e->Spotted || e->Shootable;
    ImU32 col = visible ? IM_COL32(0, 255, 120, 255) : IM_COL32(255, 80, 80, 255);
    ImU32 skel = visible ? IM_COL32(255, 255, 255, 200) : IM_COL32(200, 200, 200, 120);

    // box
    if (st.Boxes && st.BoxType != 2) {
      if (st.BoxType == 1) {
        dl->AddRect(ImVec2(sb.x, sb.y), ImVec2(sb.x + sb.w, sb.y + sb.h),
                    col, 0, 0, st.LineThick);
      } else {
        float cl = sb.w * 0.25f, ct = sb.h * 0.2f;
        ImVec2 a(sb.x, sb.y), b(sb.x + sb.w, sb.y + sb.h);
        dl->AddLine({a.x, a.y}, {a.x + cl, a.y}, col, st.LineThick);
        dl->AddLine({a.x, a.y}, {a.x, a.y + ct}, col, st.LineThick);
        dl->AddLine({b.x, a.y}, {b.x - cl, a.y}, col, st.LineThick);
        dl->AddLine({b.x, a.y}, {b.x, a.y + ct}, col, st.LineThick);
        dl->AddLine({a.x, b.y}, {a.x + cl, b.y}, col, st.LineThick);
        dl->AddLine({a.x, b.y}, {a.x, b.y - ct}, col, st.LineThick);
        dl->AddLine({b.x, b.y}, {b.x - cl, b.y}, col, st.LineThick);
        dl->AddLine({b.x, b.y}, {b.x, b.y - ct}, col, st.LineThick);
      }
    }

    // skeleton
    if (st.Skeleton) {
      for (const auto& ed : kSkeleton) {
        Vec3 p0{ e->HitboxPos[ed.a][0], e->HitboxPos[ed.a][1], e->HitboxPos[ed.a][2] };
        Vec3 p1{ e->HitboxPos[ed.b][0], e->HitboxPos[ed.b][1], e->HitboxPos[ed.b][2] };
        float x0, y0, x1, y1;
        if (W2S(p0, x0, y0, W, H) && W2S(p1, x1, y1, W, H))
          dl->AddLine({x0, y0}, {x1, y1}, skel, st.LineThick);
      }
    }

    // head circle
    if (st.HeadCircle) {
      Vec3 h{ e->HitboxPos[HB_HEAD][0], e->HitboxPos[HB_HEAD][1], e->HitboxPos[HB_HEAD][2] };
      float hx, hy;
      if (W2S(h, hx, hy, W, H)) {
        Vec3 n{ e->HitboxPos[HB_NECK][0], e->HitboxPos[HB_NECK][1], e->HitboxPos[HB_NECK][2] };
        float nx, ny;
        float rad = W2S(n, nx, ny, W, H) ? fabsf(hy - ny) + 2.f : sb.h * 0.12f;
        dl->AddCircle({hx, hy}, rad, col, 0, st.LineThick);
      }
    }

    // snapline из нижнего центра экрана
    if (st.Snaplines)
      dl->AddLine({ W * 0.5f, (float)H }, { sb.x + sb.w * 0.5f, sb.y + sb.h },
                  col, st.LineThick);

    // инфо-строка над боксом: имя HP дистанция оружие флаги
    char info[256];
    int  off = 0;
    if (st.Names)  off += sprintf_s(info + off, sizeof(info) - off, "%s ",
                                    e->PlayerName[0] ? e->PlayerName : "?");
    if (st.Health) off += sprintf_s(info + off, sizeof(info) - off, "%dHP ", e->Health);
    if (st.Distance) off += sprintf_s(info + off, sizeof(info) - off, "%.0fm ", distM);
    if (st.Weapon && e->WeaponName[0])
      off += sprintf_s(info + off, sizeof(info) - off, "%s ", e->WeaponName);
    if (st.Flags) {
      if (e->HasHelmet) off += sprintf_s(info + off, sizeof(info) - off, "[HELM] ");
      if (e->HasBomb)   off += sprintf_s(info + off, sizeof(info) - off, "[C4] ");
      if (e->IsScoped)  off += sprintf_s(info + off, sizeof(info) - off, "[SCOPED] ");
      if (e->Flashing)  off += sprintf_s(info + off, sizeof(info) - off, "[FLASHED] ");
    }
    if (off > 0) {
      info[off - 1] = 0; // убрать хвостовой пробел
      ImVec2 ts = ImGui::CalcTextSize(info);
      dl->AddText({ sb.x + sb.w * 0.5f - ts.x * 0.5f, sb.y - ts.y - 2.f },
                  IM_COL32(255, 255, 255, 230), info);
    }

    // полоска HP слева от бокса
    if (st.Health) {
      float hpFrac = e->Health / 100.f;
      ImVec2 p0(sb.x - 5.f, sb.y + sb.h), p1(sb.x - 5.f, sb.y);
      dl->AddLine(p0, p1, IM_COL32(60, 60, 60, 200), 3.f);
      ImU32 hpc = hpFrac > 0.66f ? IM_COL32(0, 230, 0, 255) :
                  hpFrac > 0.33f ? IM_COL32(230, 200, 0, 255) : IM_COL32(230, 0, 0, 255);
      dl->AddLine(p0, { p0.x, p0.y + (p1.y - p0.y) * hpFrac }, hpc, 3.f);
    }
  }
}
