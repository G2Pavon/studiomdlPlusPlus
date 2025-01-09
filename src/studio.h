/***
 *
 *	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

#ifndef _STUDIO_H_
#define _STUDIO_H_

#include <cstdint>

#include "studio_event.h"
/*
==============================================================================

STUDIO MODELS

Studio models are position independent, so the cache manager can move them.
==============================================================================
*/

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

	int soundtable;
	int soundindex;
	int soundgroups;
	int soundgroupindex;

	int numtransitions; // animation node to animation node transition graph
	int transitionindex;
};

// header for demand loaded sequence group data
struct studioseqhdr_t
{
	int id;
	int version;

	char name[64];
	int length;
};

// bones
struct mstudiobone_t
{
	char name[32];		   // bone name for symbolic links
	int parent;			   // parent bone
	int flags;			   // ??
	int bonecontroller[6]; // bone controller index, -1 == none
	float value[6];		   // default DoF values
	float scale[6];		   // scale for delta DoF values
};

// bone controllers
struct mstudiobonecontroller_t
{
	int bone; // -1 == 0
	int type; // X, Y, Z, XR, YR, ZR, M
	float start;
	float end;
	int rest;  // byte index value at rest
	int index; // 0-3 user set controller, 4 mouth
};

// intersection boxes
struct mstudiobbox_t
{
	int bone;
	int group;	  // intersection group
	vec3_t bbmin; // bounding box
	vec3_t bbmax;
};

//
// demand loaded sequence groups
//
struct mstudioseqgroup_t
{
	char label[32]; // textual name
	char name[64];	// file name
	int32_t unused1;
	int unused2; // was "data" -  hack for group 0
};

// sequence descriptions
struct mstudioseqdesc_t
{
	char label[32]; // sequence label

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

	int blendtype[2];	 // X, Y, Z, XR, YR, ZR
	float blendstart[2]; // starting value
	float blendend[2];	 // ending value
	int blendparent;

	int seqgroup; // sequence group for demand loading

	int entrynode; // transition node at entry
	int exitnode;  // transition node at exit
	int nodeflags; // transition rules

	int nextseq; // auto advancing sequences
};

// pivots
struct mstudiopivot_t
{
	vec3_t org; // pivot point
	int start;
	int end;
};

// attachment
struct mstudioattachment_t
{
	char name[32];
	int type;
	int bone;
	vec3_t org; // attachment point
	vec3_t vectors[3];
};

struct mstudioanim_t
{
	unsigned short offset[6];
};

// animation frames

union mstudioanimvalue_t
{
	struct
	{
		byte valid;
		byte total;
	} num;
	short value;
};
// body part index
struct mstudiobodyparts_t
{
	char name[64];
	int nummodels;
	int base;
	int modelindex; // index into models array
};

// skin info
struct mstudiotexture_t
{
	char name[64];
	int flags;
	int width;
	int height;
	int index;
};

// skin families
// short	index[skinfamilies][skinref]

// studio models
struct mstudiomodel_t
{
	char name[64];

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

// vec3_t	boundingbox[model][bone][2];	// complex intersection info

// meshes
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
#define STUDIO_NF_TRANSPARENT 0x0040 // or STUDIO_NF_MASKED

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

#endif
