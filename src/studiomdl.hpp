#pragma once

#include <string>

#include "studio.hpp"
#include "mathlib.hpp"
#include "modeldata.hpp"

// Common studiomdl and writemdl QC's variables -----------------
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

// Common studiomdl and writemdl variables -----------------
extern int g_numxnodes; // Not initialized??
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