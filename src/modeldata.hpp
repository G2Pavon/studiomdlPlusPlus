/*
    Internal data used by studiomdl which is then transferred/mapped to the real .mdl data
*/

#pragma once

#include <string>
#include <vector>

#include "utils/mathlib.hpp"
#include "format/mdl.hpp"

struct TriangleVert
{
    int vertindex;
    int normindex; // index into normal array
    int s, t;
    float u, v;
};

struct Vertex
{
    int bone;    // bone transformation index
    Vector3 org; // original position
};

struct Normal
{
    int skinref;
    int bone;    // bone transformation index
    Vector3 org; // original position
};

struct BoneFixUp
{
    Vector3 worldorg;
    float matrix[3][4];
    float inv_matrix[3][4];
};

struct BoneTable
{
    std::string name; // bone name for symbolic links. TODO: Check name length limits
    int parent;                   // parent bone
    Vector3 pos;        // default pos
    Vector3 posscale;   // pos values scale
    Vector3 rot;        // default pos
    Vector3 rotscale;   // rotation values scale
    int group;          // hitgroup
    Vector3 bmin, bmax; // bounding box
};

struct RenameBone
{
    std::string from;
    std::string to;
};

struct HitBox
{
    std::string name; // bone name
    int bone;
    int group; // hitgroup
    Vector3 bmin, bmax; // bounding box
};

struct HitGroup
{
    int group;
    std::string name; // bone name
};

struct BoneController
{
    std::string name;
    int bone;
    int type;
    int index;
    float start;
    float end;
};

struct Attachment
{
    std::string bonename;
    int bone;
    Vector3 org;
};

struct Node
{
    std::string name; // before: name[64]
    int parent;
    int mirrored;
};

struct Animation
{
    std::string name; // before: name[64]
    int startframe;
    int endframe;
    std::vector<Node> nodes;
    int boneimap[MAXSTUDIOSRCBONES];
    Vector3 *pos[MAXSTUDIOSRCBONES];
    Vector3 *rot[MAXSTUDIOSRCBONES];
    int numanim[MAXSTUDIOSRCBONES][DEGREESOFFREEDOM];
    StudioAnimationValue *anims[MAXSTUDIOSRCBONES][DEGREESOFFREEDOM];
};

struct Event
{
    int event;
    int frame;
    std::string options;
};

struct Sequence
{
    int motiontype;
    Vector3 linearmovement;

    std::string name; // before: name[64]
    int flags;
    float fps;
    int numframes;

    int activity;
    int actweight;

    int frameoffset; // used to adjust frame numbers

    std::vector<Event> events;

    std::vector<Animation> anims;
    float blendtype[MAXSEQUENCEBLEND];
    float blendstart[MAXSEQUENCEBLEND];
    float blendend[MAXSEQUENCEBLEND];

    int seqgroup;
    int animindex;

    Vector3 bmin;
    Vector3 bmax;
    int entrynode;
    int exitnode;
    int nodeflags;
};

struct RGB
{
    std::uint8_t r, g, b;
};

// FIXME: what about texture overrides inline with loading models
struct Texture
{
    std::string name;
    int flags;
    int srcwidth;
    int srcheight;
    std::uint8_t *ppicture;
    RGB *ppal;
    float max_s;
    float min_s;
    float max_t;
    float min_t;
    int skintop;
    int skinleft;
    int skinwidth;
    int skinheight;
    int size;
    void *pdata;
    int parent;
};

struct Mesh
{
    int alloctris;
    int numtris;
    TriangleVert (*triangles)[3];

    int skinref;
    int numnorms;
};

struct Bone
{
    Vector3 pos;
    Vector3 rot;
};

struct Model
{
    std::string name;

    std::vector<Node> nodes;
    std::vector<Bone> skeleton;
    int boneref[MAXSTUDIOSRCBONES];  // is local bone (or child) referenced with a vertex
    int bonemap[MAXSTUDIOSRCBONES];  // local bone to world bone mapping
    int boneimap[MAXSTUDIOSRCBONES]; // world bone to local bone mapping

    std::vector<Vertex> verts;

    std::vector<Normal> normals;

    int nummesh;
    Mesh *pmeshes[MAXSTUDIOMESHES];
};

struct BodyPart
{
    std::string name;
    int num_submodels;
    int base;
};