#pragma once

#include "modeldata.hpp"

void find_neighbor(int starttri, int startv);
int strip_length(int starttri, int startv);
int fan_length(int starttri, int startv);

int build_tris(TriangleVert (*x)[3], Mesh *y, std::uint8_t **ppdata);