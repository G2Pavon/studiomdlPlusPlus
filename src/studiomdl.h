#pragma once

#include "studio.h"

struct s_trianglevert_t
{
	int vertindex;
	int normindex; // index into normal array
	int s, t;
	float u, v;
};

struct s_vertex_t
{
	int bone;	// bone transformation index
	vec3_t org; // original position
};

struct s_normal_t
{
	int skinref;
	int bone;	// bone transformation index
	vec3_t org; // original position
};

struct s_bonefixup_t
{
	vec3_t worldorg;
	float m[3][4];
	float im[3][4];
	float length;
};

struct s_bonetable_t
{
	char name[32];		// bone name for symbolic links
	int parent;			// parent bone
	int bonecontroller; // -1 == 0
	int flags;			// X, Y, Z, XR, YR, ZR
	// short		value[6];	// default DoF values
	vec3_t pos;		   // default pos
	vec3_t posscale;   // pos values scale
	vec3_t rot;		   // default pos
	vec3_t rotscale;   // rotation values scale
	int group;		   // hitgroup
	vec3_t bmin, bmax; // bounding box
};

struct s_renamebone_t
{
	char from[32];
	char to[32];
};

struct s_bbox_t
{
	char name[32]; // bone name
	int bone;
	int group; // hitgroup
	int model;
	vec3_t bmin, bmax; // bounding box
};

struct s_hitgroup_t
{
	int models;
	int group;
	char name[32]; // bone name
};

struct s_bonecontroller_t
{
	char name[32];
	int bone;
	int type;
	int index;
	float start;
	float end;
};

struct s_attachment_t
{
	char name[32];
	char bonename[32];
	int index;
	int bone;
	int type;
	vec3_t org;
};

struct s_node_t
{
	char name[64];
	int parent;
	int mirrored;
};

struct s_animation_t
{
	char name[64];
	int startframe;
	int endframe;
	int flags;
	int numbones;
	s_node_t node[MAXSTUDIOSRCBONES];
	int bonemap[MAXSTUDIOSRCBONES];
	int boneimap[MAXSTUDIOSRCBONES];
	vec3_t *pos[MAXSTUDIOSRCBONES];
	vec3_t *rot[MAXSTUDIOSRCBONES];
	int numanim[MAXSTUDIOSRCBONES][6];
	mstudioanimvalue_t *anim[MAXSTUDIOSRCBONES][6];
};

struct s_event_t
{
	int event;
	int frame;
	char options[64];
};

struct s_pivot_t
{
	int index;
	vec3_t org;
	int start;
	int end;
};

struct s_sequence_t
{
	int motiontype;
	vec3_t linearmovement;

	char name[64];
	int flags;
	float fps;
	int numframes;

	int activity;
	int actweight;

	int frameoffset; // used to adjust frame numbers

	int numevents;
	s_event_t event[MAXSTUDIOEVENTS];

	int numpivots;
	s_pivot_t pivot[MAXSTUDIOPIVOTS];

	int numblends;
	s_animation_t *panim[MAXSTUDIOGROUPS];
	float blendtype[2];
	float blendstart[2];
	float blendend[2];

	vec3_t automovepos[MAXSTUDIOANIMATIONS];
	vec3_t automoveangle[MAXSTUDIOANIMATIONS];

	int seqgroup;
	int animindex;

	vec3_t bmin;
	vec3_t bmax;
	int entrynode;
	int exitnode;
	int nodeflags;
};

struct s_sequencegroup_t
{
	char label[32];
	char name[64];
};

struct rgb_t
{
	std::uint8_t r, g, b;
};
struct rgb2_t
{
	std::uint8_t b, g, r, x;
};

// FIXME: what about texture overrides inline with loading models
struct s_texture_t
{
	char name[64];
	int flags;
	int srcwidth;
	int srcheight;
	std::uint8_t *ppicture;
	rgb_t *ppal;
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

struct s_mesh_t
{
	int alloctris;
	int numtris;
	s_trianglevert_t (*triangle)[3];

	int skinref;
	int numnorms;
};

struct s_bone_t
{
	vec3_t pos;
	vec3_t rot;
};

struct s_model_t
{
	char name[64];

	int numbones;
	s_node_t node[MAXSTUDIOSRCBONES];
	s_bone_t skeleton[MAXSTUDIOSRCBONES];
	int boneref[MAXSTUDIOSRCBONES];	 // is local bone (or child) referenced with a vertex
	int bonemap[MAXSTUDIOSRCBONES];	 // local bone to world bone mapping
	int boneimap[MAXSTUDIOSRCBONES]; // world bone to local bone mapping

	vec3_t boundingbox[MAXSTUDIOSRCBONES][2];

	s_mesh_t *trimesh[MAXSTUDIOTRIANGLES];
	int trimap[MAXSTUDIOTRIANGLES];

	int numverts;
	s_vertex_t vert[MAXSTUDIOVERTS];

	int numnorms;
	s_normal_t normal[MAXSTUDIOVERTS];

	int nummesh;
	s_mesh_t *pmesh[MAXSTUDIOMESHES];

	float boundingradius;

	int numframes;
	float interval;
	struct s_model_s *next;
};

struct s_bodypart_t
{
	char name[32];
	int nummodels;
	int base;
	s_model_t *pmodel[MAXSTUDIOMODELS];
};

// QC Command variables -----------------
extern char g_modelnameCommand[1024];				// $modelname
extern vec3_t g_eyepositionCommand;					// $eyeposition
extern int g_flagsCommand;							// $flags
extern vec3_t g_bboxCommand[2];						// $bbox
extern vec3_t g_cboxCommand[2];						// $cbox
extern s_bbox_t g_hitboxCommand[MAXSTUDIOSRCBONES]; // $hbox
extern int g_hitboxescount;
extern s_bonecontroller_t g_bonecontrollerCommand[MAXSTUDIOSRCBONES]; // $$controller
extern int g_bonecontrollerscount;
extern s_attachment_t g_attachmentCommand[MAXSTUDIOSRCBONES]; // $attachment
extern int g_attachmentscount;
extern s_sequence_t g_sequenceCommand[MAXSTUDIOSEQUENCES]; // $sequence
extern int g_sequencecount;
extern s_sequencegroup_t g_sequencegroupCommand[MAXSTUDIOSEQUENCES]; // $sequencegroup
extern int g_sequencegroupcount;
extern s_model_t *g_submodel[MAXSTUDIOMODELS]; // $body
extern int g_submodelscount;
extern s_bodypart_t g_bodypart[MAXSTUDIOBODYPARTS]; // $bodygroup
extern int g_bodygroupcount;
// -----------------------------------------

extern int g_numxnodes;
extern int g_xnode[100][100];
extern s_bonetable_t g_bonetable[MAXSTUDIOSRCBONES];
extern int g_bonescount;

extern s_texture_t g_texture[MAXSTUDIOSKINS];
extern int g_texturescount;
extern int g_skinrefcount;
extern int g_skinfamiliescount;
extern int g_skinref[256][MAXSTUDIOSKINS]; // [skin][skinref], returns texture index

// Main functions -----------------------

// setSkinValues:
void SetSkinValues();
void Grab_Skin(s_texture_t *ptexture);
void Grab_BMP(char *filename, s_texture_t *ptexture);
void ResizeTexture(s_texture_t *ptexture);
void ResetTextureCoordRanges(s_mesh_t *pmesh, s_texture_t *ptexture);
void TextureCoordRanges(s_mesh_t *pmesh, s_texture_t *ptexture);

// SimplifyModel:
void SimplifyModel();
void MakeTransitions();
int findNode(char *name);
void OptimizeAnimations();
void ExtractMotion();

// End Main functions --------------------

// QC Parser
void Cmd_Modelname();
void Cmd_ScaleUp();
void Cmd_Rotate();
int Cmd_Controller();
void Cmd_Body();
void Cmd_Bodygroup();
void Cmd_Body_OptionStudio();
int Cmd_Body_OptionBlank();
int Cmd_Sequence();
int Cmd_Sequence_OptionDeform(s_sequence_t *psequence); // delete this
int Cmd_Sequence_OptionEvent(s_sequence_t *psequence);
int Cmd_Sequence_OptionAddPivot(s_sequence_t *psequence);
int Cmd_Sequence_OptionFps(s_sequence_t *psequence);
void Cmd_Sequence_OptionRotate();
void Cmd_Sequence_OptionScaleUp();
void Cmd_Sequence_OptionRotate();
void Cmd_Sequence_OptionAnimation(char *name, s_animation_t *panim);
void Grab_OptionAnimation(s_animation_t *panim);
void Shift_OptionAnimation(s_animation_t *panim);
int Cmd_Sequence_OptionAction(char *szActivity);
int Cmd_SequenceGroup();
void Cmd_Eyeposition();
void Cmd_Origin();
void Cmd_BBox();
void Cmd_CBox();
void Cmd_Mirror();
void Cmd_Gamma();
void Cmd_Flags();
int Cmd_TextureGroup();
int Cmd_Hitgroup();
int Cmd_Hitbox();
int Cmd_Attachment();
void Cmd_Renamebone();
void Cmd_TexRenderMode();
int lookupControl(char *string);
void ParseQCscript();

// SMD Parser
void ParseSMD(s_model_t *pmodel);
void Grab_SMDTriangles(s_model_t *pmodel);
void Grab_SMDSkeleton(s_node_t *pnodes, s_bone_t *pbones);
int Grab_SMDNodes(s_node_t *pnodes);
void Build_Reference(s_model_t *pmodel);
s_mesh_t *FindMeshByTexture(s_model_t *pmodel, char *texturename);
s_trianglevert_t *FindMeshTriangleByIndex(s_mesh_t *pmesh, int index);
int FindVertexNormalIndex(s_model_t *pmodel, s_normal_t *pnormal);
int FindVertexIndex(s_model_t *pmodel, s_vertex_t *pv);
void AdjustVertexToQcOrigin(float *org);
void ScaleVertexByQcScale(float *org);
void clip_rotations(vec3_t rot);

// Common QC and SMD parser
int FindTextureIndex(char *texturename);

// Helpers
char *stristr(const char *string, const char *string2);

extern int BuildTris(s_trianglevert_t (*x)[3], s_mesh_t *y, std::uint8_t **ppdata);
extern int LoadBMP(const char *szFile, std::uint8_t **ppbBits, std::uint8_t **ppbPalette, int *width, int *height);
