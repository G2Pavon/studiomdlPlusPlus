#pragma once

#include <cstdint>

#include "format/qc.hpp"
#include "utils/cmdlib.hpp"

#define ALIGN(a) (((uintptr_t)(a) + 3) & ~(uintptr_t)3)

void write_bone_info(StudioHeader *header, QC &qc);
void write_sequence_info(StudioHeader *header, QC &qc, int &frames, float &seconds);
std::uint8_t *write_animations(QC &qc, std::uint8_t *pData, const std::uint8_t *pStart, int group);
void write_textures(StudioHeader *header);
void write_model(StudioHeader *header, QC &qc);

void write_file(std::filesystem::path path, QC &qc);