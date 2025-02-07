#pragma once

#include <string>
#include <filesystem>
#include <array>
#include <vector>

#include "utils/cmdlib.hpp"
#include "utils/mathlib.hpp"
#include "modeldata.hpp"
#include "format/mdl.hpp"

class QC
{
public:
    std::string modelname;                                  // $modelname
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

    std::vector<RenameBone> renamebones; // $renamebone
    std::vector<HitGroup> hitgroups; // $hgroup
    std::vector<std::string> mirroredbones; // $mirrorbone
    std::vector<Animation> sequenceAnimationOptions; // $sequence, each sequence can have 16 blends

    std::array<std::array<int, 32>, 32> texturegroups{}; // $texturegroup
    int texturegroup_rows;
    int texturegroup_cols;

    std::array<Vector3, 2> bbox{}; // $bbox
    std::array<Vector3, 2> cbox{}; // $cbox

    std::vector<HitBox> hitboxes; // $hbox
    std::vector<BoneController> bonecontrollers; // $controller
    std::vector<Attachment> attachments; // $attachment

    std::vector<Sequence> sequences; // $sequence

    std::vector<Model*> submodels; // $body

    std::vector<BodyPart> bodyparts; // $bodygroup

    int flags = 0; // $flags

    // Constructor
    QC()
    {
        // Initialize arrays with default values
        texturegroups.fill({});
        bbox.fill(Vector3{});
        cbox.fill(Vector3{});
    }
};


extern bool end_of_qc_file;
extern std::vector<char> qc_script_buffer;

void load_qc_file(const std::filesystem::path& filename);

bool get_token(bool crossline, std::string &token);
bool token_available(void);
