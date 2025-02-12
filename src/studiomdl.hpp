#pragma once

#include <filesystem>
#include <string>

#include "format/mdl.hpp"
#include "format/qc.hpp"
#include "modeldata.hpp"
#include "utils/mathlib.hpp"

// Common studiomdl and writemdl variables -----------------
extern std::array<std::array<int, 100>, 100> g_xnode;
extern int g_numxnodes; // Not initialized??

extern std::vector<BoneTable> g_bonetable;

extern std::vector<Texture> g_textures;

extern std::array<std::array<int, MAXSTUDIOSKINS>, 256> g_skinref; // [skin][skinref], returns texture index
extern int g_skinrefcount;
extern int g_skinfamiliescount;

// Main functions -----------------------

// setSkinValues:
void set_skin_values(QC &qc);
void grab_skin(const QC &qc, Texture *ptexture);
void grab_bmp(const char *filename, Texture *ptexture);
void resize_texture(const QC &qc, Texture *ptexture);
void reset_texture_coord_ranges(Mesh *pmesh, const Texture *ptexture);
void texture_coord_ranges(Mesh *pmesh, Texture *ptexture);

// SimplifyModel:
void simplify_model(QC &qc);
void make_transitions(QC &qc);
int find_node(const std::string name);
void optimize_animations(QC &qc);
void extract_motion(QC &qc);

// End Main functions --------------------

// QC Parser
void cmd_modelname(QC &qc, std::string &token);
void cmd_scale(QC &qc, std::string &token);
void cmd_rotate(QC &qc, std::string &token);
int cmd_controller(QC &qc, std::string &token);
void cmd_body(QC &qc, std::string &token);
void cmd_bodygroup(QC &qc, std::string &token);
void cmd_body_option_studio(QC &qc, std::string &token);
int cmd_body_option_blank(QC &qc);
int cmd_sequence(QC &qc, std::string &token);
int cmd_sequence_option_event(std::string &token, Sequence &seq);
int cmd_sequence_option_fps(std::string &token, Sequence &seq);
void cmd_sequence_option_origin(QC &qc, std::string &token);
void cmd_sequence_option_rotate(QC &qc, std::string &token);
void cmd_sequence_option_scale(QC &qc, std::string &token);
void shift_option_animation(Animation &anim);
int cmd_sequence_option_action(std::string &szActivity);
void cmd_eyeposition(QC &qc, std::string &token);
void cmd_origin(QC &qc, std::string &token);
void cmd_bbox(QC &qc, std::string &token);
void cmd_cbox(QC &qc, std::string &token);
void cmd_mirror(QC &qc, std::string &token);
void cmd_gamma(QC &qc, std::string &token);
void cmd_flags(QC &qc, std::string &token);
int cmd_texturegroup(QC &qc, std::string &token);
int cmd_hitgroup(QC &qc, std::string &token);
int cmd_hitbox(QC &qc, std::string &token);
int cmd_attachment(QC &qc, std::string &token);
void cmd_renamebone(QC &qc, std::string &token);
void cmd_texrendermode(std::string &token);
int lookup_control(const std::string);
void parse_qc_file(const std::filesystem::path path, QC &qc);

// SMD Parser
void parse_smd_reference(const QC &qc, Model *pmodel);
void parse_smd_animation(const QC &qc, std::filesystem::path &smd_path, Animation &anim);
void parse_smd_reference_skeleton(const QC &qc, std::vector<Node> &nodes, std::vector<Bone> &bones, std::filesystem::path &path);
void parse_smd_animation_skeleton(const QC &qc, Animation &anim);
void parse_smd_triangles(const QC &qc, Model *pmodel);
int parse_smd_nodes(const QC &qc, std::vector<Node> &nodes);
void build_reference(const Model *pmodel);
Mesh *find_mesh_by_texture(Model *pmodel, const std::string texturename);
TriangleVert *find_mesh_triangle_by_index(Mesh *pmesh, const int index);
int find_vertex_normal_index(Model *pmodel, const Normal *pnormal);
int find_vertex_index(Model *pmodel, Vertex *pv);
void clip_rotations(Vector3 rot);

// Common QC and SMD parser
int find_texture_index(const std::string texturename);