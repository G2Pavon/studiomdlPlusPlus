#pragma once

#include <cstdint>

#include "mathlib.hpp"

// Half-Life 1 MDL v10 Constants and Structures

// --- Magic Numbers and Version ---
// MDL files have a magic number "IDST" (0x49 0x44 0x53 0x54) and a version number of 10.
// Sequence group files have a magic number "IDSQ" (0x49 0x44 0x51 0x54) and the same version number.
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
constexpr int MAXSTUDIOPIVOTS = 256;	  // pivot points
constexpr int MAXSTUDIOBLENDS = 16;		  // max anim blends
constexpr int MAXSTUDIOCONTROLLERS = 8;	  // max controllers per model
constexpr int MAXSTUDIOATTACHMENTS = 4;	  // max attachments per model
constexpr int MAXSTUDIOTEXTUREGROUP = 0;  // actually 1 but 0 cus is the first index of an texturegroup array, since engine doesn't support multiple texturegroup

// --- String Length Limits ---
// Maximum lengths for various name strings within the MDL file.
constexpr int MAXEVENTOPTIONS = 64;
constexpr int MAXTEXTURENAMELENGTH = 64;
constexpr int MAXMODELNAMELENGTH = 64;
constexpr int MAXBODYPARTSNAMELENGHT = 64;
constexpr int MAXBONENAMELENGTH = 32;
constexpr int MAXSEQUENCENAMELENGTH = 32;

// --- Animation and Blending ---
constexpr int DEGREESOFFREEDOM = 6; // X, Y, Z, rotX, rotY, rotZ - Representing the 6 degrees of freedom for bone transformations.
constexpr int MAXSEQUENCEBLEND = 2; // Maximum number of blends per sequence (as described in the "Animation blending" section of the wiki).

// other
constexpr float ENGINE_ORIENTATION = 90.0f; // Z rotation to match with engine forward direction (x axis)

// --- Structure Definitions --- // [byte offset] Description

/**
 *  Main header structure for Half-Life MDL files. aka studiohdr_t
 *
 * This structure contains global information about the model, as well as offsets to various data chunks.
 */
struct StudioHeader
{
	int ident;	 // [00] The magic number, should be IDSTUDIOHEADER ("IDST").
	int version; // [04] The version number, should be STUDIO_VERSION (10).

	char name[64]; // [08] The model's name.
	int length;	   // [48] The file size in bytes.

	Vector3 eyeposition; // [52] The model's eye position (in model space).
	Vector3 min;		 // [64] The model's hull min extent.
	Vector3 max;		 // [76] The model's hull max extent.

	Vector3 bbmin; // [88] The clipping bounding box min extent.
	Vector3 bbmax; // [100] The clipping bounding box max extent.

	int flags; // [112] Model flags.

	int numbones;  // [116] The number of bones.
	int boneindex; // [120] The offset to the first bone structure.

	int numbonecontrollers;	 // [124] The number of bone controllers.
	int bonecontrollerindex; // [128] The offset to the first bone controller structure.

	int numhitboxes; // [132] The number of hitboxes.
	int hitboxindex; // [136] The offset to the first hitbox structure.

	int numseq;	  // [140] The number of animation sequences.
	int seqindex; // [144] The offset to the first sequence description structure.

	int numseqgroups;  // [148] The number of sequence groups.
	int seqgroupindex; // [152] The offset to the first sequence group structure.

	int numtextures;	  // [156] The number of textures.
	int textureindex;	  // [160] The offset to the first texture structure.
	int texturedataindex; // [164] The offset to the texture pixel data.

	int numskinref;		 // [168] The number of replaceable textures.
	int numskinfamilies; // [172] The number of skin families.
	int skinindex;		 // [176] The offset to the skin data (a matrix of texture indices).

	int numbodyparts;  // [180] The number of bodyparts.
	int bodypartindex; // [184] The offset to the first bodypart structure.

	int numattachments;	 // [188] The number of attachment points.
	int attachmentindex; // [192] The offset to the first attachment structure.

	int soundtable;		 // [196] Unused.
	int soundindex;		 // [200] Unused.
	int soundgroups;	 // [204] Unused.
	int soundgroupindex; // [208] Unused.

	int numtransitions;	 // [212] The number of nodes in the sequence transition graph.
	int transitionindex; // [216] The offset to the sequence transition graph data.
};

/**
 *  Header structure for sequence group files. aka studioseqhdr_t
 *
 * Sequence group files can also be loaded using the regular StudioHeader.
 */
struct StudioSequenceGroupHeader
{
	int id;		 // The magic number, should be IDSTUDIOSEQHEADER ("IDSQ").
	int version; // The version number, should be STUDIO_VERSION (10).

	char name[64]; // The model's name.
	int length;	   // The file size in bytes.
};

/**
 *  Bone structure. aka mstudiobone_t
 *
 * Represents a bone in the skeleton.
 */
struct StudioBone
{
	char name[MAXBONENAMELENGTH];		  // [00] The bone's name.
	int parent;							  // [32] The index of the parent bone (-1 for root).
	int flags;							  // [36] Unused.
	int bonecontroller[DEGREESOFFREEDOM]; // [40] Bone controller indices for each degree of freedom (X, Y, Z, rotX, rotY, rotZ). -1 indicates no controller.
	float value[DEGREESOFFREEDOM];		  // [64] Default values for each degree of freedom (position and rotation).
	float scale[DEGREESOFFREEDOM];		  // [88] Scale values used for compressed animation data.
};

/**
 *  Bone controller structure. aka mstudiobonecontroller_t
 *
 * Used to modify bone positions/rotations at runtime.
 */
struct StudioBoneController
{
	int bone;	 // [00] The index of the bone this controller affects.
	int type;	 // [04] The motion type (X, Y, Z, XR, YR, ZR, possibly OR'd with STUDIO_RLOOP).
	float start; // [08] The minimum value for the controller.
	float end;	 // [12] The maximum value for the controller.
	int rest;	 // [16] The controller's value at the rest pose.
	int index;	 // [20] The controller's channel/index.
};

/**
 *  Hitbox structure. aka mstudiobbox_t
 *
 * Defines a collision hitbox associated with a bone.
 */
struct StudioHitbox
{
	int bone;	   // [00] The index of the bone this hitbox is attached to.
	int group;	   // [04] The hit group.
	Vector3 bbmin; // [08] The minimum extent of the hitbox.
	Vector3 bbmax; // [20] The maximum extent of the hitbox.
};

/**
 *  Sequence group structure. aka mstudioseqgroup_t
 *
 * Used for demand-loaded animations stored in separate files.
 */
struct StudioSequenceGroup
{
	char label[32]; // [00] The sequence group's name.
	char name[64];	// [32] The filename of the sequence group (e.g., "barney01.mdl").
	int unused1;	// [96] Unused.
	int unused2;	// [100] Unused. (Previously used for "data" in group 0).
};

/**
 *  Sequence description structure. aka mstudioseqdesc_t
 *
 * Contains information about an animation sequence.
 */
struct StudioSequenceDescription
{
	char label[MAXSEQUENCENAMELENGTH]; // [00] The sequence's name.

	float fps; // [32] The frames per second for this sequence.
	int flags; // [36] Sequence flags (e.g., STUDIO_LOOPING).

	int activity;  // [40] The activity ID.
	int actweight; // [44] The activity's weight (used to choose among sequences with the same activity).

	int numevents;	// [48] The number of animation events.
	int eventindex; // [52] The offset to the first animation event.

	int numframes; // [56] The number of frames in this sequence.

	int numpivots;	// [60] Unused in Goldsrc.
	int pivotindex; // [64] Unused in Goldsrc.

	int motiontype;			// [68] The motion type.
	int motionbone;			// [72] The index of the motion bone.
	Vector3 linearmovement; // [76] The linear movement of the motion bone from the first to the last frame.
	int automoveposindex;	// [88] Unused in Goldsrc.
	int automoveangleindex; // [92] Unused in Goldsrc.

	Vector3 bbmin; // [96] The bounding box min extent for this sequence.
	Vector3 bbmax; // [108] The bounding box max extent for this sequence.

	int numblends; // [120] The number of blend controllers (either 1 or 2).
	int animindex; // [124] The offset to the animation frame offset data.

	int blendtype[MAXSEQUENCEBLEND];	// [128] The motion types for the blend controllers (X, Y, Z, XR, YR, ZR).
	float blendstart[MAXSEQUENCEBLEND]; // [136] The starting blend values.
	float blendend[MAXSEQUENCEBLEND];	// [144] The ending blend values.
	int blendparent;					// [152] Unused in Goldsrc.

	int seqgroup; // [156] The index of the sequence group this sequence belongs to.

	int entrynode; // [160] The entry node in the sequence transition graph.
	int exitnode;  // [164] The exit node in the sequence transition graph.
	int nodeflags; // [168] The node flags (0 or 1, indicating unidirectional or bidirectional transitions).
	int nextseq;   // [172] Unused in Goldsrc.
};

/**
 *  Animation event structure. aka mstudioevent_t
 *
 * Represents an event that occurs at a specific frame in a sequence in the game at run time.
 */
struct StudioAnimationEvent
{
	int frame;					   // [00] The frame at which the event occurs.
	int event;					   // [04] The event ID.
	int type;					   // [08] Unused.
	char options[MAXEVENTOPTIONS]; // [12] Additional event data.
};

/**
 *  Pivot structure. aka mstudiopivot_t
 *
 * Unused in Goldsrc?.
 */
struct StudioPivot
{
	Vector3 org; // pivot point
	int start;
	int end;
};

/**
 *  Attachment structure. aka mstudioattachment_t
 *
 * Defines an attachment point on a bone.
 */
struct StudioAttachment
{
	char name[32];		// [00] Unused in Goldsrc.
	int type;			// [32] Unused in Goldsrc.
	int bone;			// [36] The index of the bone this attachment is connected to.
	Vector3 org;		// [40] The attachment point's position relative to the bone's origin.
	Vector3 vectors[3]; // [52] Unused in Goldsrc.
};

/**
 *  Animation frame offset structure. aka mstudioanim_t
 *
 * Contains offsets to the compressed animation data for a bone in a sequence.
 */
struct StudioAnimationFrameOffset
{
	unsigned short offset[DEGREESOFFREEDOM]; // [00] Offsets to the animation frame data for each degree of freedom. 0 means no data.
};

/**
 *  Animation frame data. aka mstudioanimvalue_t
 *
 * Union representing a single compressed animation value (either position or rotation component).
 */
union StudioAnimationValue
{
	struct
	{
		std::uint8_t valid; // Number of valid anim data.
		std::uint8_t total; // Total number of anim data.
	} num;
	short value; // The actual animation value.
};

/**
 *  Bodypart structure. aka mstudiobodyparts_t
 *
 * Groups together multiple models that can be switched at runtime.
 */
struct StudioBodyPart
{
	char name[MAXBODYPARTSNAMELENGHT]; // [00] The bodypart's name.
	int nummodels;					   // [64] The number of models in this bodypart.
	int base;						   // [68] Used to calculate the model index relative to the bodypart.
	int modelindex;					   // [72] The offset to the first model in this bodypart.
};

/**
 *  Texture structure. aka mstudiotexture_t
 *
 * Contains information about a texture used by the model.
 */
struct StudioTexture
{
	char name[MAXTEXTURENAMELENGTH]; // [00] The texture's name.
	int flags;						 // [64] Texture flags (e.g., STUDIO_NF_CHROME).
	int width;						 // [68] The texture's width in pixels.
	int height;						 // [72] The texture's height in pixels.
	int index;						 // [76] The offset to the texture's pixel data.
};

/**
 *  Model structure. aka mstudiomodel_t
 *
 * Represents a single model within a bodypart.
 */
struct StudioModel
{
	char name[MAXMODELNAMELENGTH]; // [00] The model's name (which is usually the name of the SMD file it was created from).

	int type; // [64] Unused.

	float boundingradius; // [68] Unused.

	int nummesh;   // [72] The number of meshes in this model.
	int meshindex; // [76] The offset to the first mesh in this model.

	int numverts;	   // [80] The number of vertices.
	int vertinfoindex; // [84] The offset to an array of bone indices, one for each vertex, indicating which bone influences that vertex.
	int vertindex;	   // [88] The offset to the vertex position data (Vector3 array).
	int numnorms;	   // [92] The number of normals.
	int norminfoindex; // [96] The offset to an array of bone indices, one for each normal, indicating which bone influences that normal.
	int normindex;	   // [100] The offset to the normal data (Vector3 array).

	int numgroups;	// [104] Unused.
	int groupindex; // [108] Unused.
};

/**
 *  Mesh structure. aka mstudiomesh_t
 *
 * Represents a mesh (a collection of triangles) within a model.
 */
struct StudioMesh
{
	int numtris;   // [00] The total number of triverts in this mesh.
	int triindex;  // [04] The offset to the trivert data.
	int skinref;   // [08] The index of the texture to use for this mesh.
	int numnorms;  // [12] Unused.
	int normindex; // [16] Unused.
};

// --- Lighting and Motion Flags ---
// These flags are used to specify various rendering and animation options.

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
#define STUDIO_RLOOP 0x8000 // Indicates that the controller wraps around (e.g., for angles).

// Sequence flags
#define STUDIO_LOOPING 0x0001 // Indicates that the sequence loops.