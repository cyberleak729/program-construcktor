// config.hpp — все параметры ESP в одном месте. Правь без лезания в код.
#pragma once

#define KDF_FACTORY_NAME "CS2-VDC"   // имя фабрики твоего экспортера (VDC)
#ifndef MAX_ENTS
#define MAX_ENTS 64                  // записей в буфере экспортера (см. offsets.hpp)
#endif
#define MAX_ENTITIES MAX_ENTS        // алиас для кода отрисовки

// ---- Окно оверлея ----------------------------------------------------------
#define OVERLAY_TITLE L"CS2 Overlay"

// ---- Хитбоксы: индексы в EntityData::HitboxPos[16] -------------------------
enum HitboxIdx { HB_PELVIS = 0, HB_STOMACH, HB_LOW_TORSO, HB_CHEST,
                 HB_NECK_BASE, HB_NECK, HB_HEAD, HB_L_THIGH, HB_L_CALF,
                 HB_R_THIGH, HB_R_CALF, HB_L_HAND, HB_L_FOREARM, HB_L_UPPERARM,
                 HB_R_HAND, HB_R_FOREARM };
#define HB_FOOT  HB_PELVIS
#define HB_SKULL HB_HEAD

// Bones-скелет: список рёбер {from,to}
struct BoneEdge { int a, b; };
static const BoneEdge kSkeleton[] = {
  {HB_PELVIS, HB_STOMACH}, {HB_STOMACH, HB_LOW_TORSO}, {HB_LOW_TORSO, HB_CHEST},
  {HB_CHEST, HB_NECK_BASE}, {HB_NECK_BASE, HB_NECK}, {HB_NECK, HB_HEAD},
  {HB_PELVIS, HB_L_THIGH}, {HB_L_THIGH, HB_L_CALF}, {HB_L_CALF, HB_L_HAND},
  {HB_PELVIS, HB_R_THIGH}, {HB_R_THIGH, HB_R_CALF}, {HB_R_CALF, HB_R_HAND},
};

// ---- Цвета (ImColor-friendly RGBA float) ------------------------------------
struct EspStyle {
  bool  Boxes       = true;    // 2D-боксы
  int   BoxType     = 0;       // 0=corner, 1=full, 2=skeleton-only
  bool  Skeleton    = true;    // кости
  bool  HeadCircle  = true;    // окружность головы
  bool  Names       = true;
  bool  Health      = true;
  bool  Distance    = true;
  bool  Weapon      = true;
  bool  Flags       = true;    // C4/helmet/scoped/flash
  bool  Snaplines   = false;
  bool  Radar       = false;   // мини-радар в углу
  float MaxDist     = 300.f;   // метры; конверсия: meters = units * 0.0254
  bool  VisibleOnly = false;   // только spotted/shootable
  float LineThick   = 1.2f;
};

// ---- Чтение ------------------------------------------------------------------
#define READ_INTERVAL_MS 8        // период опроса seqnum (не чаще ~60Гц)
#define STALE_TIMEOUT_MS 1000     // если seqnum не меняется — данные протухли
