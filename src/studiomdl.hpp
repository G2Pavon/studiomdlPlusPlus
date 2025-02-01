#pragma once

#include <string>

#include "utils/mathlib.hpp"
#include "modeldata.hpp"
#include "format/mdl.hpp"
#include "format/qc.hpp"

// Common studiomdl and writemdl variables -----------------
extern std::array<std::array<int, 100>, 100> g_xnode;
extern int g_numxnodes; // Not initialized??

extern std::array<BoneTable, MAXSTUDIOSRCBONES> g_bonetable;
extern int g_bonescount;

extern std::vector<Texture> g_textures;

extern std::array<std::array<int, MAXSTUDIOSKINS>, 256> g_skinref; // [skin][skinref], returns texture index
extern int g_skinrefcount;
extern int g_skinfamiliescount;

// Main functions -----------------------

// setSkinValues:
void set_skin_values(QC &qc_cmd);
void grab_skin(QC &qc_cmd, Texture *ptexture);
void grab_bmp(char *filename, Texture *ptexture);
void resize_texture(QC &qc_cmd, Texture *ptexture);
void reset_texture_coord_ranges(Mesh *pmesh, Texture *ptexture);
void texture_coord_ranges(Mesh *pmesh, Texture *ptexture);

// SimplifyModel:
void simplify_model(QC &qc_cmd);
void make_transitions(QC &qc_cmd);
int find_node(std::string name);
void optimize_animations(QC &qc_cmd);
void extract_motion(QC &qc_cmd);

// End Main functions --------------------

// QC Parser
void cmd_modelname(QC &qc_cmd, std::string &token);
void cmd_scale(QC &qc_cmd, std::string &token);
void cmd_rotate(QC &qc_cmd, std::string &token);
int cmd_controller(QC &qc_cmd, std::string &token);
void cmd_body(QC &qc_cmd, std::string &token);
void cmd_bodygroup(QC &qc_cmd, std::string &token);
void cmd_body_option_studio(QC &qc_cmd, std::string &token);
int cmd_body_option_blank(QC &qc_cmd);
int cmd_sequence(QC &qc_cmd, std::string &token);
int cmd_sequence_option_event(std::string &token, Sequence *psequence);
int cmd_sequence_option_fps(std::string &token, Sequence *psequence);
void cmd_sequence_option_origin(QC &qc_cmd, std::string &token);
void cmd_sequence_option_rotate(QC &qc_cmd, std::string &token);
void cmd_sequence_option_scale(QC &qc_cmd, std::string &token);
void cmd_sequence_option_animation(QC &qc_cmd, char *name, Animation *panim);
void grab_option_animation(QC &qc_cmd, Animation *panim);
void shift_option_animation(Animation *panim);
int cmd_sequence_option_action(std::string &szActivity);
void cmd_eyeposition(QC &qc_cmd, std::string &token);
void cmd_origin(QC &qc_cmd, std::string &token);
void cmd_bbox(QC &qc_cmd, std::string &token);
void cmd_cbox(QC &qc_cmd, std::string &token);
void cmd_mirror(QC &qc_cmd, std::string &token);
void cmd_gamma(QC &qc_cmd, std::string &token);
void cmd_flags(QC &qc_cmd, std::string &token);
int cmd_texturegroup(QC &qc_cmd, std::string &token);
int cmd_hitgroup(QC &qc_cmd, std::string &token);
int cmd_hitbox(QC &qc_cmd, std::string &token);
int cmd_attachment(QC &qc_cmd, std::string &token);
void cmd_renamebone(QC &qc_cmd, std::string &token);
void cmd_texrendermode(std::string &token);
int lookup_control(const char *string);
void parse_qc_file(QC &qc_cmd);

// SMD Parser
void parse_smd(QC &qc_cmd, Model *pmodel);
void grab_smd_triangles(QC &qc_cmd, Model *pmodel);
void grab_smd_skeleton(QC &qc_cmd, Node *pnodes, Bone *pbones);
int grab_smd_nodes(QC &qc_cmd, Node *pnodes);
void build_reference(Model *pmodel);
Mesh *find_mesh_by_texture(Model *pmodel, char *texturename);
TriangleVert *find_mesh_triangle_by_index(Mesh *pmesh, int index);
int find_vertex_normal_index(Model *pmodel, Normal *pnormal);
int find_vertex_index(Model *pmodel, Vertex *pv);
void clip_rotations(Vector3 rot);

// Common QC and SMD parser
int find_texture_index(std::string texturename);