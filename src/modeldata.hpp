/*
    Internal data used by studiomdl which is then transferred/mapped to the real .mdl data
*/

#pragma once

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
    float m[3][4];
    float im[3][4];
    float length;
};

struct BoneTable
{
    char name[MAXBONENAMELENGTH]; // bone name for symbolic links
    int parent;                   // parent bone
    int bonecontroller;           // -1 == 0
    int flags;                    // X, Y, Z, XR, YR, ZR
    // short		value[6];	// default DoF values
    Vector3 pos;        // default pos
    Vector3 posscale;   // pos values scale
    Vector3 rot;        // default pos
    Vector3 rotscale;   // rotation values scale
    int group;          // hitgroup
    Vector3 bmin, bmax; // bounding box
};

struct RenameBone
{
    char from[MAXBONENAMELENGTH];
    char to[MAXBONENAMELENGTH];
};

struct HitBox
{
    char name[MAXBONENAMELENGTH]; // bone name
    int bone;
    int group; // hitgroup
    int model;
    Vector3 bmin, bmax; // bounding box
};

struct HitGroup
{
    int models;
    int group;
    char name[MAXBONENAMELENGTH]; // bone name
};

struct BoneController
{
    char name[MAXBONENAMELENGTH];
    int bone;
    int type;
    int index;
    float start;
    float end;
};

struct Attachment
{
    char name[MAXBONENAMELENGTH];
    char bonename[MAXBONENAMELENGTH];
    int index;
    int bone;
    int type;
    Vector3 org;
};

struct Node
{
    char name[MAXBONENAMELENGTH]; // before: name[64]
    int parent;
    int mirrored;
};

struct Animation
{
    char name[MAXBONENAMELENGTH]; // before: name[64]
    int startframe;
    int endframe;
    int flags;
    int numbones;
    Node node[MAXSTUDIOSRCBONES];
    int bonemap[MAXSTUDIOSRCBONES];
    int boneimap[MAXSTUDIOSRCBONES];
    Vector3 *pos[MAXSTUDIOSRCBONES];
    Vector3 *rot[MAXSTUDIOSRCBONES];
    int numanim[MAXSTUDIOSRCBONES][DEGREESOFFREEDOM];
    StudioAnimationValue *anim[MAXSTUDIOSRCBONES][DEGREESOFFREEDOM];
};

struct Event
{
    int event;
    int frame;
    char options[MAXEVENTOPTIONS];
};

struct Pivot
{
    int index;
    Vector3 org;
    int start;
    int end;
};

struct Sequence
{
    int motiontype;
    Vector3 linearmovement;

    char name[MAXSEQUENCENAMELENGTH]; // before: name[64]
    int flags;
    float fps;
    int numframes;

    int activity;
    int actweight;

    int frameoffset; // used to adjust frame numbers

    int numevents;
    Event event[MAXSTUDIOEVENTS];

    int numpivots;
    Pivot pivot[MAXSTUDIOPIVOTS];

    int numblends;
    Animation *panim[MAXSTUDIOGROUPS];
    float blendtype[MAXSEQUENCEBLEND];
    float blendstart[MAXSEQUENCEBLEND];
    float blendend[MAXSEQUENCEBLEND];

    Vector3 automovepos[MAXSTUDIOANIMATIONS];
    Vector3 automoveangle[MAXSTUDIOANIMATIONS];

    int seqgroup;
    int animindex;

    Vector3 bmin;
    Vector3 bmax;
    int entrynode;
    int exitnode;
    int nodeflags;
};

struct SequenceGroup
{
    char label[32];
    char name[64];
};

struct RGB
{
    std::uint8_t r, g, b;
};

// FIXME: what about texture overrides inline with loading models
struct Texture
{
    char name[64];
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
    float fskintop;
    float fskinleft;
    float fskinwidth;
    float fskinheight;
    int size;
    void *pdata;
    int parent;
};

struct Mesh
{
    int alloctris;
    int numtris;
    TriangleVert (*triangle)[3];

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
    char name[64];

    int numbones;
    Node node[MAXSTUDIOSRCBONES];
    Bone skeleton[MAXSTUDIOSRCBONES];
    int boneref[MAXSTUDIOSRCBONES];  // is local bone (or child) referenced with a vertex
    int bonemap[MAXSTUDIOSRCBONES];  // local bone to world bone mapping
    int boneimap[MAXSTUDIOSRCBONES]; // world bone to local bone mapping

    Vector3 boundingbox[MAXSTUDIOSRCBONES][2];

    Mesh *trimesh[MAXSTUDIOTRIANGLES];
    int trimap[MAXSTUDIOTRIANGLES];

    int numverts;
    Vertex vert[MAXSTUDIOVERTS];

    int numnorms;
    Normal normal[MAXSTUDIOVERTS];

    int nummesh;
    Mesh *pmesh[MAXSTUDIOMESHES];

    float boundingradius;

    int numframes;
    float interval;
    struct Model *next;
};

struct BodyPart
{
    char name[32];
    int nummodels;
    int base;
    Model *pmodel[MAXSTUDIOMODELS];
};