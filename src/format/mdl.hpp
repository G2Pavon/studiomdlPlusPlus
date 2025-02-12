#pragma once

#include <cstdint>

#include "utils/mathlib.hpp"

// Half-Life 1 MDL v10 Constants and Structures

// --- Magic Numbers and Version ---
constexpr int STUDIO_VERSION = 10;
constexpr int IDSTUDIOHEADER = (('T' << 24) + ('S' << 16) + ('D' << 8) + 'I');	  // little-endian "IDST"
constexpr int IDSTUDIOSEQHEADER = (('Q' << 24) + ('S' << 16) + ('D' << 8) + 'I'); // little-endian "IDSQ"

// --- Model Limits ---
constexpr int MAXSTUDIOTRIANGLES = 20000; // max triangles per model
constexpr int MAXSTUDIOVERTS = 2048;	  // max vertices per submodel
constexpr int MAXSTUDIOSEQUENCES = 2048;  // total animation sequences
constexpr int MAXSTUDIOSKINS = 100;		  // total textures
constexpr int MAXSTUDIOSRCBONES = 512;	  // bones allowed at source movement
constexpr int MAXSTUDIOBONES = 128;		  // total bones actually used
constexpr int MAXSTUDIOMODELS = 32;		  // sub-models per model
constexpr int MAXSTUDIOBODYPARTS = 32;	  // body parts per submodel
constexpr int MAXSTUDIOGROUPS = 16;		  // sequence groups (e.g. barney01.mdl, barney02.mdl, etc)
constexpr int MAXSTUDIOANIMATIONS = 2048; // max frames per sequence
constexpr int MAXSTUDIOMESHES = 256;	  // max textures per model
constexpr int MAXSTUDIOEVENTS = 1024;	  // events per model
constexpr int MAXSTUDIOBLENDS = 16;		  // max anim blends
constexpr int MAXSTUDIOCONTROLLERS = 8;	  // max controllers per model
constexpr int MAXSTUDIOATTACHMENTS = 4;	  // max attachments per model

// --- String Length Limits ---
// Maximum lengths for various name strings within the MDL file
constexpr int MAXEVENTOPTIONS = 64;
constexpr int MAXTEXTURENAMELENGTH = 64;
constexpr int MAXMODELNAMELENGTH = 64;
constexpr int MAXBODYPARTSNAMELENGHT = 64;
constexpr int MAXBONENAMELENGTH = 32;
constexpr int MAXSEQUENCENAMELENGTH = 32;

// --- Animation and Blending ---
constexpr int DEGREESOFFREEDOM = 6; // X, Y, Z, rotX, rotY, rotZ - for bone transformations
constexpr int MAXSEQUENCEBLEND = 2; // maximum number of blends per sequence

// other
constexpr float ENGINE_ORIENTATION = 90.0f; // Z rotation to match with engine forward direction (x axis)

// --- Structure Definitions --- //

/**
 *  Main header structure for Half-Life MDL files. aka studiohdr_t
 *
 * Contains global information about the model, as well as offsets to various data chunks
 */
struct StudioHeader
{
	int ident;	 // magic number IDSTUDIOHEADER = "IDST"
	int version; // version number STUDIO_VERSION = 10

	char name[64]; // model's name
	int length;	   // file size in bytes

	Vector3 eyeposition; // model's eye position (in model space)
	Vector3 min;		 // model's hull min extent
	Vector3 max;		 // model's hull max extent

	Vector3 bbmin; // clipping bounding box min extent
	Vector3 bbmax; // clipping bounding box max extent

	int flags; // model flags

	int numbones;  // number of bones.
	int boneindex; // offset to the first bone

	int numbonecontrollers;
	int bonecontrollerindex; // offset to the first bone controller

	int numhitboxes;
	int hitboxindex; // offset to the first hitbox

	int numseq;	  // number of animation sequences
	int seqindex; // offset to the first sequence description

	int numseqgroups;
	int seqgroupindex; // offset to the first sequence group

	int numtextures;
	int textureindex;	  // offset to the first texture
	int texturedataindex; // offset to the texture pixel data.

	int numskinref; // number of replaceable textures
	int numskinfamilies;
	int skinindex; // offset to the skin data (a matrix of texture indices)

	int numbodyparts;
	int bodypartindex; // offset to the first bodypart

	int numattachments;
	int attachmentindex; // offset to the first attachment

	int soundtable;		 // Unused
	int soundindex;		 // Unused
	int soundgroups;	 // Unused
	int soundgroupindex; // Unused

	int numtransitions;
	int transitionindex; // offset to the sequence transition graph
};

/**
 *  Header for sequence group files. aka studioseqhdr_t
 *
 * Sequence group files can also be loaded using the regular StudioHeader
 */
struct StudioSequenceGroupHeader
{
	int id;		 // magic number = IDSQ
	int version; // version number = 10

	char name[64]; // model's name.
	int length;	   // file size in bytes
};

/**
 *  aka mstudiobone_t
 *
 * Represents a bone in the skeleton
 */
struct StudioBone
{
	char name[MAXBONENAMELENGTH];
	int parent;							  // index of the parent bone (-1 for root)
	int flags = 0;						  // Unused.
	int bonecontroller[DEGREESOFFREEDOM]; // Bone controller indices for each degree of freedom (X, Y, Z, rotX, rotY, rotZ). -1 indicates no controller
	float value[DEGREESOFFREEDOM];		  // Default values for each degree of freedom
	float scale[DEGREESOFFREEDOM];		  // Scale values used for compressed animation data
};

/**
 *  aka mstudiobonecontroller_t
 *
 * Used to modify bone positions/rotations at runtime
 */
struct StudioBoneController
{
	int bone;	 // index of the bone this controller affects
	int type;	 // motion type (X, Y, Z, XR, YR, ZR, possibly OR'd with STUDIO_RLOOP).
	float start; // minimum value for the controller
	float end;	 // maximum value for the controller
	int rest;	 // controller's value at the rest pose
	int index;	 // controller's channel/index
};

/**
 *  aka mstudiobbox_t
 *
 * Defines a collision hitbox associated with a bone
 */
struct StudioHitbox
{
	int bone;	   // index of the bone this hitbox is attached to
	int group;	   // hit group
	Vector3 bbmin; // minimum extent of the hitbox
	Vector3 bbmax; // maximum extent of the hitbox
};

/**
 *  aka mstudioseqgroup_t
 *
 * Used for demand-loaded animations stored in separate files.
 */
struct StudioSequenceGroup
{
	char label[32]; // sequence group's name
	char name[64];	// filename of the sequence group (e.g., "barney01.mdl").
	int unused1;	// Unused
	int unused2;	// Unused
};

/**
 *  mstudioseqdesc_t
 *
 * Contains information about an animation sequence
 */
struct StudioSequenceDescription
{
	char label[MAXSEQUENCENAMELENGTH]; // sequence's name

	float fps;
	int flags; // sequence flags (e.g., STUDIO_LOOPING)

	int activity;  // activity ID
	int actweight; // activity's weight (used to choose among sequences with the same activity)

	int numevents;
	int eventindex; // offset to the first animation event

	int numframes;

	int numpivots;	// Unused in Goldsrc
	int pivotindex; // Unused in Goldsrc

	int motiontype;
	int motionbone;			// index of the motion bone
	Vector3 linearmovement; // linear movement of the motion bone from the first to the last frame
	int automoveposindex;	// Unused in Goldsrc
	int automoveangleindex; // Unused in Goldsrc

	Vector3 bbmin; // bounding box min extent for this sequence
	Vector3 bbmax; // bounding box max extent for this sequence

	int numblends; // number of blend controllers (either 1 or 2)
	int animindex; // offset to the animation frame offset data

	int blendtype[MAXSEQUENCEBLEND];	// motion types for the blend controllers (X, Y, Z, XR, YR, ZR).
	float blendstart[MAXSEQUENCEBLEND]; // starting blend values
	float blendend[MAXSEQUENCEBLEND];	// ending blend values
	int blendparent;					// Unused in Goldsrc

	int seqgroup; // index of the sequence group this sequence belongs to

	int entrynode; // entry node in the sequence transition graph
	int exitnode;  // exit node in the sequence transition graph
	int nodeflags; // node flags (0 or 1, indicating unidirectional or bidirectional transitions)
	int nextseq;   // Unused in Goldsrc
};

/**
 *  aka mstudioevent_t
 *
 * Represents an event that occurs at a specific frame in a sequence in the game at run time
 */
struct StudioAnimationEvent
{
	int frame;					   // frame at which the event occurs
	int event;					   // event ID
	int type;					   // Unused
	char options[MAXEVENTOPTIONS]; // Additional event data
};

/**
 *  aka mstudioattachment_t
 *
 * Defines an attachment point on a bone
 */
struct StudioAttachment
{
	char name[32];		// Unused in Goldsrc
	int type;			// Unused in Goldsrc
	int bone;			// index of the bone this attachment is connected to
	Vector3 org;		// attachment point's position relative to the bone's origin
	Vector3 vectors[3]; // Unused in Goldsrc
};

/**
 *  aka mstudioanim_t
 *
 * Contains offsets to the compressed animation data for a bone in a sequence
 */
struct StudioAnimationFrameOffset
{
	unsigned short offset[DEGREESOFFREEDOM]; // Offsets to the animation frame data for each degree of freedom. 0 means no data.
};

/**
 *  aka mstudioanimvalue_t
 *
 * Union representing a single compressed animation value (either position or rotation component)
 */
union StudioAnimationValue
{
	struct
	{
		std::uint8_t valid; // Number of valid anim data
		std::uint8_t total; // Total number of anim data
	} num;
	short value; // actual animation value
};

/**
 *  aka mstudiobodyparts_t
 *
 * Groups together multiple models that can be switched at runtime
 */
struct StudioBodyPart
{
	char name[MAXBODYPARTSNAMELENGHT];
	int nummodels;
	int base;		// Used to calculate the model index relative to the bodypart
	int modelindex; // offset to the first model in this bodypart
};

/**
 *  aka mstudiotexture_t
 *
 * Contains information about a texture used by the model
 */
struct StudioTexture
{
	char name[MAXTEXTURENAMELENGTH];
	int flags; // Texture flags (e.g., STUDIO_NF_CHROME).
	int width;
	int height;
	int index; // offset to the texture's data
};

/**
 *  aka mstudiomodel_t
 *
 * Represents a single model within a bodypart
 */
struct StudioModel
{
	char name[MAXMODELNAMELENGTH]; // model's name (which is usually the name of the SMD file it was created from)

	int type; // Unused

	float boundingradius; // Unused

	int nummesh;
	int meshindex; // offset to the first mesh in this model

	int numverts;
	int vertinfoindex; // offset to an array of bone indices, one for each vertex, indicating which bone influences that vertex
	int vertindex;	   // offset to the vertex position data (Vector3 array)
	int numnorms;
	int norminfoindex; // offset to an array of bone indices, one for each normal, indicating which bone influences that normal
	int normindex;	   // offset to the normal data (Vector3 array)

	int numgroups;	// Unused
	int groupindex; // Unused
};

/**
 *  aka mstudiomesh_t
 *
 * Represents a mesh (a collection of triangles) within a model
 */
struct StudioMesh
{
	int numtris;
	int triindex;  // offset to the trivert data
	int skinref;   // index of the texture to use for this mesh
	int numnorms;  // Unused
	int normindex; // Unused
};

// --- Lighting and Motion Flags ---
// These flags are used to specify various rendering and animation options

// Lighting options (used in texture flags)
#define STUDIO_NF_FLATSHADE 0x0001
#define STUDIO_NF_CHROME 0x0002
#define STUDIO_NF_FULLBRIGHT 0x0004
#define STUDIO_NF_NOMIPS 0x0008 // Not used
#define STUDIO_NF_ALPHA 0x0010	// Not used
#define STUDIO_NF_ADDITIVE 0x0020
#define STUDIO_NF_MASKED 0x0040

// Motion flags (used in bone controllers and animation data)
#define STUDIO_X 0x0001	  // X-axis movement
#define STUDIO_Y 0x0002	  // Y-axis movement
#define STUDIO_Z 0x0004	  // Z-axis movement
#define STUDIO_XR 0x0008  // X-axis rotation
#define STUDIO_YR 0x0010  // Y-axis rotation
#define STUDIO_ZR 0x0020  // Z-axis rotation
#define STUDIO_LX 0x0040  // Unused
#define STUDIO_LY 0x0080  // Unused
#define STUDIO_LZ 0x0100  // Unused
#define STUDIO_AX 0x0200  // Unused
#define STUDIO_AY 0x0400  // Unused
#define STUDIO_AZ 0x0800  // Unused
#define STUDIO_AXR 0x1000 // Unused
#define STUDIO_AYR 0x2000 // Unused
#define STUDIO_AZR 0x4000 // Unused
#define STUDIO_TYPES 0x7FFF
#define STUDIO_RLOOP 0x8000 // Indicates that the controller wraps around (e.g., for angles)

// Sequence flags
#define STUDIO_LOOPING 0x0001 // Indicates that the sequence loops