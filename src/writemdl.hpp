#pragma once
#include <cstdint>
#include "cmdlib.hpp"
#include "format/qc.hpp"

#define ALIGN(a) (((uintptr_t)(a) + 3) & ~(uintptr_t)3)

void write_bone_info(QC &qc_cmd);
void write_sequence_info(QC &qc_cmd);
std::uint8_t *write_animations(QC &qc_cmd, std::uint8_t *pData, const std::uint8_t *pStart, int group);
void write_textures(QC &qc_cmd);
void write_model();

void write_file(QC &qc_cmd);