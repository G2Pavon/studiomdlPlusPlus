#pragma once

#include <string>

#include "studio.hpp"

struct TriangleVert
{
	int vertindex;
	int normindex; // index into normal array
	int s, t;
	float u, v;
};

struct Vertex
{
	int bone;	 // bone transformation index
	Vector3 org; // original position
};

struct Normal
{
	int skinref;
	int bone;	 // bone transformation index
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
	int parent;					  // parent bone
	int bonecontroller;			  // -1 == 0
	int flags;					  // X, Y, Z, XR, YR, ZR
	// short		value[6];	// default DoF values
	Vector3 pos;		// default pos
	Vector3 posscale;	// pos values scale
	Vector3 rot;		// default pos
	Vector3 rotscale;	// rotation values scale
	int group;			// hitgroup
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
	int boneref[MAXSTUDIOSRCBONES];	 // is local bone (or child) referenced with a vertex
	int bonemap[MAXSTUDIOSRCBONES];	 // local bone to world bone mapping
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
	struct s_model_s *next;
};

struct BodyPart
{
	char name[32];
	int nummodels;
	int base;
	Model *pmodel[MAXSTUDIOMODELS];
};

// QC Command variables -----------------
extern char g_modelnameCommand[1024];			  // $modelname
extern Vector3 g_eyepositionCommand;			  // $eyeposition
extern int g_flagsCommand;						  // $flags
extern Vector3 g_bboxCommand[2];				  // $bbox
extern Vector3 g_cboxCommand[2];				  // $cbox
extern HitBox g_hitboxCommand[MAXSTUDIOSRCBONES]; // $hbox
extern int g_hitboxescount;
extern BoneController g_bonecontrollerCommand[MAXSTUDIOSRCBONES]; // $$controller
extern int g_bonecontrollerscount;
extern Attachment g_attachmentCommand[MAXSTUDIOSRCBONES]; // $attachment
extern int g_attachmentscount;
extern Sequence g_sequenceCommand[MAXSTUDIOSEQUENCES]; // $sequence
extern int g_sequencecount;
extern SequenceGroup g_sequencegroupCommand[MAXSTUDIOSEQUENCES]; // $sequencegroup
extern int g_sequencegroupcount;
extern Model *g_submodel[MAXSTUDIOMODELS]; // $body
extern int g_submodelscount;
extern BodyPart g_bodypart[MAXSTUDIOBODYPARTS]; // $bodygroup
extern int g_bodygroupcount;
// -----------------------------------------

extern int g_numxnodes;
extern int g_xnode[100][100];
extern BoneTable g_bonetable[MAXSTUDIOSRCBONES];
extern int g_bonescount;

extern Texture g_texture[MAXSTUDIOSKINS];
extern int g_texturescount;
extern int g_skinrefcount;
extern int g_skinfamiliescount;
extern int g_skinref[256][MAXSTUDIOSKINS]; // [skin][skinref], returns texture index

// Main functions -----------------------

// setSkinValues:
void set_skin_values();
void grab_skin(Texture *ptexture);
void grab_bmp(char *filename, Texture *ptexture);
void resize_texture(Texture *ptexture);
void reset_texture_coord_ranges(Mesh *pmesh, Texture *ptexture);
void texture_coord_ranges(Mesh *pmesh, Texture *ptexture);

// SimplifyModel:
void simplify_model();
void make_transitions();
int find_node(char *name);
void optimize_animations();
void extract_motion();

// End Main functions --------------------

// QC Parser
void cmd_modelname(std::string &token);
void cmd_scale(std::string &token);
void cmd_rotate(std::string &token);
int cmd_controller(std::string &token);
void cmd_body(std::string &token);
void cmd_bodygroup(std::string &token);
void cmd_body_optionstudio(std::string &token);
int cmd_body_optionblank();
int cmd_sequence(std::string &token);
int cmd_sequence_option_event(std::string &token, Sequence *psequence);
int cmd_sequence_option_addpivot(std::string &token, Sequence *psequence);
int cmd_sequence_option_fps(std::string &token, Sequence *psequence);
void cmd_sequence_option_rotate(std::string &token);
void cmd_sequence_option_scale(std::string &token);
void cmd_sequence_option_animation(char *name, Animation *panim);
void grab_option_animation(Animation *panim);
void shift_option_animation(Animation *panim);
int cmd_sequence_option_action(std::string &szActivity);
int cmd_sequencegroup(std::string &token);
void cmd_eyeposition(std::string &token);
void cmd_origin(std::string &token);
void cmd_bbox(std::string &token);
void cmd_cbox(std::string &token);
void cmd_mirror(std::string &token);
void cmd_gamma(std::string &token);
void cmd_flags(std::string &token);
int cmd_texturegroup(std::string &token);
int cmd_hitgroup(std::string &token);
int cmd_hitbox(std::string &token);
int cmd_attachment(std::string &token);
void cmd_renamebone(std::string &token);
void cmd_texrendermode(std::string &token);
int lookup_control(char *string);
void parse_qc_file();

// SMD Parser
void parse_smd(Model *pmodel);
void grab_smd_triangles(Model *pmodel);
void grab_smd_skeleton(Node *pnodes, Bone *pbones);
int grab_smd_nodes(Node *pnodes);
void build_reference(Model *pmodel);
Mesh *find_mesh_by_texture(Model *pmodel, char *texturename);
TriangleVert *find_mesh_triangle_by_index(Mesh *pmesh, int index);
int finx_vertex_normal_index(Model *pmodel, Normal *pnormal);
int find_vertex_index(Model *pmodel, Vertex *pv);
void adjust_vertex_to_origin(float *org);
void scale_vertex(float *org);
void clip_rotations(Vector3 rot);

// Common QC and SMD parser
int find_texture_index(std::string texturename);

// Helpers
char *stristr(const char *string, const char *string2);

extern int build_tris(TriangleVert (*x)[3], Mesh *y, std::uint8_t **ppdata);
extern int load_bmp(const char *szFile, std::uint8_t **ppbBits, std::uint8_t **ppbPalette, int *width, int *height);
