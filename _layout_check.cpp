// Кроссплатформенная проверка layout'ов из offsets.hpp (без WinAPI-типов).
#include <cstdint>
#include <cstddef>
typedef int32_t INT32; typedef uint64_t UINT64; typedef float FLOAT; typedef char CHAR; typedef unsigned char BYTE;
#include "offsets.hpp"
#include <cstdio>
int main() {
  printf("LocalData   = 0x%llX\n", (unsigned long long)sizeof(LocalData));
  printf("EntityData  = 0x%llX\n", (unsigned long long)sizeof(EntityData));
  printf("HB offset   = 0x%llX\n", (unsigned long long)offsetof(EntityData, HitboxPos));
  printf("Valid off   = 0x%llX\n", (unsigned long long)offsetof(EntityData, Valid));
  printf("Name off    = 0x%llX\n", (unsigned long long)offsetof(EntityData, PlayerName));
  return sizeof(EntityData) == ENTITY_RECORD_SIZE ? 0 : 1;
}
