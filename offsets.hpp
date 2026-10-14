// offsets.hpp — схема shared-буфера VDC-экспортера (CS2 build 14285).
// Формат little-endian, без выравнивания. Меняй под свой экспортер.
#pragma once
#include <cstdint>
#include <cstddef>

#ifndef MAX_ENTS
#define MAX_ENTS 64            // записей в буфере экспортера (см. config.hpp)
#endif

#define OFF_SEQNUM   0x000   // UINT64 — счётчик обновлений
#define OFF_TICK     0x008   // UINT32
#define OFF_LOCAL    0x00C   // LocalData (0x38)
#define OFF_COUNTS   0x044   // INT32 total / aliveT / aliveCT
#define OFF_ENTITIES 0x050   // запись ENTITY_RECORD_SIZE (0x1A8), i = base + 0x050 + i*0x1A8
#define OFF_VIEWPROJ (OFF_ENTITIES + MAX_ENTS * ENTITY_RECORD_SIZE) // float[16], row-major: world->clip = v*M, w' = x*M03+y*M13+z*M23+M33
#define OFF_SIZE     (OFF_VIEWPROJ + 0xC0)  // полный размер снапшота (viewmatrix + reserve)

struct Vec3 { float x, y, z; };

struct LocalData {
  Vec3   Pos;          // +0x00
  float  pad0;
  Vec3   Vel;          // +0x0C
  float  pad1;
  float  Yaw;          // +0x18
  float  Pitch;        // +0x1C
  int32_t  Flags;      // +0x20  (FL_ONGROUND=1<<0, FL_DUCKING=1<<1)
  int32_t  pad2;       // +0x24
  int32_t  Health;     // +0x28
  int32_t  Team;       // +0x2C  (2=T, 3=CT)
};

// Запись 0x1A8 байт. Поля идут строго по смещениям; padding-поля закрывают дыры
// до следующего поля. Смещения соответствуют типичному public VDC-layout.
#pragma pack(push, 1)
struct EntityData {
  Vec3    Pos;                    // +0x000
  float   pad0;                   // +0x00C
  Vec3    Vel;                    // +0x010
  float   pad1;                   // +0x01C
  float   Yaw;                    // +0x020
  float   Pitch;                  // +0x024
  int32_t Health;                 // +0x028
  int32_t Team;                   // +0x02C  (2=T, 3=CT)
  int32_t Flags;                  // +0x030
  float   OriginOld[3];           // +0x034 — предыдущий кадр (расчёт скорости)
  float   EyePos[3];              // +0x040
  float   HitboxPos[16][3];       // +0x04C .. +0x10B, 16 хитбоксов
  int32_t Valid;                  // +0x10C
  int32_t Spotted;                // +0x110  (bitmask: 0=не виден)
  int32_t Shootable;              // +0x114
  int32_t Flashing;               // +0x118
  float   BulletsLeft;            // +0x11C
  float   AbsVelocity;            // +0x120
  int32_t OnGround;               // +0x124
  int32_t ReloadProgress;         // +0x128
  int32_t WeaponXuid;             // +0x12C
  char    WeaponName[32];         // +0x130
  int32_t PlayerXuid;             // +0x150
  char    PlayerName[64];         // +0x154 — буфер ника UTF-8
  int32_t Money;                  // +0x194
  int32_t ReadyToBuy;             // +0x198
  int32_t HasHelmet;              // +0x19C
  int32_t HasBomb;                // +0x1A0
  int32_t IsScoped;               // +0x1A4
};
#pragma pack(pop)

#define ENTITY_RECORD_SIZE 0x1A8
static_assert(sizeof(EntityData) == ENTITY_RECORD_SIZE, "EntityData size mismatch");
// страховка от рассинхрона со схемой экспортера — компилятор поймает любой сдвиг
static_assert(offsetof(EntityData, HitboxPos)  == 0x04C, "HB offset");
static_assert(offsetof(EntityData, Valid)      == 0x10C, "Valid offset");
static_assert(offsetof(EntityData, PlayerName) == 0x154, "Name offset");
static_assert(offsetof(EntityData, HasHelmet)  == 0x19C, "Helmet offset");
