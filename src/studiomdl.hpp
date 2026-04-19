#pragma once

#include "format/mdl.hpp"
#include "format/qc.hpp"
#include "modeldata.hpp"

// Common studiomdl and writemdl variables -----------------
extern std::array<std::array<int, 100>, 100> g_xnode;
extern int g_numxnodes; // Not initialized??
extern std::vector<BoneTable> g_bonetable;
extern std::vector<Texture> g_textures;
extern std::array<std::array<int, MAXSTUDIOSKINS>, 256> g_skinref; // [skin][skinref], returns texture index
extern int g_skinrefcount;
extern int g_skinfamiliescount;