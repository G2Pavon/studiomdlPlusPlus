#pragma once

#include <filesystem>
#include <cstring>
#include <array>
#include <algorithm>

#include "mathlib.hpp"
#include "modeldata.hpp"
#include "studio.hpp"

class QC
{
public:
    char modelname[1024];                                  // $modelname
    std::filesystem::path cd;                              // $cd
    std::filesystem::path cdAbsolute;                      // $cd
    std::filesystem::path cdtexture;                       // $cdtexture
    Vector3 eyeposition{};                                 // $eyeposition
    float scale = 1.0f;                                    // $scale
    float scaleBodyAndSequenceOption = 1.0f;               // $body studio <value> // also for $sequence
    Vector3 origin{};                                      // $origin
    float originRotation = to_radians(ENGINE_ORIENTATION); // $origin <X> <Y> <Z> <rotation>
    float rotate = 0.0f;                                   // $rotate and $sequence <sequence name> <SMD path> {[rotate <zrotation>]} only z axis
    Vector3 sequenceOrigin{};                              // $sequence <sequence name> <SMD path> {[origin <X> <Y> <Z>]}
    float gamma = 1.8f;                                    // $$gamma

    std::array<RenameBone, MAXSTUDIOSRCBONES> renamebone{}; // $renamebone
    int renamebonecount = 0;

    std::array<HitGroup, MAXSTUDIOSRCBONES> hitgroup{}; // $hgroup
    int hitgroupscount = 0;

    std::array<std::array<char, 64>, MAXSTUDIOSRCBONES> mirrorbone{}; // $mirrorbone
    int mirroredcount = 0;

    std::array<Animation *, MAXSTUDIOSEQUENCES * MAXSTUDIOBLENDS> animationSequenceOption{}; // $sequence, each sequence can have 16 blends
    int animationcount = 0;

    std::array<std::array<std::array<int, 32>, 32>, 32> texturegroup{}; // $texturegroup
    int texturegroupCount = 0;                                          // unnecessary? since engine doesn't support multiple texturegroups
    std::array<int, 32> texturegrouplayers{};
    std::array<int, 32> texturegroupreps{};

    std::array<Vector3, 2> bbox{}; // $bbox
    std::array<Vector3, 2> cbox{}; // $cbox

    std::array<HitBox, MAXSTUDIOSRCBONES> hitbox{}; // $hbox
    int hitboxescount = 0;

    std::array<BoneController, MAXSTUDIOSRCBONES> bonecontroller{}; // $$controller
    int bonecontrollerscount = 0;

    std::array<Attachment, MAXSTUDIOSRCBONES> attachment{}; // $attachment
    int attachmentscount = 0;

    std::array<Sequence, MAXSTUDIOSEQUENCES> sequence{}; // $sequence
    int sequencecount = 0;

    std::array<SequenceGroup, MAXSTUDIOSEQUENCES> sequencegroup{}; // $sequencegroup
    int sequencegroupcount = 0;

    std::array<Model *, MAXSTUDIOMODELS> submodel{}; // $body
    int submodelscount = 0;

    std::array<BodyPart, MAXSTUDIOBODYPARTS> bodypart{}; // $bodygroup
    int bodygroupcount = 0;

    int flags = 0; // $flags

    // Constructor
    QC()
    {
        // Initialize arrays with default values
        renamebone.fill(RenameBone{});
        hitgroup.fill(HitGroup{});
        mirrorbone.fill({});
        animationSequenceOption.fill(nullptr);
        texturegroup.fill({});
        texturegrouplayers.fill(0);
        texturegroupreps.fill(0);
        bbox.fill(Vector3{});
        cbox.fill(Vector3{});
        hitbox.fill(HitBox{});
        bonecontroller.fill(BoneController{});
        attachment.fill(Attachment{});
        sequence.fill(Sequence{});
        sequencegroup.fill(SequenceGroup{});
        submodel.fill(nullptr);
        bodypart.fill(BodyPart{});
    }
};
