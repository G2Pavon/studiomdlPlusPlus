#pragma once
#include <cstdint>
#include "cmdlib.hpp"

#define ALIGN(a) (((uintptr_t)(a) + 3) & ~(uintptr_t)3)

void WriteBoneInfo();
void WriteSequenceInfo();
std::uint8_t *WriteAnimations(std::uint8_t *pData, const std::uint8_t *pStart, int group);
void WriteTextures();
void WriteModel();

void WriteFile(void);