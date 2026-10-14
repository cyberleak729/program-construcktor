// dllmain.cpp — VDC-экспортер CS2: инъектируется в cs2.exe, создаёт KDF-фабрику
// "CS2-VDC" и пишет снапшоты по схеме offsets.hpp. Внешний WH (cs2_wh.exe) читает их снаружи.
//
// Поток: DllMain(ATTACH) -> CreateThread(Main)
//   Main: ждёт client.dll -> резолвит entity list / local pawn / view matrix по сигнатурам
//         -> kdf::FactoryServer::Create("CS2-VDC", OFF_SIZE)
//         -> цикл ~60 Гц: обход контроллеров (индексы 1..64), запись EntityData,
//            seqnum++ ПОСЛЕ данных (release) — клиент всегда видит согласованный снапшот.
//
// Инъекция: injector <pid> vdc_exporter.dll (CreateRemoteThread+LoadLibraryA).
// Для CS2 с VAC предпочтителен manual map; здесь обычный LoadLibrary-путь —
// выбор метода инжекта остаётся на твоём инструменте.
#include <windows.h>
#include <intrin.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>

#include "../kdf_write.hpp"
#include "../offsets.hpp"
#include "../config.hpp"   // HB_* индексы хитбоксов
#include "sdk.hpp"

using namespace sdk;

static volatile bool      g_Stop = false;
static kdf::FactoryServer g_Factory;

// ---- безопасное чтение float-полей pawn ---------------------------------------
static float ReadF(uintptr_t addr, float fallback = 0.f) {
#ifdef _MSC_VER
  __try { return *reinterpret_cast<float*>(addr); }
  __except (EXCEPTION_EXECUTE_HANDLER) { return fallback; }
#else
  return SafeReadT<float>(addr, fallback);
#endif
}

// ---- локальный игрок ----------------------------------------------------------
static void* GetLocalPawn() {
  if (!g.LocalPawn) return nullptr;
#ifdef _MSC_VER
  __try { return reinterpret_cast<void*>(*reinterpret_cast<uintptr_t*>(g.LocalPawn)); }
  __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
#else
  return (void*)SafeReadT<uintptr_t>(g.LocalPawn, 0);
#endif
}

static void FillLocal(LocalData* L) {
  memset(L, 0, sizeof(*L));
  void* pawn = GetLocalPawn();
  if (!pawn) return;
  Vec3f pos = GetWorldPos(pawn);
  L->Pos = { pos.x, pos.y, pos.z };
  L->Vel = {}; // velocity не экспортируем (нет предыдущего кадра в глобале) — WH считает сам
  L->Pitch = ReadF((uintptr_t)pawn + off::PAWN_ANG_PITCH);
  L->Yaw   = ReadF((uintptr_t)pawn + off::PAWN_ANG_YAW);
  L->Flags = (int32_t)GetFlags(pawn);
  L->Health = GetHealth(pawn);
  int32_t team = 0, helmet = 0, bomb = 0, money = 0;
  uint32_t slot = 0;
#ifdef _MSC_VER
  __try { slot = *(uint32_t*)((uintptr_t)pawn + off::PAWN_CONTROLLER_SLOT); } __except(EXCEPTION_EXECUTE_HANDLER) {}
#else
  slot = SafeReadT<uint32_t>((uintptr_t)pawn + off::PAWN_CONTROLLER_SLOT, 0);
#endif
  if (ReadResourceFields((void*)g.ResourceEnt, (int)slot, team, helmet, bomb, money)) L->Team = team;
  else L->Team = GetTeamRaw(pawn);
  L->pad2 = 0;
}

// ---- одна запись игрока -------------------------------------------------------
static bool FillEntity(EntityData* E, uint32_t controllerIdx) {
  memset(E, 0, sizeof(*E));
  void* controller = EntByIndex(controllerIdx);
  if (!controller) return false;
  void* pawn = GetPawnFromController(controller);
  if (!pawn) return false;
  int32_t hp = GetHealth(pawn);
  if (hp <= 0) return false;

  Vec3f pos = GetWorldPos(pawn);
  E->Pos = { pos.x, pos.y, pos.z };
  E->OriginOld[0] = pos.x; E->OriginOld[1] = pos.y; E->OriginOld[2] = pos.z;

  uintptr_t scene = 0;
#ifdef _MSC_VER
  __try { scene = *reinterpret_cast<uintptr_t*>((uintptr_t)pawn + off::SCENE_NODE); }
  __except (EXCEPTION_EXECUTE_HANDLER) { scene = 0; }
#else
  scene = SafeReadT<uintptr_t>((uintptr_t)pawn + off::SCENE_NODE, 0);
#endif
  if (scene) {
    // eye position = world pos + локальный eye-shift из CGameSceneNode
    Vec3f eye{};
#ifdef _MSC_VER
    __try { memcpy(&eye, (void*)(scene + off::EYE_OFFSET), sizeof(eye)); } __except (EXCEPTION_EXECUTE_HANDLER) {}
#else
    if (!IsBadReadPtr((void*)(scene + off::EYE_OFFSET), sizeof(eye)))
      memcpy(&eye, (void*)(scene + off::EYE_OFFSET), sizeof(eye));
#endif
    E->EyePos[0] = pos.x + eye.x;
    E->EyePos[1] = pos.y + eye.y;
    E->EyePos[2] = pos.z + eye.z;
  }

  // Хитбоксы: честный studio-скининг требует рендер-стейта модели; здесь — скелет-аппрокс
  // от позиции feet по константным Z-сдвигам (default player model, stand):
  static const float zhb[16] = {
    /*PELVIS*/0.f, /*STOMACH*/18.f, /*LOW_TORSO*/30.f, /*CHEST*/42.f,
    /*NECK_BASE*/54.f, /*NECK*/58.f, /*HEAD*/64.f, /*L_THIGH*/-4.f,
    /*L_CALF*/-32.f, /*R_THIGH*/-4.f, /*R_CALF*/-32.f, /*L_HAND*/40.f,
    /*L_FOREARM*/46.f, /*L_UPPERARM*/52.f, /*R_HAND*/40.f, /*R_FOREARM*/46.f
  };
  for (int hb = 0; hb < 16; ++hb) {
    E->HitboxPos[hb][0] = pos.x;
    E->HitboxPos[hb][1] = pos.y;
    E->HitboxPos[hb][2] = pos.z + zhb[hb];
  }

  E->Health = hp;
  int32_t team = 0, helmet = 0, bomb = 0, money = 0;
  uint32_t slot = 0;
#ifdef _MSC_VER
  __try { slot = *(uint32_t*)((uintptr_t)pawn + off::PAWN_CONTROLLER_SLOT); } __except (EXCEPTION_EXECUTE_HANDLER) {}
#else
  slot = SafeReadT<uint32_t>((uintptr_t)pawn + off::PAWN_CONTROLLER_SLOT, 0);
#endif
  if (ReadResourceFields((void*)g.ResourceEnt, (int)slot, team, helmet, bomb, money)) {
    E->Team = team; E->HasHelmet = helmet; E->HasBomb = bomb; E->Money = money;
  } else {
    E->Team = GetTeamRaw(pawn);
  }
  E->Flags     = (int32_t)GetFlags(pawn);
  E->OnGround  = (GetFlags(pawn) & FL_ONGROUND_BIT) ? 1 : 0;
  E->Valid     = 1;
  E->Spotted   = 1;      // server-side spotted из клиента без netvar-чипа недоступен
  E->Shootable = 1;      // trace-фильтр — задача WH (или свой EngineTrace здесь)
  E->Flashing  = ReadF((uintptr_t)pawn + off::PAWN_FLASH_DURATION) > 0.01f ? 1 : 0;
  GetPlayerName(controller, E->PlayerName, sizeof(E->PlayerName));
  E->PlayerXuid = (int32_t)controllerIdx;

  // оружие через weapon services
  uintptr_t ws = 0;
#ifdef _MSC_VER
  __try { ws = *reinterpret_cast<uintptr_t*>((uintptr_t)pawn + off::PAWN_WEAPON_SVC); }
  __except (EXCEPTION_EXECUTE_HANDLER) { ws = 0; }
#else
  ws = SafeReadT<uintptr_t>((uintptr_t)pawn + off::PAWN_WEAPON_SVC, 0);
#endif
  if (ws) {
    uint32_t hActive = 0;
#ifdef _MSC_VER
    __try { hActive = *(uint32_t*)(ws + off::WSVC_ACTIVE_WEP); } __except (EXCEPTION_EXECUTE_HANDLER) {}
#else
    hActive = SafeReadT<uint32_t>(ws + off::WSVC_ACTIVE_WEP, 0);
#endif
    void* wep = EntByIndex(hActive);
    if (wep) {
      struct CFStr { uint16_t len; uint16_t pad; char* ptr; };
#ifdef _MSC_VER
      __try {
#endif
        CFStr s = SafeReadT<CFStr>((uintptr_t)wep + off::WEP_NAME, CFStr{});
        if (s.ptr && s.len && !IsBadReadPtr(s.ptr, 1)) {
          size_t n = s.len < sizeof(E->WeaponName) - 1 ? s.len : sizeof(E->WeaponName) - 1;
          memcpy(E->WeaponName, s.ptr, n);
        }
        E->BulletsLeft = (float)SafeReadT<int32_t>((uintptr_t)wep + off::WEP_CLIPAMMO, 0);
#ifdef _MSC_VER
      } __except (EXCEPTION_EXECUTE_HANDLER) {}
#endif
    }
  }
  return true;
}

// CCSPlayerResource — сущность с фиксированным низким index; ищем перебором при каждом
// обновлении списка (дёшево: 1..10).
static void* FindResourceEntity() {
  for (uint32_t i = 1; i <= 10; ++i) {
    void* e = EntByIndex(i);
    if (!e) continue;
    uintptr_t svc = SafeReadT<uintptr_t>((uintptr_t)e + off::RES_PAWN_BASE, 0);
    if (svc > 0x1000 && svc < 0x7FFFFFFFFFFFULL) return e;
  }
  return nullptr;
}

// ---- главный поток экспортера --------------------------------------------------
static DWORD WINAPI Main(LPVOID) {
  HMODULE client = nullptr;
  while (!g_Stop && !(client = GetModuleHandleA("client.dll")))
    Sleep(500);
  if (g_Stop) return 0;
  g.Client = client;

  auto ripGlobal = [&](const char* pat, int dispOff, int insnEndOff) -> uintptr_t {
    uintptr_t hit = ScanModule(client, pat);
    if (!hit) return 0;
    return RipTo(hit + insnEndOff, *reinterpret_cast<int32_t*>(hit + dispOff));
  };
  // 48 8B 05 disp32 -> mov rax,[rip+disp], disp на +3, длина ins 7
  g.EntityList = ripGlobal(off::PAT_ENTITYLIST, 3, 7);
  g.LocalPawn  = ripGlobal(off::PAT_LOCALPAWN,  3, 7);
  // view matrix: mat_world глобал лежит рядом с кодом GetViewMatrix; если сигнатура
  // не совпала после патча — матрица не пишется, WH не покажет экранные координаты
  // (мировые позиции/дистанции продолжают работать). Правь PAT_VIEWMATRIX в sdk.hpp.
  uintptr_t vmHit = ScanModule(client, off::PAT_VIEWMATRIX);
  g.ViewMatrix = vmHit ? RipTo(vmHit + 11, *reinterpret_cast<int32_t*>(vmHit + 7)) : 0;

  if (!g.EntityList) return 0;
  if (!g_Factory.Create(KDF_FACTORY_NAME, OFF_SIZE)) return 0;
  auto* buf = reinterpret_cast<BYTE*>(g_Factory.Data());

  UINT64 seq = 0;
  LARGE_INTEGER qpf, t0, tc;
  QueryPerformanceFrequency(&qpf); QueryPerformanceCounter(&t0);

  while (!g_Stop) {
    INT32 total = 0, aliveT = 0, aliveCT = 0;
    auto* ents = reinterpret_cast<EntityData*>(buf + OFF_ENTITIES);
    g.ResourceEnt = (uintptr_t)FindResourceEntity();

    for (uint32_t i = 1; i <= MAX_ENTS_LOCAL; ++i) {
      EntityData tmp{};
      if (FillEntity(&tmp, i)) {
        ents[total++] = tmp;
        if (tmp.Team == 2) ++aliveT; else if (tmp.Team == 3) ++aliveCT;
      }
    }

    _mm_sfence(); // release: данные до seqnum
    *reinterpret_cast<UINT64*>(buf + OFF_SEQNUM) = ++seq;

    QueryPerformanceCounter(&tc);
    *reinterpret_cast<UINT32*>(buf + OFF_TICK) =
        (UINT32)((tc.QuadPart - t0.QuadPart) * 1000 / qpf.QuadPart);
    memcpy(buf + OFF_COUNTS, &total, 4);
    memcpy(buf + OFF_COUNTS + 4, &aliveT, 4);
    memcpy(buf + OFF_COUNTS + 8, &aliveCT, 4);

    LocalData L{}; FillLocal(&L);
    memcpy(buf + OFF_LOCAL, &L, sizeof(L));

    float vm[16]{};
    if (g.ViewMatrix) {
#ifdef _MSC_VER
      __try { memcpy(vm, (void*)g.ViewMatrix, sizeof(vm)); } __except (EXCEPTION_EXECUTE_HANDLER) {}
#else
      if (!IsBadReadPtr((void*)g.ViewMatrix, sizeof(vm)))
        memcpy(vm, (void*)g.ViewMatrix, sizeof(vm));
#endif
    }
    memcpy(buf + OFF_VIEWPROJ, vm, sizeof(vm));

    Sleep(16); // ~60 Hz
  }
  g_Factory.Destroy();
  return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(h);
    CreateThread(nullptr, 0, Main, nullptr, 0, nullptr);
  } else if (reason == DLL_PROCESS_DETACH) {
    g_Stop = true;
  }
  return TRUE;
}
