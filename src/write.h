#include <cstdint>

#include "cmdlib.h"

#define ALIGN(a) (((uintptr_t)(a) + 3) & ~(uintptr_t)3)

void WriteBoneInfo();
void WriteSequenceInfo();
byte *WriteAnimations(byte *pData, const byte *pStart, int group);
void WriteTextures();
void WriteModel();

void WriteFile(void);