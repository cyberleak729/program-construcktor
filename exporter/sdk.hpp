// sdk.hpp — офсеты и геттеры для CS2 (build 14285).
// Смещения даны как паттерны от модулей; резолв — FindPattern по образцу.
// Если после патча игра не находит сигнатуру — обнови её здесь, всё остальное не трогай.
#pragma once
#include <windows.h>
#include <psapi.h>
#include <cstdint>
#include <cstring>

#define MAX_ENTS_LOCAL 64 // синхронно с offsets.hpp клиента

// ---- утилита поиска паттерна -------------------------------------------------
inline uintptr_t FindPattern(uintptr_t base, size_t size, const BYTE* pat, const char* mask) {
  auto cmp = [&](uintptr_t a) {
    for (size_t i = 0; mask[i]; ++i)
      if (mask[i] == 'x' && *(BYTE*)(a + i) != pat[i]) return false;
    return true;
  };
  for (uintptr_t p = base, e = base + size - strlen(mask); p <= e; p += 16)
    if (cmp(p)) return p;
  // fallback: побайтовый шаг
  for (uintptr_t p = base, e = base + size - strlen(mask); p <= e; ++p)
    if (cmp(p)) return p;
  return 0;
}

inline size_t ModuleSize(HMODULE h) {
  auto dos = (PIMAGE_DOS_HEADER)h;
  auto nt = (PIMAGE_NT_HEADERS)((BYTE*)h + dos->e_lfanew);
  return nt->OptionalHeader.SizeOfImage;
}

// разбор hex-строки "48 8D 0D ? ? ? ?" в байты; '?' -> 0 с wildcard в маске
inline size_t ParsePattern(const char* str, BYTE* pat, char* maskOut, size_t cap) {
  size_t n = 0;
  for (const char* p = str; *p && n < cap - 1; ) {
    while (*p == ' ') ++p;
    if (!*p) break;
    if (*p == '?') { pat[n] = 0; maskOut[n] = '?'; ++n; p += 2; continue; }
    auto hx = [](char c) -> int { return (c >= 'a') ? c - 'a' + 10 : (c >= 'A' ? c - 'A' + 10 : c - '0'); };
    pat[n] = (BYTE)(hx(p[0]) * 16 + hx(p[1]));
    maskOut[n] = 'x';
    ++n; p += 3;
  }
  maskOut[n] = 0;
  return n;
}

// SEH-обёртки: MSVC — __try/__except; mingw-g++ — VCC exception spec (не генерируется,
// но синтаксис проходит; на mingw читай через ReadSafe ниже).
#ifdef _MSC_VER
  #define SAFE_READ(expr, fallback) __pragma(warning(push)) __pragma(warning(disable:4505)) \
    [&]() { __try { return (expr); } __except(EXCEPTION_EXECUTE_HANDLER) { return (fallback); } }()
#else
  // mingw: структурная защита через VEH-фильтр не нужна — читаем только валидные указатели,
  // ошибки ловит IsBadReadPtr.
  template <typename T>
  inline T SafeReadT(uintptr_t addr, T fallback) {
    if (!addr || IsBadReadPtr((void*)addr, sizeof(T))) return fallback;
    return *reinterpret_cast<T*>(addr);
  }
  #define SAFE_READ_T(type, addr, fallback) SafeReadT<type>((uintptr_t)(addr), fallback)
#endif

// относительный rip-displacement -> абсолютный адрес цели
inline uintptr_t RipTo(uintptr_t insnEnd, int32_t disp) { return insnEnd + disp; }

// поиск сигнатуры в образе модуля (страница guard-безопасно: ищем только по .text/.rdata — здесь просто весь образ)
inline uintptr_t ScanModule(HMODULE h, const char* pattern) {
  BYTE pat[64]; char mask[64];
  size_t len = ParsePattern(pattern, pat, mask, sizeof(pat));
  if (!len) return 0;
  auto dos = (PIMAGE_DOS_HEADER)h;
  auto nt = (PIMAGE_NT_HEADERS)((BYTE*)h + dos->e_lfanew);
  auto sec = IMAGE_FIRST_SECTION(nt);
  for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
    if (!(sec->Misc.VirtualSize)) continue;
    if (sec->Characteristics & IMAGE_SCN_MEM_DISCARDABLE) continue;
    uintptr_t s = (uintptr_t)h + sec->VirtualAddress;
    size_t sz = sec->Misc.VirtualSize;
    if (sz < len) continue;
    for (size_t o = 0; o + len <= sz; ++o) {
      bool ok = true;
      for (size_t k = 0; k < len; ++k)
        if (mask[k] == 'x' && ((BYTE*)s)[o + k] != pat[k]) { ok = false; break; }
      if (ok) return s + o;
    }
  }
  return 0;
}

// ---- указатели на глобалы, заполняются в ExporterInit() -----------------------
namespace sdk {

struct Ptrs {
  HMODULE   Client       = nullptr; // client.dll
  uintptr_t EntityList   = 0; // CGameEntitySystem** (global)
  uintptr_t ViewMatrix   = 0; // float[16] view-projection (row-major)
  uintptr_t LocalPawn    = 0; // CBasePlayerPawn** (global)
  uintptr_t ResourceEnt  = 0; // CCSPlayerResource* (кэш, обновляется каждый кадр)
  bool Ready = false;
};

inline Ptrs g;

// офсеты — стабильные публичные значения build 14285 (после патчей сверять ReClass/дампером)
namespace off {
  // client.dll globals (сигнатуры резолвятся в ExporterInit)
  constexpr const char* PAT_ENTITYLIST = "48 8B 05 ? ? ? ? 48 8D 4D ? FF 15 ? ? ? ? 33 C0";
  constexpr const char* PAT_LOCALPAWN  = "48 8B 05 ? ? ? ? 48 85 C0 74 ? 8B 90";
  constexpr const char* PAT_VIEWMATRIX = "48 8D 0C D9 F3 0F 10 84 24"; // mat_world: lea rcx,[rbx+rbx*8] рядом

  constexpr uintptr_t ENT_SYS_ENTRIES = 0x5C8;   // CGameEntitySystem::entity array base
  constexpr uintptr_t ENT_SYS_MAX     = 0x40000; // ёмкость списка
  constexpr uintptr_t SCENE_NODE      = 0x1A0;   // CEntityInstance::m_pScene (CGameSceneNode*)
  constexpr uintptr_t WORLD_POS       = 0x88;    // CGameSceneNode::m_vecWorldPosition
  constexpr uintptr_t EYE_OFFSET      = 0x58;    // CGameSceneNode::m_vecOrigin (локальный eye-shift)
  constexpr uintptr_t CTRL_PAWN_HANDLE= 0x7C8;   // CPlayerController::m_hPawn
  constexpr uintptr_t CTRL_NAME       = 0x3E0;   // CPlayerController::m_iszPlayerName (CStringToken: u16 len + ptr)
  constexpr uintptr_t PAWN_HEALTH     = 0x340;   // C_BasePlayerPawn::m_iHealth
  constexpr uintptr_t PAWN_TEAM       = 0x45B;   // CPlayer_MovementServices_Pawn::m_iTeamNum (в pawn)
  constexpr uintptr_t PAWN_FLAGS      = 0x3EC;   // m_fFlags
  constexpr uintptr_t PAWN_CONTROLLER_SLOT = 0x169C; // m_iControllerSlot? нет: m_hController рядом — slot берём из index контроллера при обходе, здесь fallback
  constexpr uintptr_t PAWN_FLASH_DURATION  = 0x1678; // m_flFlashDuration (CCSPlayerPawn block)
#define FL_ONGROUND_BIT 1u
  constexpr uintptr_t PAWN_ANG_PITCH  = 0x120C;  // m_angEyeAngles.x (pitch)
  constexpr uintptr_t PAWN_ANG_YAW    = 0x1210;  // m_angEyeAngles.y (yaw)
  constexpr uintptr_t PAWN_ITEM_SVC   = 0x500;   // m_pItemServices? нет: weapon services
  constexpr uintptr_t PAWN_WEAPON_SVC = 0x500;   // CPlayer_WeaponServices*
  constexpr uintptr_t WSVC_ACTIVE_WEP = 0x58;    // m_hActiveWeapon (CEntIndex)
  constexpr uintptr_t WEP_CLIPAMMO    = 0x5C0;   // m_iClip1
  constexpr uintptr_t WEP_NAME        = 0x3C;    // CStringToken имени definition (упрощённо)
  constexpr uintptr_t RES_PAWN_BASE   = 0x133C;  // CCSPlayerResource::m_pawnServicesForPlayers[0]
  constexpr uintptr_t PS_TEAM         = 0x3C;    // PlayerServices::m_iTeamNum
  constexpr uintptr_t PS_HELMET       = 0x41;    // bHasHelmet
  constexpr uintptr_t PS_BOMB         = 0x42;    // bHasBomb (carrier flag byte)
  constexpr uintptr_t PS_MONEY        = 0x44;    // m_iAccount
  constexpr uintptr_t FLASH_DURATION  = 0x38;    // m_flFlashDuration в pawn (ccs player camera block)
}

// ---- доступ к объектам --------------------------------------------------------
inline void* EntByIndex(uint32_t rawIndex) {
  if (!g.EntityList) return nullptr;
  uintptr_t sys = *reinterpret_cast<uintptr_t*>(g.EntityList);
  if (!sys) return nullptr;
  uint32_t i = rawIndex & 0xFFFF;
  if (i >= off::ENT_SYS_MAX) return nullptr;
  return reinterpret_cast<void*>(*reinterpret_cast<uintptr_t*>(sys + off::ENT_SYS_ENTRIES + (uintptr_t)i * 8));
}

struct Vec3f { float x, y, z; };
struct QAngle { float pitch, yaw, roll; };

inline Vec3f GetWorldPos(void* pawn) {
  Vec3f v{};
  if (!pawn) return v;
  uintptr_t scene = *reinterpret_cast<uintptr_t*>((uintptr_t)pawn + off::SCENE_NODE);
  if (!scene) return v;
  memcpy(&v, (void*)(scene + off::WORLD_POS), sizeof(v));
  return v;
}

inline int32_t GetHealth(void* pawn) {
  if (!pawn) return 0;
#ifdef _MSC_VER
  __try { return *(int32_t*)((uintptr_t)pawn + off::PAWN_HEALTH); }
  __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
#else
  return SafeReadT<int32_t>((uintptr_t)pawn + off::PAWN_HEALTH, 0);
#endif
}
inline int32_t GetTeamRaw(void* pawn) {
  if (!pawn) return 0;
#ifdef _MSC_VER
  __try { return *(int32_t*)((uintptr_t)pawn + off::PAWN_TEAM); }
  __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
#else
  return SafeReadT<int32_t>((uintptr_t)pawn + off::PAWN_TEAM, 0);
#endif
}
inline uint32_t GetFlags(void* pawn) {
  if (!pawn) return 0;
#ifdef _MSC_VER
  __try { return *(uint32_t*)((uintptr_t)pawn + off::PAWN_FLAGS); }
  __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
#else
  return SafeReadT<uint32_t>((uintptr_t)pawn + off::PAWN_FLAGS, 0);
#endif
}

// имя из контроллера (CStringToken: uint16 len, pad, char* ptr)
inline void GetPlayerName(void* controller, char* out, size_t cap) {
  out[0] = 0;
  if (!controller) return;
  struct CFStr { uint16_t len; uint16_t pad; char* ptr; };
#ifdef _MSC_VER
  __try {
    CFStr s = *(CFStr*)((uintptr_t)controller + off::CTRL_NAME);
    if (s.ptr && s.len) {
      size_t n = s.len < cap - 1 ? s.len : cap - 1;
      memcpy(out, s.ptr, n);
      out[n] = 0;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) { out[0] = 0; }
#else
  CFStr s = SafeReadT<CFStr>((uintptr_t)controller + off::CTRL_NAME, CFStr{});
  if (s.ptr && s.len && !IsBadReadPtr(s.ptr, 1)) {
    size_t n = s.len < cap - 1 ? s.len : cap - 1;
    memcpy(out, s.ptr, n);
    out[n] = 0;
  }
#endif
}

inline void* GetPawnFromController(void* controller) {
  if (!controller) return nullptr;
#ifdef _MSC_VER
  __try {
    uint32_t h = *(uint32_t*)((uintptr_t)controller + off::CTRL_PAWN_HANDLE);
    return EntByIndex(h);
  } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
#else
  uint32_t h = SafeReadT<uint32_t>((uintptr_t)controller + off::CTRL_PAWN_HANDLE, 0);
  return EntByIndex(h);
#endif
}

// team/helmet/bomb/money читаем из CCSPlayerResource (per-player массив PlayerServices)
inline bool ReadResourceFields(void* resource, int slot, int32_t& team, int32_t& helmet,
                               int32_t& bomb, int32_t& money) {
  if (!resource || slot < 0 || slot > 63) return false;
#ifdef _MSC_VER
  __try {
#endif
    uintptr_t svc = SafeReadT<uintptr_t>((uintptr_t)resource + off::RES_PAWN_BASE + slot * 8, 0);
    if (!svc) return false;
    team   = SafeReadT<int32_t>(svc + off::PS_TEAM, 0);
    helmet = SafeReadT<uint8_t>(svc + off::PS_HELMET, 0) ? 1 : 0;
    bomb   = SafeReadT<uint8_t>(svc + off::PS_BOMB, 0) ? 1 : 0;
    money  = SafeReadT<int32_t>(svc + off::PS_MONEY, 0);
    return true;
#ifdef _MSC_VER
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
#endif
}

} // namespace sdk
