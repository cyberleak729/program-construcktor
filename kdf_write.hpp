// kdf_write.hpp — Keyed Data Factory (VDC) SERVER-сторона, x64 only.
// Используется внутри injected DLL (exporter). Протокол зеркален kdf_read.hpp:
// NtQuerySystemInformation(SystemReservedInformation=0x2D), код 0x5A.
// sub-op: 1=open(создаёт фабрику+секцию), 2=query(size), 3=close.
#pragma once
#include <windows.h>
#include <winternl.h>
#include <cstdint>
#include <cstring>

#define KDF_MAGIC      "KDMFD"
#define KDF_MAX_NAME   48
#define KDF_MAX_DATA   0x100000
#define KDF_OP_EXCHANGE 0x5A

typedef NTSTATUS(WINAPI* QuerySysInfo_t)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);

struct KDF_ATTR_IN {
  char Name[KDF_MAX_NAME];
  UINT64 CreateFlags;
  UINT64 QueryFlags;
  UINT64 ValueSize;
  UINT64 MaxObjectCount;
};

struct KDF_ATTR_OUT {
  HANDLE FactoryHandle;
  HANDLE SectionHandle;
  UINT64 Unknown;
  UINT64 DataSize;
  UINT32 ObjectCount;
  LARGE_INTEGER SystemTime;
};

struct KDF_PAYLOAD {
  UINT32 Code;
  UINT32 SubOp;
  union {
    struct { UINT64 a; UINT64 b; } Close;
    struct { HANDLE FactoryHandle; char Name[KDF_MAX_NAME]; UINT64 c; UINT64 d; } Open;
    struct { HANDLE hFactory; HANDLE hObj; UINT64 a; UINT64 b; BYTE v1; BYTE v2; BYTE v3; } Query;
  };
};

struct KDF_RESULT {
  UINT64 Status;
  KDF_ATTR_OUT Out;
};

namespace kdf {

inline NTSTATUS Exchange(const KDF_ATTR_IN* in, KDF_ATTR_OUT* out,
                         const KDF_PAYLOAD* payload, KDF_RESULT* result) {
  static QuerySysInfo_t pQSII = [] {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    return reinterpret_cast<QuerySysInfo_t>(
        GetProcAddress(ntdll, "NtQuerySystemInformation"));
  }();

  struct {
    const void* AttrIn; SIZE_T AttrInSz;
    void* AttrOut;    SIZE_T AttrOutSz;
    const void* Payload; SIZE_T PayloadSz;
    void* Result;     SIZE_T ResultSz;
  } buf{ in, sizeof(*in), out, sizeof(*out), payload, sizeof(*payload),
         result, sizeof(*result) };

  return pQSII((SYSTEM_INFORMATION_CLASS)0x2D, &buf, sizeof(buf), nullptr);
}

class FactoryServer {
public:
  // Создаёт фабрику и shared-секцию заданного размера. Идемпотентно не сделано:
  // вызывается один раз при init экспортера.
  bool Create(const char* name, UINT64 dataSize) {
    if (dataSize == 0 || dataSize > KDF_MAX_DATA) return false;
    KDF_ATTR_IN attr{};
    memcpy_s(attr.Name, sizeof(attr.Name), KDF_MAGIC, sizeof(KDF_MAGIC));
    attr.ValueSize = dataSize;
    attr.MaxObjectCount = 1;

    KDF_ATTR_OUT out{};
    KDF_PAYLOAD p1{}; p1.Code = KDF_OP_EXCHANGE; p1.SubOp = 1;
    p1.Open.FactoryHandle = nullptr;
    strncpy_s(p1.Open.Name, sizeof(p1.Open.Name), name, _TRUNCATE);
    p1.Open.c = 0; p1.Open.d = 1;
    KDF_RESULT r1{};
    if (Exchange(&attr, &out, &p1, &r1) != 0 || r1.Status != 0) return false;

    m_Factory = out.FactoryHandle;
    m_Section = out.SectionHandle;
    m_DataSize = out.DataSize ? out.DataSize : dataSize;

    KDF_PAYLOAD p2{}; p2.Code = KDF_OP_EXCHANGE; p2.SubOp = 2;
    p2.Query = { m_Factory, nullptr, 1, 1, 1, 1, 0 };
    KDF_RESULT r2{};
    if (Exchange(&attr, &out, &p2, &r2) != 0 || r2.Status != 0) return false;

    m_Map = MapViewOfFile(m_Section, FILE_MAP_WRITE, 0, 0, m_DataSize);
    if (!m_Map) return false;
    memset(m_Map, 0, m_DataSize);
    return true;
  }

  void* Data() const { return m_Map; }
  UINT64 Size() const { return m_DataSize; }

  void Destroy() {
    if (m_Map) { UnmapViewOfFile(m_Map); m_Map = nullptr; }
    KDF_ATTR_IN attr{};
    memcpy_s(attr.Name, sizeof(attr.Name), KDF_MAGIC, sizeof(KDF_MAGIC));
    KDF_ATTR_OUT out{};
    KDF_PAYLOAD p{}; p.Code = KDF_OP_EXCHANGE; p.SubOp = 3;
    p.Close.a = (UINT64)m_Factory; p.Close.b = (UINT64)m_Section;
    KDF_RESULT r{};
    Exchange(&attr, &out, &p, &r);
    m_Factory = nullptr; m_Section = nullptr;
  }

private:
  HANDLE m_Factory = nullptr;
  HANDLE m_Section = nullptr;
  UINT64 m_DataSize = 0;
  void*  m_Map = nullptr;
};

} // namespace kdf
