#pragma once

#include <filesystem>
#include <cstring>

#include "mathlib.hpp"
#include "modeldata.hpp"
#include "studio.hpp"

class QC
{
public:
    std::filesystem::path cd;         // $cd
    std::filesystem::path cdAbsolute; // $cd
    std::filesystem::path cdtexture;  // $cdtexture
    float scale;                      // $scale
    float scaleBodyAndSequenceOption; // $body studio <value> // also for $sequence
    Vector3 origin;                   // $origin
    float originRotation;             // $origin <X> <Y> <Z> <rotation>
    float rotateCommand;              // $rotate and $sequence <sequence name> <SMD path> {[rotate <zrotation>]} only z axis
    Vector3 sequenceOrigin;           // $sequence <sequence name> <SMD path>  {[origin <X> <Y> <Z>]}
    float gamma;                      // $$gamma

    // Bone related settings
    RenameBone renamebone[MAXSTUDIOSRCBONES]; // $renamebone
    int renamebonecount;
    HitGroup hitgroup[MAXSTUDIOSRCBONES]; // $hgroup
    int hitgroupscount;
    char mirrorbone[MAXSTUDIOSRCBONES][64]; // $mirrorbone
    int mirroredcount;

    // Animation settings
    Animation *animationSequenceOption[MAXSTUDIOSEQUENCES * MAXSTUDIOBLENDS]; // $sequence, each sequence can have 16 blends
    int animationcount;

    // Texture settings
    int texturegroup[32][32][32]; // $texturegroup
    int texturegroupCount;        // unnecessary? since engine doesn't support multiple texturegroups
    int texturegrouplayers[32];
    int texturegroupreps[32];

    QC() : scale(1.0f),
           scaleBodyAndSequenceOption(1.0f),
           originRotation(to_radians(ENGINE_ORIENTATION)),
           rotateCommand(0.0f),
           gamma(1.8f),
           renamebonecount(0),
           hitgroupscount(0),
           mirroredcount(0),
           animationcount(0),
           texturegroupCount(0)
    {
        std::memset(renamebone, 0, sizeof(renamebone));
        std::memset(hitgroup, 0, sizeof(hitgroup));
        std::memset(mirrorbone, 0, sizeof(mirrorbone));
        std::memset(animationSequenceOption, 0, sizeof(animationSequenceOption));
        std::memset(texturegroup, 0, sizeof(texturegroup));
        std::memset(texturegrouplayers, 0, sizeof(texturegrouplayers));
        std::memset(texturegroupreps, 0, sizeof(texturegroupreps));
    }
};