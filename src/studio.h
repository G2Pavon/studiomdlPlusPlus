#pragma once
#include <cstdint>

// header
constexpr int STUDIO_VERSION = 10;
constexpr int IDSTUDIOHEADER = (('T' << 24) + ('S' << 16) + ('D' << 8) + 'I');	  // little-endian "IDST"
constexpr int IDSTUDIOSEQHEADER = (('Q' << 24) + ('S' << 16) + ('D' << 8) + 'I'); // little-endian "IDSQ"

constexpr int MAXSTUDIOTRIANGLES = 20000; // max triangles per model
constexpr int MAXSTUDIOVERTS = 2048;	  // max vertices per submodel
constexpr int MAXSTUDIOSEQUENCES = 2048;  // total animation sequences
constexpr int MAXSTUDIOSKINS = 100;		  // total textures
constexpr int MAXSTUDIOSRCBONES = 512;	  // bones allowed at source movement
constexpr int MAXSTUDIOBONES = 128;		  // total bones actually used
constexpr int MAXSTUDIOMODELS = 32;		  // sub-models per model
constexpr int MAXSTUDIOBODYPARTS = 32;	  // body parts per submodel
constexpr int MAXSTUDIOGROUPS = 16;		  // sequence groups (e.g. barney01.mdl, barney02.mdl, e.t.c)
constexpr int MAXSTUDIOANIMATIONS = 2048; // max frames per sequence
constexpr int MAXSTUDIOMESHES = 256;	  // max textures per model
constexpr int MAXSTUDIOEVENTS = 1024;	  // events per model
constexpr int MAXSTUDIOPIVOTS = 256;	  // pivot points
constexpr int MAXSTUDIOBLENDS = 16;		  // max anim blends
constexpr int MAXSTUDIOCONTROLLERS = 8;	  // max controllers per model
constexpr int MAXSTUDIOATTACHMENTS = 4;	  // max attachments per model
constexpr int MAXSTUDIOTEXTUREGROUP = 0;  // actually 1 but 0 cus is the first index of an texturegroup array, since engine doesn't support multiple texturegroup

constexpr int MAXEVENTOPTIONS = 64;
constexpr int MAXTEXTURENAMELENGTH = 64;
constexpr int MAXMODELNAMELENGTH = 64;
constexpr int MAXBODYPARTSNAMELENGHT = 64;
constexpr int MAXBONENAMELENGTH = 32;
constexpr int MAXSEQUENCENAMELENGTH = 32;
constexpr int DEGREESOFFREEDOM = 6; // X, Y, Z, rotX, rotY, rotZ
constexpr int MAXSEQUENCEBLEND = 2;

struct studiohdr_t
{
	int ident;
	int version;

	char name[64];
	int length;

	vec3_t eyeposition; // ideal eye position
	vec3_t min;			// ideal movement hull size
	vec3_t max;

	vec3_t bbmin; // clipping bounding box
	vec3_t bbmax;

	int flags;

	int numbones; // bones
	int boneindex;

	int numbonecontrollers; // bone controllers
	int bonecontrollerindex;

	int numhitboxes; // complex bounding boxes
	int hitboxindex;

	int numseq; // animation sequences
	int seqindex;

	int numseqgroups; // demand loaded sequences
	int seqgroupindex;

	int numtextures; // raw textures
	int textureindex;
	int texturedataindex;

	int numskinref; // replaceable textures
	int numskinfamilies;
	int skinindex;

	int numbodyparts;
	int bodypartindex;

	int numattachments; // queryable attachable points
	int attachmentindex;

	int soundtable;		 // unused
	int soundindex;		 // unused
	int soundgroups;	 // unused
	int soundgroupindex; // unused

	int numtransitions; // animation node to animation node transition graph
	int transitionindex;
};

struct studioseqhdr_t
{
	int id;
	int version;

	char name[64];
	int length;
};

struct mstudiobone_t
{
	char name[MAXBONENAMELENGTH];		  // bone name for symbolic links
	int parent;							  // parent bone
	int flags;							  // ??
	int bonecontroller[DEGREESOFFREEDOM]; // bone controller index, -1 == none
	float value[DEGREESOFFREEDOM];		  // default DoF values
	float scale[DEGREESOFFREEDOM];		  // scale for delta DoF values
};

struct mstudiobonecontroller_t
{
	int bone; // -1 == 0
	int type; // X, Y, Z, XR, YR, ZR, M
	float start;
	float end;
	int rest;  // byte index value at rest
	int index; // 0-3 user set controller, 4 mouth
};

struct mstudiobbox_t
{
	int bone;
	int group;	  // intersection group
	vec3_t bbmin; // bounding box
	vec3_t bbmax;
};

struct mstudioseqgroup_t
{
	char label[32]; // textual name
	char name[64];	// file name
	int unused1;
	int unused2; // was "data" -  hack for group 0
};

struct mstudioseqdesc_t
{
	char label[MAXSEQUENCENAMELENGTH]; // sequence label

	float fps; // frames per second
	int flags; // looping/non-looping flags

	int activity;
	int actweight;

	int numevents;
	int eventindex;

	int numframes; // number of frames per sequence

	int numpivots; // number of foot pivots
	int pivotindex;

	int motiontype;
	int motionbone;
	vec3_t linearmovement;
	int automoveposindex;
	int automoveangleindex;

	vec3_t bbmin; // per sequence bounding box
	vec3_t bbmax;

	int numblends;
	int animindex; // mstudioanim_t pointer relative to start of sequence group data
				   // [blend][bone][X, Y, Z, XR, YR, ZR]

	int blendtype[MAXSEQUENCEBLEND];	// X, Y, Z, XR, YR, ZR
	float blendstart[MAXSEQUENCEBLEND]; // starting value
	float blendend[MAXSEQUENCEBLEND];	// ending value
	int blendparent;

	int seqgroup; // sequence group for demand loading

	int entrynode; // transition node at entry
	int exitnode;  // transition node at exit
	int nodeflags; // transition rules

	int nextseq; // auto advancing sequences
};

struct mstudioevent_t
{
	int frame;
	int event;
	int type;
	char options[MAXEVENTOPTIONS];
};

struct mstudiopivot_t
{
	vec3_t org; // pivot point
	int start;
	int end;
};

struct mstudioattachment_t
{
	char name[32];	   // unused in goldsrc
	int type;		   // unused in goldsrc
	int bone;		   // index of the bone this is attached
	vec3_t org;		   // attachment point, offset from bone origin
	vec3_t vectors[3]; // unused in goldsrc
};

struct mstudioanim_t
{
	unsigned short offset[DEGREESOFFREEDOM];
};

union mstudioanimvalue_t
{
	struct
	{
		byte valid;
		byte total;
	} num;
	short value;
};

struct mstudiobodyparts_t
{
	char name[MAXBODYPARTSNAMELENGHT];
	int nummodels;
	int base;
	int modelindex; // index into models array
};

struct mstudiotexture_t
{
	char name[MAXTEXTURENAMELENGTH];
	int flags;
	int width;
	int height;
	int index;
};

struct mstudiomodel_t
{
	char name[MAXMODELNAMELENGTH];

	int type;

	float boundingradius;

	int nummesh;
	int meshindex;

	int numverts;	   // number of unique vertices
	int vertinfoindex; // vertex bone info
	int vertindex;	   // vertex vec3_t
	int numnorms;	   // number of unique surface normals
	int norminfoindex; // normal bone info
	int normindex;	   // normal vec3_t

	int numgroups; // deformation groups
	int groupindex;
};

struct mstudiomesh_t
{
	int numtris;
	int triindex;
	int skinref;
	int numnorms;  // per mesh normals
	int normindex; // normal vec3_t
};

// lighting options
#define STUDIO_NF_FLATSHADE 0x0001
#define STUDIO_NF_CHROME 0x0002
#define STUDIO_NF_FULLBRIGHT 0x0004
// #define STUDIO_NF_NOMIPS 0x0008
// #define STUDIO_NF_ALPHA 0x0010
#define STUDIO_NF_ADDITIVE 0x0020
#define STUDIO_NF_MASKED 0x0040

// motion flags
#define STUDIO_X 0x0001
#define STUDIO_Y 0x0002
#define STUDIO_Z 0x0004
#define STUDIO_XR 0x0008
#define STUDIO_YR 0x0010
#define STUDIO_ZR 0x0020
#define STUDIO_LX 0x0040
#define STUDIO_LY 0x0080
#define STUDIO_LZ 0x0100
#define STUDIO_AX 0x0200
#define STUDIO_AY 0x0400
#define STUDIO_AZ 0x0800
#define STUDIO_AXR 0x1000
#define STUDIO_AYR 0x2000
#define STUDIO_AZR 0x4000
#define STUDIO_TYPES 0x7FFF
#define STUDIO_RLOOP 0x8000 // controller that wraps shortest distance

// sequence flags
#define STUDIO_LOOPING 0x0001