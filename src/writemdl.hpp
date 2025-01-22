#pragma once
#include <cstdint>
#include "cmdlib.hpp"

#define ALIGN(a) (((uintptr_t)(a) + 3) & ~(uintptr_t)3)

void write_bone_info();
void write_sequence_info();
std::uint8_t *write_animations(std::uint8_t *pData, const std::uint8_t *pStart, int group);
void write_textures();
void write_model();

void write_file(void);