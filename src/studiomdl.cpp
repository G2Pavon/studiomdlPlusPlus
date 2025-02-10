// studiomdl.cpp: generates a studio .mdl file from a .qc script

#include <cstring>
#include <string>
#include <filesystem>
#include <algorithm>
#include <iostream>

#include "studiomdl.hpp"
#include "utils/cmdlib.hpp"
#include "utils/mathlib.hpp"
#include "format/image/bmp.hpp"
#include "format/mdl.hpp"
#include "format/qc.hpp"
#include "writemdl.hpp"

#include "monsters/activity.hpp"
#include "monsters/activitymap.hpp"


// studiomdl.exe args -----------
bool g_flaginvertnormals;
float g_flagnormalblendangle;
bool g_flagkeepallbones;

// SMD variables --------------------------
std::filesystem::path g_smdpath;
FILE *g_smdfile;
char g_currentsmdline[1024];
int g_smdlinecount;

char g_defaulttextures[16][256];
char g_sourcetexture[16][256];
int g_numtextureteplacements;
BoneFixUp g_bonefixup[MAXSTUDIOSRCBONES];

// Common studiomdl and writemdl variables -----------------
std::array<std::array<int, 100>, 100> g_xnode;
int g_numxnodes; // Not initialized??

std::vector<BoneTable> g_bonetable;

std::vector<Texture> g_textures;

std::array<std::array<int, MAXSTUDIOSKINS>, 256> g_skinref; // [skin][skinref], returns texture index
int g_skinrefcount;
int g_skinfamiliescount;

// ---------------------------------------
void clip_rotations(Vector3 rot)
{
	// clip everything to : -Q_PI <= x < Q_PI
	for (int j = 0; j < 3; j++)
	{
		while (rot[j] >= Q_PI)
			rot[j] -= Q_PI * 2;
		while (rot[j] < -Q_PI)
			rot[j] += Q_PI * 2;
	}
}

void extract_motion(QC &qc)
{
	int i, j, k;
	int blendIndex;

	// extract linear motion
	for (auto& sequence : qc.sequences)
	{
		if (sequence.numframes > 1)
		{
			// assume 0 for now.
			Vector3 motion{0, 0, 0};
			int typeMotion = sequence.motiontype;
			Vector3* ptrPos = sequence.anims[0].pos[0];

			int k = sequence.numframes - 1;

			if (typeMotion & STUDIO_LX)
				motion[0] = ptrPos[k][0] - ptrPos[0][0];
			if (typeMotion & STUDIO_LY)
				motion[1] = ptrPos[k][1] - ptrPos[0][1];
			if (typeMotion & STUDIO_LZ)
				motion[2] = ptrPos[k][2] - ptrPos[0][2];

			for (j = 0; j < sequence.numframes; j++)
			{
				Vector3 adjustedPosition;
				for (k = 0; k < sequence.anims[0].nodes.size(); k++)
				{
					if (sequence.anims[0].nodes[k].parent == -1)
					{
						ptrPos = sequence.anims[0].pos[k];

						adjustedPosition = motion * (static_cast<float>(j) / static_cast<float>(sequence.numframes - 1));
						for (blendIndex = 0; blendIndex < sequence.anims.size(); blendIndex++)
						{
							sequence.anims[blendIndex].pos[k][j] = sequence.anims[blendIndex].pos[k][j] - adjustedPosition;
						}
					}
				}
			}

			sequence.linearmovement = motion;
		}
		else
		{
			sequence.linearmovement = Vector3(0, 0, 0);
		}
	}


	// extract unused motion
	for (auto& sequence : qc.sequences)
	{
		int typeUnusedMotion = sequence.motiontype;
		for (k = 0; k < sequence.anims[0].nodes.size(); k++)
		{
			if (sequence.anims[0].nodes[k].parent == -1)
			{
				for (blendIndex = 0; blendIndex < sequence.anims.size(); blendIndex++)
				{
					float motion[6];
					motion[0] = sequence.anims[blendIndex].pos[k][0][0];
					motion[1] = sequence.anims[blendIndex].pos[k][0][1];
					motion[2] = sequence.anims[blendIndex].pos[k][0][2];
					motion[3] = sequence.anims[blendIndex].rot[k][0][0];
					motion[4] = sequence.anims[blendIndex].rot[k][0][1];
					motion[5] = sequence.anims[blendIndex].rot[k][0][2];

					for (j = 0; j < sequence.numframes; j++)
					{
						if (typeUnusedMotion & STUDIO_XR)
							sequence.anims[blendIndex].rot[k][j][0] = motion[3];
						if (typeUnusedMotion & STUDIO_YR)
							sequence.anims[blendIndex].rot[k][j][1] = motion[4];
						if (typeUnusedMotion & STUDIO_ZR)
							sequence.anims[blendIndex].rot[k][j][2] = motion[5];
					}
				}
			}
		}
	}
}

void optimize_animations(QC &qc)
{
	int startFrame, endFrame;
	int typeMotion;
	int iError = 0;

	// optimize animations
	for (auto& sequence : qc.sequences)
	{
		sequence.numframes = sequence.anims[0].endframe - sequence.anims[0].startframe + 1;

		// force looping animations to be looping
		if (sequence.flags & STUDIO_LOOPING)
		{
			for (int j = 0; j < sequence.anims[0].nodes.size(); j++)
			{
				for (int blends = 0; blends < sequence.anims.size(); blends++)
				{
					Vector3 *ppos = sequence.anims[blends].pos[j];
					Vector3 *prot = sequence.anims[blends].rot[j];

					startFrame = 0;								 // sequence[i].panim[q]->startframe;
					endFrame = sequence.numframes - 1; // sequence[i].panim[q]->endframe;

					typeMotion = sequence.motiontype;
					if (!(typeMotion & STUDIO_LX))
						ppos[endFrame][0] = ppos[startFrame][0];
					if (!(typeMotion & STUDIO_LY))
						ppos[endFrame][1] = ppos[startFrame][1];
					if (!(typeMotion & STUDIO_LZ))
						ppos[endFrame][2] = ppos[startFrame][2];

					prot[endFrame][0] = prot[startFrame][0];
					prot[endFrame][1] = prot[startFrame][1];
					prot[endFrame][2] = prot[startFrame][2];
				}
			}
		}

		for (auto& event : sequence.events)
		{
			if (event.frame < sequence.anims[0].startframe)
			{
				printf("sequence %s has event (%d) before first frame (%d)\n", sequence.name.c_str(), event.frame, sequence.anims[0].startframe);
				event.frame = sequence.anims[0].startframe;
				iError++;
			}
			if (event.frame > sequence.anims[0].endframe)
			{
				printf("sequence %s has event (%d) after last frame (%d)\n", sequence.name.c_str(), event.frame, sequence.anims[0].endframe);
				event.frame = sequence.anims[0].endframe;
				iError++;
			}
		}

		sequence.frameoffset = sequence.anims[0].startframe;
	}
}

int find_node(std::string name)
{
	for (int k = 0; k < g_bonetable.size(); k++)
	{
		if (g_bonetable[k].name == name)
		{
			return k;
		}
	}
	return -1;
}

void make_transitions(QC &qc)
{
	int i, j;
	int iHit;

	// Add in direct node transitions
	for (auto& sequence : qc.sequences)
	{
		if (sequence.entrynode != sequence.exitnode)
		{
			g_xnode[sequence.entrynode - 1][sequence.exitnode - 1] = sequence.exitnode;
			if (sequence.nodeflags)
			{
				g_xnode[sequence.exitnode - 1][sequence.entrynode - 1] = sequence.entrynode;
			}
		}
		if (sequence.entrynode > g_numxnodes)
			g_numxnodes = sequence.entrynode;
	}

	// Add multi-stage transitions
	while (true)
	{
		iHit = 0;
		for (i = 1; i <= g_numxnodes; i++)
		{
			for (j = 1; j <= g_numxnodes; j++)
			{
				// If I can't go there directly
				if (i != j && g_xnode[i - 1][j - 1] == 0)
				{
					for (int k = 1; k < g_numxnodes; k++)
					{
						// But I found someone who knows how that I can get to
						if (g_xnode[k - 1][j - 1] > 0 && g_xnode[i - 1][k - 1] > 0)
						{
							// Then go to them
							g_xnode[i - 1][j - 1] = -g_xnode[i - 1][k - 1];
							iHit = 1;
							break;
						}
					}
				}
			}
		}

		// Reset previous pass so the links can be used in the next pass
		for (i = 1; i <= g_numxnodes; i++)
		{
			for (j = 1; j <= g_numxnodes; j++)
			{
				g_xnode[i - 1][j - 1] = abs(g_xnode[i - 1][j - 1]);
			}
		}

		if (!iHit)
			break; // No more transitions are added
	}
}

void simplify_model(QC &qc)
{
	int i, j, k;
	int n, m, q;
	Vector3 *defaultpos[MAXSTUDIOSRCBONES] = {0};
	Vector3 *defaultrot[MAXSTUDIOSRCBONES] = {0};
	int iError = 0;

	optimize_animations(qc);
	extract_motion(qc);
	make_transitions(qc);

	// find used bones TODO: find_used_bones()
	for (auto& submodel : qc.submodels)
	{
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			submodel->boneref[k] = g_flagkeepallbones;
		}
		for (auto& vert : submodel->verts)
		{
			submodel->boneref[vert.bone_id] = 1;
		}
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			// tag parent bones as used if child has been used
			if (submodel->boneref[k])
			{
				n = submodel->nodes[k].parent;
				while (n != -1 && !submodel->boneref[n])
				{
					submodel->boneref[n] = 1;
					n = submodel->nodes[n].parent;
				}
			}
		}
	}

	// rename model bones if needed TODO: rename_submodel_bones()
	for (auto& submodel : qc.submodels)
	{
		for (auto& node : submodel->nodes)
		{
			for (auto& rename : qc.renamebones)
			{
				if (node.name == rename.from)
				{
					node.name = rename.to;
					break;
				}
			}
		}
	}

	// union of all used bones TODO:create_bone_union()
	g_bonetable.clear();
	for (auto& submodel : qc.submodels)
	{
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			submodel->boneimap[k] = -1;
		}
		for (j = 0; j < submodel->nodes.size(); j++)
		{
			if (submodel->boneref[j])
			{
				k = find_node(submodel->nodes[j].name);
				if (k == -1)
				{
					// create new bone
					k = g_bonetable.size();
					BoneTable newb;
					newb.name = submodel->nodes[j].name;
					if ((n = submodel->nodes[j].parent) != -1)
						newb.parent = find_node(submodel->nodes[n].name);
					else
						newb.parent = -1;
					// set defaults
					defaultpos[k] = (Vector3 *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(Vector3));
					defaultrot[k] = (Vector3 *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(Vector3));
					for (n = 0; n < MAXSTUDIOANIMATIONS; n++)
					{
						defaultpos[k][n] = submodel->skeleton[j].pos;
						defaultrot[k][n] = submodel->skeleton[j].rot;
					}
					newb.pos = submodel->skeleton[j].pos;
					newb.rot = submodel->skeleton[j].rot;
					g_bonetable.push_back(newb);
				}
				else
				{
					// double check parent assignments
					n = submodel->nodes[j].parent;
					if (n != -1)
						n = find_node(submodel->nodes[n].name);
					m = g_bonetable[k].parent;

					if (n != m)
					{
						printf("illegal parent bone replacement in model \"%s\"\n\t\"%s\" has \"%s\", previously was \"%s\"\n",
							   submodel->name.c_str(),
							   submodel->nodes[j].name.c_str(),
							   (n != -1) ? g_bonetable[n].name.c_str() : "ROOT",
							   (m != -1) ? g_bonetable[m].name.c_str() : "ROOT");
						iError++;
					}
				}
				submodel->bonemap[j] = k;
				submodel->boneimap[k] = j;
			}
		}
	}

	if (iError)
	{
		exit(1);
	}

	if (g_bonetable.size() >= MAXSTUDIOBONES)
	{
		error("Too many bones used in model, used " + std::to_string(g_bonetable.size()) + ", max " + std::to_string(MAXSTUDIOBONES) + "\n");
	}

	// rename sequence bones if needed TODO: rename_sequence_bones()
	for (auto& sequence : qc.sequences)
	{
		for (auto& node : sequence.anims[0].nodes)
		{
			for (auto& rename : qc.renamebones)
			{
				if (node.name == rename.from)
				{
					node.name = rename.to;
					break;
				}
			}
		}
	}

	// map each sequences bone list to master list TODO: map_sequence_bones()
	for (auto& sequence : qc.sequences)
	{
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			sequence.anims[0].boneimap[k] = -1;
		}
		int j = 0;
		for (auto& node : sequence.anims[0].nodes)
		{
			k = find_node(node.name);

			if (k == -1)
			{
				continue;
			}
			else
			{
				std::string szAnim = "ROOT";
				std::string szNode = "ROOT";

				// whoa, check parent connections!
				if (node.parent != -1)
					szAnim = (sequence.anims[0].nodes[node.parent].name);

				if (g_bonetable[k].parent != -1)
					szNode = g_bonetable[g_bonetable[k].parent].name;

				if (!case_insensitive_compare(szAnim, szNode))
				{
					printf("illegal parent bone replacement in sequence \"%s\"\n\t\"%s\" has \"%s\", reference has \"%s\"\n",
						   sequence.name.c_str(),
						   node.name.c_str(),
						   szAnim.c_str(),
						   szNode.c_str());
					iError++;
				}
				sequence.anims[0].boneimap[k] = j;
			}
			j++;
		}
	}
	if (iError)
	{
		exit(1);
	}

	// link bonecontrollers TODO: link_bone_controllers()
	for (auto& bonecontroller : qc.bonecontrollers)
	{
		int j = 0;
		for (auto& bone : g_bonetable)
		{
			if (bonecontroller.name == bone.name)
				break;
			++j; // before or after if statement ?
		}

		if (j >= g_bonetable.size())
		{
			error("unknown bonecontroller link '" + bonecontroller.name + "'\n");
		}
		bonecontroller.bone = j;
	}


	// link attachments TODO: link_attachments()
	for (auto& attachment : qc.attachments)
	{
		int j = 0;
		for (auto& bone : g_bonetable)
		{
			if (attachment.bonename == bone.name)
				break;
			++j; // before or after if statement ?
		}
		if (j >= g_bonetable.size())
		{
			error("unknown attachment link '" + attachment.bonename + "'\n");
		}
		attachment.bone = j;
	}

	// relink model TODO: relink_model()
	for (auto& submodel : qc.submodels)
	{
		for (auto& vert : submodel->verts)
		{
			vert.bone_id = submodel->bonemap[vert.bone_id];
		}

		for (auto& normal : submodel->normals)
		{
			normal.bone_id = submodel->bonemap[normal.bone_id];
		}
	}

	// set hitgroups TODO: set_hit_groups()
	for (auto& bone_table : g_bonetable)
	{
		bone_table.group = -9999;
	}
	for (auto& hitgroup : qc.hitgroups)
	{
		int k = 0;
		for (auto& bone : g_bonetable)
		{
			if (bone.name == hitgroup.name)
			{
				bone.group = hitgroup.group;
				break;
			}
			++k;
		}
		if (k >= g_bonetable.size())
			error("cannot find bone " + hitgroup.name + " for hitgroup " + std::to_string(hitgroup.group) + "\n");
	}
	for (auto& bone : g_bonetable)
	{
		if (bone.group == -9999)
		{
			if (bone.parent != -1)
				bone.group = g_bonetable[bone.parent].group;
			else
				bone.group = 0;
		}
	}

	// TODO: find_or_create_hitboxes()
	if (qc.hitboxes.empty())
	{
		// find intersection box volume for each bone
		for (auto& bone : g_bonetable)
		{
			for (j = 0; j < 3; j++)
			{
				bone.bmin[j] = 0.0;
				bone.bmax[j] = 0.0;
			}
		}
		// try all the connect vertices
		for (auto& submodel : qc.submodels)
		{
			Vector3 p;
			for (auto& vert : submodel->verts)
			{
				p = vert.pos;
				k = vert.bone_id;

				if (p[0] < g_bonetable[k].bmin[0])
					g_bonetable[k].bmin[0] = p[0];
				if (p[1] < g_bonetable[k].bmin[1])
					g_bonetable[k].bmin[1] = p[1];
				if (p[2] < g_bonetable[k].bmin[2])
					g_bonetable[k].bmin[2] = p[2];
				if (p[0] > g_bonetable[k].bmax[0])
					g_bonetable[k].bmax[0] = p[0];
				if (p[1] > g_bonetable[k].bmax[1])
					g_bonetable[k].bmax[1] = p[1];
				if (p[2] > g_bonetable[k].bmax[2])
					g_bonetable[k].bmax[2] = p[2];
			}
		}
		// add in all your children as well
		for (auto& bone : g_bonetable)
		{
			int j = bone.parent;
			if (j != -1)
			{
				if (bone.pos[0] < g_bonetable[j].bmin[0])
					g_bonetable[j].bmin[0] = bone.pos[0];
				if (bone.pos[1] < g_bonetable[j].bmin[1])
					g_bonetable[j].bmin[1] = bone.pos[1];
				if (bone.pos[2] < g_bonetable[j].bmin[2])
					g_bonetable[j].bmin[2] = bone.pos[2];
				if (bone.pos[0] > g_bonetable[j].bmax[0])
					g_bonetable[j].bmax[0] = bone.pos[0];
				if (bone.pos[1] > g_bonetable[j].bmax[1])
					g_bonetable[j].bmax[1] = bone.pos[1];
				if (bone.pos[2] > g_bonetable[j].bmax[2])
					g_bonetable[j].bmax[2] = bone.pos[2];
			}
		}

		int k = 0;
		for (auto& bone : g_bonetable)
		{
			if (bone.bmin[0] < bone.bmax[0] - 1 && bone.bmin[1] < bone.bmax[1] - 1 && bone.bmin[2] < bone.bmax[2] - 1)
			{
				HitBox genhbox;
				genhbox.bone = k;
				genhbox.group = bone.group;
				genhbox.bmin = bone.bmin;
				genhbox.bmax = bone.bmax;
				qc.hitboxes.push_back(genhbox);
			}
			k++;
		}
	}
	else
	{
		for (auto& hitbox : qc.hitboxes)
		{
			int k = 0;
			for (auto& bone : g_bonetable)
			{
				if (bone.name == hitbox.name)
				{
					hitbox.bone = k;
					break;
				}
				k++;
			}
			if (k >= g_bonetable.size())
				error("cannot find bone " + hitbox.name + " for bbox\n");
		}
	}

	// relink animations TODO: relink_animations()
	for (auto& sequence : qc.sequences)
	{
		Vector3 *origpos[MAXSTUDIOSRCBONES] = {nullptr};
		Vector3 *origrot[MAXSTUDIOSRCBONES] = {nullptr};

		for (q = 0; q < sequence.anims.size(); q++)
		{
			// save pointers to original animations
			for (j = 0; j < sequence.anims[q].nodes.size(); j++)
			{
				origpos[j] = sequence.anims[q].pos[j];
				origrot[j] = sequence.anims[q].rot[j];
			}

			for (j = 0; j < g_bonetable.size(); j++)
			{
				if ((k = sequence.anims[0].boneimap[j]) >= 0)
				{
					// link to original animations
					sequence.anims[q].pos[j] = origpos[k];
					sequence.anims[q].rot[j] = origrot[k];
				}
				else
				{
					// link to dummy animations
					sequence.anims[q].pos[j] = defaultpos[j];
					sequence.anims[q].rot[j] = defaultrot[j];
				}
			}
		}
	}

	// find scales for all bones TODO: find_bone_scales()
	for (j = 0; j < g_bonetable.size(); j++)
	{
		for (k = 0; k < DEGREESOFFREEDOM; k++)
		{
			float minv, maxv, scale;

			if (k < 3)
			{
				minv = -128.0;
				maxv = 128.0;
			}
			else
			{
				minv = -Q_PI / 8.0;
				maxv = Q_PI / 8.0;
			}

			for (auto& sequence : qc.sequences)
			{
				for (auto& anim : sequence.anims)
				{
					for (n = 0; n < sequence.numframes; n++)
					{
						float v;
						switch (k)
						{
						case 0:
						case 1:
						case 2:
							v = (anim.pos[j][n][k] - g_bonetable[j].pos[k]);
							break;
						case 3:
						case 4:
						case 5:
							v = (anim.rot[j][n][k - 3] - g_bonetable[j].rot[k - 3]);
							if (v >= Q_PI)
								v -= Q_PI * 2;
							if (v < -Q_PI)
								v += Q_PI * 2;
							break;
						}
						if (v < minv)
							minv = v;
						if (v > maxv)
							maxv = v;
					}
				}
			}
			if (minv < maxv)
			{
				if (-minv > maxv)
				{
					scale = minv / -32768.0f;
				}
				else
				{
					scale = maxv / 32767.0f;
				}
			}
			else
			{
				scale = 1.0 / 32.0;
			}
			switch (k)
			{
			case 0:
			case 1:
			case 2:
				g_bonetable[j].posscale[k] = scale;
				break;
			case 3:
			case 4:
			case 5:
				g_bonetable[j].rotscale[k - 3] = scale;
				break;
			}
		}
	}

	// find bounding box for each sequence TODO: find_sequence_bounding_boxes()
	for (auto& sequence : qc.sequences)
	{
		Vector3 bmin, bmax;

		// find intersection box volume for each bone
		for (j = 0; j < 3; j++)
		{
			bmin[j] = 9999.0;
			bmax[j] = -9999.0;
		}

		for (auto& anim : sequence.anims)
		{
			for (n = 0; n < sequence.numframes; n++)
			{
				Matrix3x4 bonetransform[MAXSTUDIOBONES]; // bone transformation matrix
				Matrix3x4 bonematrix;					   // local transformation matrix
				Vector3 pos;
				int j = 0;
				for (auto& bone : g_bonetable)
				{

					Vector3 angles;
					angles.x = to_degrees(anim.rot[j][n][0]);
					angles.y = to_degrees(anim.rot[j][n][1]);
					angles.z = to_degrees(anim.rot[j][n][2]);

					bonematrix = angle_matrix(angles);

					bonematrix[0][3] = anim.pos[j][n][0];
					bonematrix[1][3] = anim.pos[j][n][1];
					bonematrix[2][3] = anim.pos[j][n][2];

					if (bone.parent == -1)
					{
						matrix_copy(bonematrix, bonetransform[j]);
					}
					else
					{
						 bonetransform[j] = concat_transforms(bonetransform[bone.parent], bonematrix);
					}
					j++;
				}

				for (auto& submodel : qc.submodels)
				{
					for (auto& vert : submodel->verts)
					{
						pos = vector_transform(vert.pos, bonetransform[vert.bone_id]);

						if (pos[0] < bmin[0]) bmin[0] = pos[0];
						if (pos[1] < bmin[1]) bmin[1] = pos[1];
						if (pos[2] < bmin[2]) bmin[2] = pos[2];
						if (pos[0] > bmax[0]) bmax[0] = pos[0];
						if (pos[1] > bmax[1]) bmax[1] = pos[1];
						if (pos[2] > bmax[2]) bmax[2] = pos[2];
					}
				}
			}
		}

		sequence.bmin = bmin;
		sequence.bmax = bmax;
	}

	// reduce animations TODO: reduce_animations()
	{
		int changes = 0;
		int p;

		for (auto& sequence : qc.sequences)
		{
			for (auto& anim : sequence.anims)
			{
				for (j = 0; j < g_bonetable.size(); j++)
				{
					for (k = 0; k < DEGREESOFFREEDOM; k++)
					{
						float v;
						short value[MAXSTUDIOANIMATIONS];
						StudioAnimationValue data[MAXSTUDIOANIMATIONS];

						for (n = 0; n < sequence.numframes; n++)
						{
							switch (k)
							{
							case 0:
							case 1:
							case 2:
								value[n] = static_cast<short>((anim.pos[j][n][k] - g_bonetable[j].pos[k]) / g_bonetable[j].posscale[k]);
								break;
							case 3:
							case 4:
							case 5:
								v = (anim.rot[j][n][k - 3] - g_bonetable[j].rot[k - 3]);
								if (v >= Q_PI)
									v -= Q_PI * 2;
								if (v < -Q_PI)
									v += Q_PI * 2;

								value[n] = static_cast<short>(v / g_bonetable[j].rotscale[k - 3]);
								break;
							}
						}
						if (n == 0)
							error("no animation frames: " + sequence.name + "\n");

						anim.numanim[j][k] = 0;

						std::memset(data, 0, sizeof(data));
						StudioAnimationValue *pcount = data;
						StudioAnimationValue *pvalue = pcount + 1;

						pcount->num.valid = 1;
						pcount->num.total = 1;
						pvalue->value = value[0];
						pvalue++;

						for (m = 1, p = 0; m < n; m++)
						{
							if (abs(value[p] - value[m]) > 1600)
							{
								changes++;
								p = m;
							}
						}

						// this compression algorithm needs work

						for (m = 1; m < n; m++)
						{
							if (pcount->num.total == 255)
							{
								// too many, force a new entry
								pcount = pvalue;
								pvalue = pcount + 1;
								pcount->num.valid++;
								pvalue->value = value[m];
								pvalue++;
							}
							// insert value if they're not equal,
							// or if we're not on a run and the run is less than 3 units
							else if ((value[m] != value[m - 1]) || ((pcount->num.total == pcount->num.valid) && ((m < n - 1) && value[m] != value[m + 1])))
							{
								if (pcount->num.total != pcount->num.valid)
								{
									pcount = pvalue;
									pvalue = pcount + 1;
								}
								pcount->num.valid++;
								pvalue->value = value[m];
								pvalue++;
							}
							pcount->num.total++;
						}

						anim.numanim[j][k] = static_cast<int>(pvalue - data);
						if (anim.numanim[j][k] == 2 && value[0] == 0)
						{
							anim.numanim[j][k] = 0;
						}
						else
						{
							anim.anims[j][k] = (StudioAnimationValue *)std::calloc(pvalue - data, sizeof(StudioAnimationValue));
							std::memcpy(anim.anims[j][k], data, (pvalue - data) * sizeof(StudioAnimationValue));
						}
					}
				}
			}
		}
	}
}

int lookup_control(const char *string)
{
	if (case_insensitive_compare(string, "X"))
		return STUDIO_X;
	if (case_insensitive_compare(string, "Y"))
		return STUDIO_Y;
	if (case_insensitive_compare(string, "Z"))
		return STUDIO_Z;
	if (case_insensitive_compare(string, "XR"))
		return STUDIO_XR;
	if (case_insensitive_compare(string, "YR"))
		return STUDIO_YR;
	if (case_insensitive_compare(string, "ZR"))
		return STUDIO_ZR;
	if (case_insensitive_compare(string, "LX"))
		return STUDIO_LX;
	if (case_insensitive_compare(string, "LY"))
		return STUDIO_LY;
	if (case_insensitive_compare(string, "LZ"))
		return STUDIO_LZ;
	if (case_insensitive_compare(string, "AX"))
		return STUDIO_AX;
	if (case_insensitive_compare(string, "AY"))
		return STUDIO_AY;
	if (case_insensitive_compare(string, "AZ"))
		return STUDIO_AZ;
	if (case_insensitive_compare(string, "AXR"))
		return STUDIO_AXR;
	if (case_insensitive_compare(string, "AYR"))
		return STUDIO_AYR;
	if (case_insensitive_compare(string, "AZR"))
		return STUDIO_AZR;
	return -1;
}

int find_texture_index(std::string texturename)
{
	int i = 0;
	for (auto& texture : g_textures)
	{
		if (texture.name == texturename)
		{
			return i;
		}
		i++;
	}
	Texture newtexture;
	newtexture.name = texturename;

    std::string lower_texname = texturename;
    std::transform(lower_texname.begin(), lower_texname.end(), lower_texname.begin(), ::tolower);
	if (lower_texname.find("chrome") != std::string::npos)
		newtexture.flags = STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME;
	else if (lower_texname.find("bright") != std::string::npos)
		newtexture.flags = STUDIO_NF_FLATSHADE | STUDIO_NF_FULLBRIGHT;
	else
		newtexture.flags = 0;

	g_textures.push_back(newtexture);
	return i;
}

Mesh *find_mesh_by_texture(Model *pmodel, char *texturename)
{
	int i;
	int j = find_texture_index(std::string(texturename));

	for (i = 0; i < pmodel->nummesh; i++)
	{
		if (pmodel->pmeshes[i]->skinref == j)
		{
			return pmodel->pmeshes[i];
		}
	}

	if (i >= MAXSTUDIOMESHES)
	{
		error("too many textures in model: \"" + std::string(pmodel->name) + "\"\n");
	}

	pmodel->nummesh = i + 1;
	pmodel->pmeshes[i] = (Mesh *)std::calloc(1, sizeof(Mesh));
	pmodel->pmeshes[i]->skinref = j;

	return pmodel->pmeshes[i];
}

TriangleVert *find_mesh_triangle_by_index(Mesh *pmesh, int index)
{
	if (index >= pmesh->alloctris)
	{
		int start = pmesh->alloctris;
		pmesh->alloctris = index + 256;
		if (pmesh->triangles)
		{
			pmesh->triangles = static_cast<TriangleVert(*)[3]>(realloc(pmesh->triangles, pmesh->alloctris * sizeof(*pmesh->triangles)));
			std::memset(&pmesh->triangles[start], 0, (pmesh->alloctris - start) * sizeof(*pmesh->triangles));
		}
		else
		{
			pmesh->triangles = static_cast<TriangleVert(*)[3]>(std::calloc(pmesh->alloctris, sizeof(*pmesh->triangles)));
		}
	}

	return pmesh->triangles[index];
}

int find_vertex_normal_index(Model *pmodel, Normal *pnormal)
{
	int i = 0;
	for (auto& normal : pmodel->normals)
	{
		if (normal.pos.dot(pnormal->pos) > g_flagnormalblendangle && normal.bone_id == pnormal->bone_id && normal.skinref == pnormal->skinref)
		{
			return i;
		}
		i++;
	}
	if (i >= MAXSTUDIOVERTS)
	{
		error("too many normals in model: \"" + std::string(pmodel->name) + "\"\n");
	}
	pmodel->normals.push_back(*pnormal);
	return i;
}

int find_vertex_index(Model *pmodel, Vertex *pv)
{
	int i = 0;
	// assume 2 digits of accuracy
	pv->pos[0] = static_cast<int>(pv->pos[0] * 100.0f) / 100.0f;
	pv->pos[1] = static_cast<int>(pv->pos[1] * 100.0f) / 100.0f;
	pv->pos[2] = static_cast<int>(pv->pos[2] * 100.0f) / 100.0f;

	for (auto& vert : pmodel->verts)
	{
		if ((vert.pos == pv->pos) && vert.bone_id == pv->bone_id)
		{
			return i;
		}
		i++;
	}
    if (pmodel->verts.size() >= MAXSTUDIOVERTS)
    {
        error("too many vertices in model: \"" + pmodel->name + "\"\n");
    }
	pmodel->verts.push_back(*pv);
	return i;
}

// Called for the base frame
void texture_coord_ranges(Mesh *pmesh, Texture *ptexture)
{
	int i, j;

	if (ptexture->flags & STUDIO_NF_CHROME)
	{
		ptexture->skintop = 0;
		ptexture->skinleft = 0;
		ptexture->skinwidth = (ptexture->srcwidth + 3) & ~3;
		ptexture->skinheight = ptexture->srcheight;

		for (i = 0; i < pmesh->numtris; i++)
		{
			for (j = 0; j < 3; j++)
			{
				pmesh->triangles[i][j].s = 0;
				pmesh->triangles[i][j].t = +1; // ??
			}
			ptexture->max_s = 63;
			ptexture->min_s = 0;
			ptexture->max_t = 63;
			ptexture->min_t = 0;
		}
		return;
	}

	// convert to pixel coordinates
	for (i = 0; i < pmesh->numtris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			// round to fix UV shift
			pmesh->triangles[i][j].s = static_cast<int>(std::round(pmesh->triangles[i][j].u * static_cast<float>(ptexture->srcwidth)));
			pmesh->triangles[i][j].t = static_cast<int>(std::round(pmesh->triangles[i][j].v * static_cast<float>(ptexture->srcheight)));
		}
	}

	// find the range
	ptexture->max_s = static_cast<float>(ptexture->srcwidth);
	ptexture->min_s = 0;
	ptexture->max_t = static_cast<float>(ptexture->srcheight);
	ptexture->min_t = 0;
}

void reset_texture_coord_ranges(Mesh *pmesh, Texture *ptexture)
{
	// adjust top, left edge
	for (int i = 0; i < pmesh->numtris; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			pmesh->triangles[i][j].t = -pmesh->triangles[i][j].t + ptexture->srcheight - static_cast<int>(ptexture->min_t);
		}
	}
}

void grab_bmp(const char *filename, Texture *ptexture)
{
	if (int result = load_bmp(filename, &ptexture->ppicture, (std::uint8_t **)&ptexture->ppal, &ptexture->srcwidth, &ptexture->srcheight))
	{
		error("error " + std::to_string(result) + " reading BMP image \"" + filename + "\"\n");
	}
}

void resize_texture(QC &qc, Texture *ptexture)
{
	int i, t;

	// Keep the original texture without resizing to avoid uv shift
	ptexture->skintop = static_cast<int>(ptexture->min_t);
	ptexture->skinleft = static_cast<int>(ptexture->min_s);
	ptexture->skinwidth = ptexture->srcwidth;
	ptexture->skinheight = ptexture->srcheight;

	ptexture->size = ptexture->skinwidth * ptexture->skinheight + 256 * 3;

	printf("BMP %s [%d %d] (%.0f%%)  %6d bytes\n", ptexture->name.c_str(), ptexture->skinwidth, ptexture->skinheight,
		   (static_cast<float>(ptexture->skinwidth * ptexture->skinheight) / static_cast<float>(ptexture->srcwidth * ptexture->srcheight)) * 100.0f,
		   ptexture->size);

	if (ptexture->size > 1024 * 1024)
	{
		printf("%.0f %.0f %.0f %.0f\n", ptexture->min_s, ptexture->max_s, ptexture->min_t, ptexture->max_t);
		error("texture too large\n");
	}
	std::uint8_t *pdest = (std::uint8_t *)malloc(ptexture->size);
	ptexture->pdata = pdest;

	// Data is saved as a multiple of 4
	int srcadjustedwidth = (ptexture->srcwidth + 3) & ~3;

	// Move the picture data to the model area, replicating missing data, deleting unused data.
	for (i = 0, t = ptexture->srcheight - ptexture->skinheight - ptexture->skintop + 10 * ptexture->srcheight; i < ptexture->skinheight; i++, t++)
	{
		while (t >= ptexture->srcheight)
			t -= ptexture->srcheight;
		while (t < 0)
			t += ptexture->srcheight;
		for (int j = 0, s = ptexture->skinleft + 10 * ptexture->srcwidth; j < ptexture->skinwidth; j++, s++)
		{
			while (s >= ptexture->srcwidth)
				s -= ptexture->srcwidth;
			*(pdest++) = *(ptexture->ppicture + s + t * srcadjustedwidth);
		}
	}

	// TODO: process the texture and flag it if fullbright or transparent are used.
	// TODO: only save as many palette entries as are actually used.
	if (qc.gamma != 1.8f)
	// gamma correct the monster textures to a gamma of 1.8
	{
		std::uint8_t *psrc = (std::uint8_t *)ptexture->ppal;
		float g = qc.gamma / 1.8f;
		for (i = 0; i < 768; i++)
		{
			pdest[i] = static_cast<uint8_t>(std::round(pow(psrc[i] / 255.0, g) * 255));
		}
	}
	else
	{
		memcpy(pdest, ptexture->ppal, 256 * sizeof(RGB));
	}

	free(ptexture->ppicture);
	free(ptexture->ppal);
}

void grab_skin(QC &qc, Texture *ptexture)
{
	std::filesystem::path texture_file_path = (qc.cdtexture / ptexture->name).lexically_normal();
	if (!std::filesystem::exists(texture_file_path))
	{
		error("Cannot find \"" + ptexture->name + "\" texture in \"" + qc.cdtexture.string() + "\" or path does not exist\n");
	}
	if (texture_file_path.extension() == ".bmp")
	{
		grab_bmp(texture_file_path.string().c_str(), ptexture);
	}
	else
	{
		error("Not supported texture format: \"" + texture_file_path.string() + "\"\n");
	}
}

void set_skin_values(QC &qc)
{
	int i, j;

	for (auto& texture : g_textures)
	{
		grab_skin(qc, &texture);

		texture.max_s = -9999999;
		texture.min_s = 9999999;
		texture.max_t = -9999999;
		texture.min_t = 9999999;
	}

	for (auto& submodel : qc.submodels)
	{
		for (j = 0; j < submodel->nummesh; j++)
		{
			texture_coord_ranges(submodel->pmeshes[j], &g_textures[submodel->pmeshes[j]->skinref]);
		}
	}

	for (auto& texture : g_textures)
	{
		if (texture.max_s < texture.min_s)
		{
			// must be a replacement texture
			if (texture.flags & STUDIO_NF_CHROME)
			{
				texture.max_s = 63;
				texture.min_s = 0;
				texture.max_t = 63;
				texture.min_t = 0;
			}
			else
			{
				texture.max_s = g_textures[texture.parent].max_s;
				texture.min_s = g_textures[texture.parent].min_s;
				texture.max_t = g_textures[texture.parent].max_t;
				texture.min_t = g_textures[texture.parent].min_t;
			}
		}

		resize_texture(qc, &texture);
	}

	for (auto& submodel : qc.submodels)
	{
		for (j = 0; j < submodel->nummesh; j++)
		{
			reset_texture_coord_ranges(submodel->pmeshes[j], &g_textures[submodel->pmeshes[j]->skinref]);
		}
	}

	// build texture groups
	for (i = 0; i < MAXSTUDIOSKINS; i++)
	{
		for (j = 0; j < MAXSTUDIOSKINS; j++)
		{
			g_skinref[i][j] = j;
		}
	}
	for (i = 0; i < qc.texturegroup_rows; i++)
	{
		for (j = 0; j < qc.texturegroup_cols; j++)
		{
			g_skinref[i][qc.texturegroups[0][j]] = qc.texturegroups[i][j];
		}
	}
	if (i != 0)
	{
		g_skinfamiliescount = i;
	}
	else
	{
		g_skinfamiliescount = 1;
		g_skinrefcount = g_textures.size();
	}
}

void build_reference(Model *pmodel)
{
	Vector3 bone_angles{};
	
	for (int i = 0; i < pmodel->nodes.size(); i++)
	{
		// convert to degrees
		bone_angles[0] = to_degrees(pmodel->skeleton[i].rot[0]);
		bone_angles[1] = to_degrees(pmodel->skeleton[i].rot[1]);
		bone_angles[2] = to_degrees(pmodel->skeleton[i].rot[2]);

		int parent = pmodel->nodes[i].parent;
		if (parent == -1)
		{
			// scale the done pos.
			// calc rotational matrices
			g_bonefixup[i].matrix = angle_matrix(bone_angles);
			g_bonefixup[i].inv_matrix = angle_i_matrix(bone_angles);
			g_bonefixup[i].worldorg = pmodel->skeleton[i].pos;
		}
		else
		{
			// calc compound rotational matrices
			// FIXME : Hey, it's orthogical so inv(A) == transpose(A)
			Matrix3x4 rotation_matrix = angle_matrix(bone_angles);
			g_bonefixup[i].matrix = concat_transforms(g_bonefixup[parent].matrix, rotation_matrix);
			rotation_matrix = angle_i_matrix(bone_angles);
			g_bonefixup[i].inv_matrix = concat_transforms(rotation_matrix, g_bonefixup[parent].inv_matrix);

			// calc true world coord.
			Vector3 true_pos = vector_transform(pmodel->skeleton[i].pos, g_bonefixup[parent].matrix);
			g_bonefixup[i].worldorg = true_pos + g_bonefixup[parent].worldorg;
		}
	}
}

void parse_smd_triangles(QC &qc, Model *pmodel)
{
	int i;
	Vector3 vmin{99999, 99999, 99999};
	Vector3 vmax{-99999, -99999, -99999};

	build_reference(pmodel);

	// load the base triangles
	while (true)
	{
		if (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != nullptr)
		{
			Mesh *pmesh;
			char triangleMaterial[64];
			TriangleVert *ptriangleVert;
			int parentBone;

			Vector3 triangleVertices[3];
			Vector3 triangleNormals[3];

			g_smdlinecount++;

			if (case_insensitive_compare("end\n", g_currentsmdline))
				return;

			// strip off trailing smag
			std::strcpy(triangleMaterial, g_currentsmdline);
			for (i = static_cast<int>(strlen(triangleMaterial)) - 1; i >= 0 && !isgraph(triangleMaterial[i]); i--)
				;
			triangleMaterial[i + 1] = '\0';

			// funky texture overrides
			for (i = 0; i < g_numtextureteplacements; i++)
			{
				if (g_sourcetexture[i][0] == '\0')
				{
					std::strcpy(triangleMaterial, g_defaulttextures[i]);
					break;
				}
				if (case_insensitive_compare(triangleMaterial, g_sourcetexture[i]))
				{
					std::strcpy(triangleMaterial, g_defaulttextures[i]);
					break;
				}
			}

			if (triangleMaterial[0] == '\0')
			{
				// weird model problem, skip them
				fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile);
				fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile);
				fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile);
				g_smdlinecount += 3;
				continue;
			}

			pmesh = find_mesh_by_texture(pmodel, triangleMaterial);

			for (int j = 0; j < 3; j++)
			{
				if (g_flaginvertnormals)
					ptriangleVert = find_mesh_triangle_by_index(pmesh, pmesh->numtris) + j;
				else // quake wants them in the reverse order
					ptriangleVert = find_mesh_triangle_by_index(pmesh, pmesh->numtris) + 2 - j;

				if (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != nullptr)
				{
					Vertex triangleVertex;
					Normal triangleNormal;

					g_smdlinecount++;
					if (sscanf(g_currentsmdline, "%d %f %f %f %f %f %f %f %f",
							   &parentBone,
							   &triangleVertex.pos[0], &triangleVertex.pos[1], &triangleVertex.pos[2],
							   &triangleNormal.pos[0], &triangleNormal.pos[1], &triangleNormal.pos[2],
							   &ptriangleVert->u, &ptriangleVert->v) == 9)
					{
						if (parentBone < 0 || parentBone >= pmodel->nodes.size())
						{
							fprintf(stderr, "bogus bone index\n");
							fprintf(stderr, "%d %s :\n%s", g_smdlinecount, g_smdpath.c_str(), g_currentsmdline);
							exit(1);
						}

						triangleVertices[j] = triangleVertex.pos;
						triangleNormals[j] = triangleNormal.pos;

						triangleVertex.bone_id = parentBone;
						triangleNormal.bone_id = parentBone;
						triangleNormal.skinref = pmesh->skinref;

						if (triangleVertex.pos[2] < vmin[2])
							vmin[2] = triangleVertex.pos[2];

						triangleVertex.pos -= qc.sequenceOrigin; // adjust vertex to origin
						triangleVertex.pos *= qc.scaleBodyAndSequenceOption; // scale vertex

						// move vertex position to object space.
						Vector3 tmp;
						tmp = triangleVertex.pos - g_bonefixup[triangleVertex.bone_id].worldorg;
						triangleVertex.pos = vector_transform(tmp, g_bonefixup[triangleVertex.bone_id].inv_matrix);

						// move normal to object space.
						tmp = triangleNormal.pos;
						triangleNormal.pos = vector_transform(tmp, g_bonefixup[triangleVertex.bone_id].inv_matrix);
						triangleNormal.pos.normalize();

						ptriangleVert->normindex = find_vertex_normal_index(pmodel, &triangleNormal);
						ptriangleVert->vertindex = find_vertex_index(pmodel, &triangleVertex);
					}
					else
					{
						error("Error on line " + std::to_string(g_smdlinecount) + ": " + g_currentsmdline);
					}
				}
			}

			pmesh->numtris++;
		}
		else
		{
			break;
		}
	}

	if (vmin[2] != 0.0)
	{
		printf("lowest vector at %f\n", vmin[2]);
	}
}

void parse_smd_reference_skeleton(QC &qc, std::vector<Node> &nodes, std::vector<Bone> &bones, std::filesystem::path &path)
{
    std::ifstream smdstream(path);
    std::string line, cmd;
    int node;
    float posX, posY, posZ, rotX, rotY, rotZ;

    while (std::getline(smdstream, line))
    {
        g_smdlinecount++;
        std::istringstream iss(line);

        if (iss >> node >> posX >> posY >> posZ >> rotX >> rotY >> rotZ)
        {
            bones.emplace_back();
			bones.back().pos = Vector3(posX, posY, posZ);
            bones.back().pos *= qc.scaleBodyAndSequenceOption;

            if (nodes[node].mirrored)
                bones.back().pos *= -1.0;
			bones.back().rot = Vector3(rotX, rotY, rotZ);
            clip_rotations(bones.back().rot);
        }
        else if (iss >> cmd >> node && case_insensitive_compare(cmd, "end"))
        {
            return;
        }
    }
}


int parse_smd_nodes(QC &qc, std::vector<Node> &nodes)
{
    int index;
    char bone_name[1024];
    int parent;

    while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != nullptr)
    {
        g_smdlinecount++;
        if (sscanf(g_currentsmdline, "%d \"%[^\"]\" %d", &index, bone_name, &parent) == 3)
        {
            nodes.emplace_back();
            nodes.back().name = bone_name;
            nodes.back().parent = parent;

            // Check for mirrored bones
            for (int i = 0; i < qc.mirroredbones.size(); i++)
			{
                if (case_insensitive_compare(bone_name, qc.mirroredbones[i].data())) {
                    nodes.back().mirrored = 1;
                }
            }

            if ((!nodes.back().mirrored) && parent != -1) {
                nodes.back().mirrored = nodes[parent].mirrored;
            }
        }
        else
        {
            return 1;
        }
    }
    error("Unexpected EOF at line " + std::to_string(g_smdlinecount) + "\n");
    return 0;
}

void parse_smd_reference(QC &qc, Model *pmodel)
{
	char cmd[1024];
	int option;

    g_smdpath = (qc.cd / (pmodel->name + ".smd")).lexically_normal();

    if (!std::filesystem::exists(g_smdpath))
    {
        error("Cannot find \"" + pmodel->name + ".smd\" in \"" + qc.cd.string() + "\"\n");
    }

	printf("Grabbing reference: %s\n", g_smdpath.string().c_str());

	if ((g_smdfile = fopen(g_smdpath.string().c_str(), "r")) == 0)
	{
		fprintf(stderr, "reader: could not open file '%s'\n", g_smdpath.c_str());
	}
	g_smdlinecount = 0;

	while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != nullptr)
	{
		g_smdlinecount++;
		sscanf(g_currentsmdline, "%s %d", cmd, &option);
		if (case_insensitive_compare(cmd, "version"))
		{
			if (option != 1)
			{
				error("bad version\n");
			}
		}
		else if (case_insensitive_compare(cmd, "nodes"))
		{
			parse_smd_nodes(qc, pmodel->nodes);
		}
		else if (case_insensitive_compare(cmd, "skeleton"))
		{
			parse_smd_reference_skeleton(qc, pmodel->nodes, pmodel->skeleton, g_smdpath);
		}
		else if (case_insensitive_compare(cmd, "triangles"))
		{
			parse_smd_triangles(qc, pmodel);
		}
		else
		{
			printf("unknown studio command\n");
		}
	}
	fclose(g_smdfile);
}

void cmd_eyeposition(QC &qc, std::string &token)
{
	// rotate points into frame of reference so model points down the positive x
	// axis
	get_token(false, token);
	qc.eyeposition[1] = std::stof(token);

	get_token(false, token);
	qc.eyeposition[0] = -std::stof(token);

	get_token(false, token);
	qc.eyeposition[2] = std::stof(token);
}

void cmd_flags(QC &qc, std::string &token)
{
	get_token(false, token);
	qc.flags = std::stoi(token);
}

void cmd_modelname(QC &qc, std::string &token)
{
	get_token(false, token);
	qc.modelname = token;
}


void cmd_body_option_studio(QC &qc, std::string &token)
{
	if (!get_token(false, token))
		return;

	Model* new_submodel = new Model();

	new_submodel->name = token;

	qc.scaleBodyAndSequenceOption = qc.scale;

	while (token_available())
	{
		get_token(false, token);
		if (case_insensitive_compare("reverse", token))
		{
			g_flaginvertnormals = true;
		}
		else if (case_insensitive_compare("scale", token))
		{
			get_token(false, token);
			qc.scaleBodyAndSequenceOption = std::stof(token);
		}
	}

	parse_smd_reference(qc, new_submodel);

	qc.submodels.push_back(new_submodel);
	qc.bodyparts.back().num_submodels++;


	qc.scaleBodyAndSequenceOption = qc.scale;
}

int cmd_body_option_blank(QC &qc)
{
	Model* new_submodel = new Model();

	new_submodel->name = "blank";
	
	qc.submodels.push_back(new_submodel);
	qc.bodyparts.back().num_submodels++;
	return 0;
}

void cmd_bodygroup(QC &qc, std::string &token)
{
	if (!get_token(false, token))
		return;

	BodyPart newbp{};

    if (qc.bodyparts.empty())
    {
        newbp.base = 1;
    }
    else
    {
        BodyPart &lastbp = qc.bodyparts.back();
        newbp.base = lastbp.base * lastbp.num_submodels;
    }

    newbp.name = token;
    qc.bodyparts.push_back(newbp);

	while (true)
	{
		int is_started = 0;
		get_token(true, token);
		if (end_of_qc_file)
			return;

		if (token[0] == '{')
		{
			is_started = 1;
		}
		else if (token[0] == '}')
		{
			break;
		}
		else if (case_insensitive_compare("studio", token))
		{
			cmd_body_option_studio(qc, token);
		}
		else if (case_insensitive_compare("blank", token))
		{
			cmd_body_option_blank(qc);
		}
	}

	return;
}

void cmd_body(QC &qc, std::string &token)
{
	if (!get_token(false, token))
		return;

    BodyPart newbp{};
    newbp.name = token;

    if (qc.bodyparts.empty())
    {
        newbp.base = 1;
    }
    else
    {
        BodyPart &lastbp = qc.bodyparts.back();
        newbp.base = lastbp.base * lastbp.num_submodels;
    }

    qc.bodyparts.push_back(newbp);
    cmd_body_option_studio(qc, token);
}

void parse_smd_animation_skeleton(QC &qc, Animation &anim)
{
	Vector3 pos;
	Vector3 rot;
	char cmd[1024];
	int index;
	int t = -99999999;
	int start = 99999;
	int end = 0;

	for (index = 0; index < anim.nodes.size(); index++)
	{
		anim.pos[index] = (Vector3 *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(Vector3));
		anim.rot[index] = (Vector3 *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(Vector3));
	}

	float cz = cosf(qc.rotate);
	float sz = sinf(qc.rotate);

	while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != nullptr)
	{
		g_smdlinecount++;
		if (sscanf(g_currentsmdline, "%d %f %f %f %f %f %f", &index, &pos[0], &pos[1], &pos[2], &rot[0], &rot[1], &rot[2]) == 7)
		{
			if (t >= anim.startframe && t <= anim.endframe)
			{
				if (anim.nodes[index].parent == -1)
				{
					pos -= qc.sequenceOrigin; // adjust vertex to origin
					anim.pos[index][t][0] = cz * pos[0] - sz * pos[1];
					anim.pos[index][t][1] = sz * pos[0] + cz * pos[1];
					anim.pos[index][t][2] = pos[2];
					// rotate model
					rot[2] += qc.rotate;
				}
				else
				{
					anim.pos[index][t] = pos;
				}
				if (t > end)
					end = t;
				if (t < start)
					start = t;

				if (anim.nodes[index].mirrored)
					anim.pos[index][t] = anim.pos[index][t] * -1.0;

				anim.pos[index][t] *= qc.scaleBodyAndSequenceOption; // scale vertex

				clip_rotations(rot);

				anim.rot[index][t] = rot;
			}
		}
		else if (sscanf(g_currentsmdline, "%s %d", cmd, &index))
		{
			if (case_insensitive_compare(cmd, "time"))
			{
				t = index;
			}
			else if (case_insensitive_compare(cmd, "end"))
			{
				anim.startframe = start;
				anim.endframe = end;
				return;
			}
			else
			{
				error("Error(" + std::to_string(g_smdlinecount) + ") : " + g_currentsmdline);
			}
		}
		else
		{
			error("Error(" + std::to_string(g_smdlinecount) + ") : " + g_currentsmdline);
		}
	}
	error("unexpected EOF: " + std::string(anim.name));
}

void shift_option_animation(Animation &anim)
{
	int size = (anim.endframe - anim.startframe + 1) * sizeof(Vector3);
	for (int j = 0; j < anim.nodes.size(); j++)
	{
		Vector3 *ppos = (Vector3 *)std::calloc(1, size);
		Vector3 *prot = (Vector3 *)std::calloc(1, size);

		std::memcpy(ppos, &anim.pos[j][anim.startframe], size);
		std::memcpy(prot, &anim.rot[j][anim.startframe], size);

		free(anim.pos[j]);
		free(anim.rot[j]);

		anim.pos[j] = ppos;
		anim.rot[j] = prot;
	}
}

void parse_smd_animation(QC &qc, std::string &name, Animation &anim)
{
	char cmd[1024];
	int option;

	anim.name = name;

    g_smdpath = (qc.cd / (anim.name + ".smd")).lexically_normal();
	if (!std::filesystem::exists(g_smdpath))
		error("Cannot find \"" + anim.name + ".smd\" in \"" + qc.cd.string() + "\"\n");

	printf("Grabbing animation: %s\n", g_smdpath.string().c_str());

	if ((g_smdfile = fopen(g_smdpath.string().c_str(), "r")) == 0)
	{
		fprintf(stderr, "reader: could not open file '%s'\n", g_smdpath.c_str());
		error(0);
	}
	g_smdlinecount = 0;

	while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != nullptr)
	{
		g_smdlinecount++;
		sscanf(g_currentsmdline, "%s %d", cmd, &option);
		if (case_insensitive_compare(cmd, "version"))
		{
			if (option != 1)
			{
				error("bad version\n");
			}
		}
		else if (case_insensitive_compare(cmd, "nodes"))
		{
			parse_smd_nodes(qc, anim.nodes);
		}
		else if (case_insensitive_compare(cmd, "skeleton"))
		{
			parse_smd_animation_skeleton(qc, anim);
			shift_option_animation(anim);
		}
		else
		{
			printf("unknown studio model command : %s\n", cmd);
			while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != nullptr)
			{
				g_smdlinecount++;
				if (strncmp(g_currentsmdline, "end", 3))
					break;
			}
		}
	}
	fclose(g_smdfile);
}

int cmd_sequence_option_event(std::string &token, Sequence &seq)
{
    if (seq.events.size() >= MAXSTUDIOEVENTS)
 	{
		printf("too many events\n");
		exit(0);
	}

    get_token(false, token);
    int event_id;

	event_id = std::stoi(token);

    get_token(false, token);
    int frame;
	frame = std::stoi(token);

    seq.events.emplace_back(Event{event_id, frame, ""});

    if (token_available())
    {
        get_token(false, token);
        if (token[0] == '}')
            return 1;
        seq.events.back().options = token;
    }

    return 0;
}


int cmd_sequence_option_fps(std::string &token, Sequence &seq)
{
	get_token(false, token);
	seq.fps = std::stof(token);

	return 0;
}


void cmd_origin(QC &qc, std::string &token)
{
	get_token(false, token);
	qc.origin[0] = std::stof(token);

	get_token(false, token);
	qc.origin[1] = std::stof(token);

	get_token(false, token);
	qc.origin[2] = std::stof(token);

	if (token_available())
	{
		get_token(false, token);
		qc.originRotation = to_radians(std::stof(token) + ENGINE_ORIENTATION);
	}
}

void cmd_sequence_option_origin(QC &qc, std::string &token)
{
	get_token(false, token);
	qc.sequenceOrigin[0] = std::stof(token);

	get_token(false, token);
	qc.sequenceOrigin[1] = std::stof(token);

	get_token(false, token);
	qc.sequenceOrigin[2] = std::stof(token);
}

void cmd_sequence_option_rotate(QC &qc, std::string &token)
{
	get_token(false, token);
	qc.rotate = to_radians(std::stof(token) + ENGINE_ORIENTATION);
}

void cmd_scale(QC &qc, std::string &token)
{
	get_token(false, token);
	qc.scale = qc.scaleBodyAndSequenceOption = std::stof(token);
}

void cmd_rotate(QC &qc, std::string &token) // XDM
{
	if (!get_token(false, token))
		return;
	qc.rotate = to_radians(std::stof(token) + ENGINE_ORIENTATION);
}

void cmd_sequence_option_scale(QC &qc, std::string &token)
{
	get_token(false, token);
	qc.scaleBodyAndSequenceOption = std::stof(token);
}

int cmd_sequence_option_action(std::string &szActivity)
{
	for (int i = 0; activity_map[i].name; i++)
	{
		if (case_insensitive_compare(szActivity.c_str(), activity_map[i].name))
		{
			return activity_map[i].type;
		}
	}
	if (case_insensitive_n_compare(szActivity.c_str(), "ACT_", 4))
	{
		return std::stoi(&szActivity[4]);
	}
	return 0;
}

int cmd_sequence(QC &qc, std::string &token)
{
	int depth = 0;
	std::vector<std::string> smd_files;
	int i;
	int start = 0;
	int end = MAXSTUDIOANIMATIONS - 1;

	if (!get_token(false, token))
		return 0;

	Sequence newseq = {};

	newseq.name = token;

	qc.sequenceOrigin = qc.origin;
	qc.scaleBodyAndSequenceOption = qc.scale;

	qc.rotate = qc.originRotation;
	newseq.fps = 30.0;
	newseq.seqgroup = 0; // 0 since $sequencegroup is deprecated
	newseq.blendstart[0] = 0.0;
	newseq.blendend[0] = 1.0;

	while (true)
	{
		if (depth > 0)
		{
			if (!get_token(true, token))
			{
				break;
			}
		}
		else
		{
			if (!token_available())
			{
				break;
			}
			else
			{
				get_token(false, token);
			}
		}

		if (end_of_qc_file)
		{
			if (depth != 0)
			{
				printf("missing }\n");
				exit(1);
			}
			return 1;
		}
		if (token == "{")
		{
			depth++;
		}
		else if (token == "}")
		{
			depth--;
		}
		else if (token == "event")
		{
			depth -= cmd_sequence_option_event(token, newseq);
		}
		else if (token == "fps")
		{
			cmd_sequence_option_fps(token, newseq);
		}
		else if (token == "origin")
		{
			cmd_sequence_option_origin(qc, token);
		}
		else if (token == "rotate")
		{
			cmd_sequence_option_rotate(qc, token);
		}
		else if (token == "scale")
		{
			cmd_sequence_option_scale(qc, token);
		}
		else if (token == "loop")
		{
			newseq.flags |= STUDIO_LOOPING;
		}
		else if (token == "frame")
		{
			get_token(false, token);
			start = std::stoi(token);
			get_token(false, token);
			end = std::stoi(token);
		}
		else if (token == "blend")
		{
			get_token(false, token);
			newseq.blendtype[0] = static_cast<float>(lookup_control(token.c_str()));
			get_token(false, token);
			newseq.blendstart[0] = std::stof(token);
			get_token(false, token);
			newseq.blendend[0] = std::stof(token);
		}
		else if (token == "node")
		{
			get_token(false, token);
			newseq.entrynode = newseq.exitnode = std::stoi(token);
		}
		else if (token == "transition")
		{
			get_token(false, token);
			newseq.entrynode = std::stoi(token);
			get_token(false, token);
			newseq.exitnode = std::stoi(token);
		}
		else if (token == "rtransition")
		{
			get_token(false, token);
			newseq.entrynode = std::stoi(token);
			get_token(false, token);
			newseq.exitnode = std::stoi(token);
			newseq.nodeflags |= 1;
		}
		else if (lookup_control(token.c_str()) != -1) // motion flags [motion extraction]
		{
			newseq.motiontype |= lookup_control(token.c_str());
		}
		else if (token == "animation")
		{
			get_token(false, token);
			smd_files.push_back(token);
		}
		else if ((i = cmd_sequence_option_action(token)) != 0)
		{
			newseq.activity = i;
			get_token(false, token);
			newseq.actweight = std::stoi(token);
		}
		else
		{
			smd_files.push_back(token);
		}

		if (depth < 0)
		{
			printf("missing {\n");
			exit(1);
		}
	};

	if (smd_files.empty())
	{
		printf("no animations found\n");
		exit(1);
	}
	for (auto& file : smd_files) // 
	{
		qc.sequenceAnimationOptions.push_back(Animation());
		Animation& newAnim = qc.sequenceAnimationOptions.back();
		
		// crop the SMD animation from start to end
		newAnim.startframe = start;
		newAnim.endframe = end;

		parse_smd_animation(qc, file, newAnim);
		newseq.anims.push_back(newAnim);
	}

	qc.sequences.push_back(newseq);
	return 0;
}

int cmd_controller(QC &qc, std::string &token)
{
	BoneController newbc = {};
	if (get_token(false, token))
	{
		if (token == "mouth")
		{
			newbc.index = 4;
		}
		else
		{
			newbc.index = std::stoi(token);
		}
		if (get_token(false, token))
		{
			newbc.name = token;
			get_token(false, token);
			if ((newbc.type = lookup_control(token.c_str())) == -1)
			{
				printf("unknown bonecontroller type '%s'\n", token.c_str());
				return 0;
			}
			get_token(false, token);
			newbc.start = std::stof(token);
			get_token(false, token);
			newbc.end = std::stof(token);

			if (newbc.type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
			{
				if ((static_cast<int>(newbc.start + 360) % 360) == (static_cast<int>(newbc.end + 360) % 360))
				{
					newbc.type |= STUDIO_RLOOP;
				}
			}
			qc.bonecontrollers.push_back(newbc);
		}
	}
	return 1;
}

void cmd_bbox(QC &qc, std::string &token)
{	// min
	get_token(false, token);
	qc.bbox[0].x = std::stof(token);

	get_token(false, token);
	qc.bbox[0].y = std::stof(token);

	get_token(false, token);
	qc.bbox[0].z = std::stof(token);
	// max
	get_token(false, token);
	qc.bbox[1].x = std::stof(token);

	get_token(false, token);
	qc.bbox[1].y = std::stof(token);

	get_token(false, token);
	qc.bbox[1].z = std::stof(token);
}

void cmd_cbox(QC &qc, std::string &token)
{	// min
	get_token(false, token);
	qc.cbox[0].x = std::stof(token);

	get_token(false, token);
	qc.cbox[0].y = std::stof(token);

	get_token(false, token);
	qc.cbox[0].z = std::stof(token);
	// max
	get_token(false, token);
	qc.cbox[1].x = std::stof(token);

	get_token(false, token);
	qc.cbox[1].y = std::stof(token);

	get_token(false, token);
	qc.cbox[1].z = std::stof(token);
}

void cmd_mirror(QC &qc, std::string &token)
{	
	get_token(false, token);
	std::string bonename = token;
	qc.mirroredbones.push_back(bonename);
}

void cmd_gamma(QC &qc, std::string &token)
{
	get_token(false, token);
	qc.gamma = std::stof(token);
}

int cmd_texturegroup(QC &qc, std::string &token)
{
	int i;
	int depth = 0;
	int col_index = 0;
	int row_index = 0;

	if (g_textures.empty())
		error("texturegroups must follow model loading\n");

	if (!get_token(false, token))
		return 0;

	if (g_skinrefcount == 0)
		g_skinrefcount = g_textures.size();

	while (true)
	{
		if (!get_token(true, token))
		{
			break;
		}

		if (end_of_qc_file)
		{
			if (depth != 0)
			{
				error("missing }\n");
			}
			return 1;
		}
		if (token[0] == '{')
		{
			depth++;
		}
		else if (token[0] == '}')
		{
			depth--;
			if (depth == 0)
				break;
			row_index++;
			col_index = 0;
		}
		else if (depth == 2)
		{
			i = find_texture_index(token);
			qc.texturegroups[row_index][col_index] = i;
			if (row_index != 0)
				g_textures[i].parent = qc.texturegroups[0][col_index];
			col_index++;
			qc.texturegroup_cols = col_index;
			qc.texturegroup_rows = row_index + 1;
		}
	}

	return 0;
}

int cmd_hitgroup(QC &qc, std::string &token)
{
	HitGroup newhg = {};
	get_token(false, token);
	newhg.group = std::stoi(token);
	get_token(false, token);
	newhg.name = token;
	qc.hitgroups.push_back(newhg);

	return 0;
}

int cmd_hitbox(QC &qc, std::string &token)
{
	HitBox newhb = {};
	get_token(false, token);
	newhb.group = std::stoi(token);
	get_token(false, token);
	newhb.name = token;
	get_token(false, token);
	newhb.bmin.x = std::stof(token);
	get_token(false, token);
	newhb.bmin.y = std::stof(token);
	get_token(false, token);
	newhb.bmin.z = std::stof(token);
	get_token(false, token);
	newhb.bmax.x = std::stof(token);
	get_token(false, token);
	newhb.bmax.y = std::stof(token);
	get_token(false, token);
	newhb.bmax.z = std::stof(token);

	qc.hitboxes.push_back(newhb);

	return 0;
}

int cmd_attachment(QC &qc, std::string &token)
{
	Attachment newattach = {};
	// index
	get_token(false, token); // unused

	// bone name
	get_token(false, token);
	newattach.bonename = token;

	// position
	get_token(false, token);
	newattach.org[0] = std::stof(token);
	get_token(false, token);
	newattach.org[1] = std::stof(token);
	get_token(false, token);
	newattach.org[2] = std::stof(token);

	if (token_available())
		get_token(false, token);

	if (token_available())
		get_token(false, token);

	qc.attachments.push_back(newattach);
	return 0;
}

void cmd_renamebone(QC &qc, std::string &token)
{
	RenameBone rename = {};
	get_token(false, token);
	rename.from = token;
	get_token(false, token);
	rename.to = token;
	qc.renamebones.push_back(rename);
}

void cmd_texrendermode(std::string &token)
{
	get_token(false, token);
	std::string tex_name = token;

	get_token(false, token);
	if (token == "additive")
	{
		g_textures[find_texture_index(tex_name)].flags |= STUDIO_NF_ADDITIVE;
	}
	else if (token == "masked")
	{
		g_textures[find_texture_index(tex_name)].flags |= STUDIO_NF_MASKED;
	}
	else if (token == "fullbright")
	{
		g_textures[find_texture_index(tex_name)].flags |= STUDIO_NF_FULLBRIGHT;
	}
	else if (token == "flatshade")
	{
		g_textures[find_texture_index(tex_name)].flags |= STUDIO_NF_FLATSHADE;
	}
	else
		printf("Texture '%s' has unknown render mode '%s'!\n", tex_name.c_str(), token.c_str());
}

void parse_qc_file(std::filesystem::path path, QC &qc)
{
	std::string token;
	bool iscdalreadyset = false;
	bool iscdtexturealreadyset = false;
	while (true)
	{
		// Look for a line starting with a $ command
		while (true)
		{
			get_token(true, token);
			if (end_of_qc_file)
				return;

			if (token[0] == '$')
				break;

			// Skip the rest of the line
			while (token_available())
				get_token(false, token);
		}

		if (token == "$modelname")
		{
			cmd_modelname(qc, token);
		}
  		else if (token == "$cd")
        {
            if (iscdalreadyset)
                error("Two $cd in one model");
            iscdalreadyset = true;

            get_token(false, token);
            std::filesystem::path cd_path(token);
            if (cd_path.is_relative()) 
			{
                qc.cd = std::filesystem::absolute(path.parent_path() / cd_path);
            }
            else 
			{
                qc.cd = std::filesystem::absolute(cd_path);
            }

        }
        else if (token == "$cdtexture")
        {
            if (iscdtexturealreadyset)
                error("Two $cdtexture in one model");
            iscdtexturealreadyset = true;

            get_token(false, token);
            std::filesystem::path cdtexture_path(token);

            if (cdtexture_path.is_relative())
			{
                qc.cdtexture = std::filesystem::absolute(path.parent_path() / cdtexture_path);
            }
            else 
			{
                qc.cdtexture = std::filesystem::absolute(cdtexture_path);
            }
        }
		else if (token == "$scale")
		{
			cmd_scale(qc, token);
		}
		else if (token == "$rotate") // XDM
		{
			cmd_rotate(qc, token);
		}
		else if (token == "$controller")
		{
			cmd_controller(qc, token);
		}
		else if (token == "$body")
		{
			cmd_body(qc, token);
		}
		else if (token == "$bodygroup")
		{
			cmd_bodygroup(qc, token);
		}
		else if (token == "$sequence")
		{
			cmd_sequence(qc, token);
		}
		else if (token == "$eyeposition")
		{
			cmd_eyeposition(qc, token);
		}
		else if (token == "$origin")
		{
			cmd_origin(qc, token);
		}
		else if (token == "$bbox")
		{
			cmd_bbox(qc, token);
		}
		else if (token == "$cbox")
		{
			cmd_cbox(qc, token);
		}
		else if (token == "$mirrorbone")
		{
			cmd_mirror(qc, token);
		}
		else if (token == "$gamma")
		{
			cmd_gamma(qc, token);
		}
		else if (token == "$flags")
		{
			cmd_flags(qc, token);
		}
		else if (token == "$texturegroup")
		{
			cmd_texturegroup(qc, token);
		}
		else if (token == "$hgroup")
		{
			cmd_hitgroup(qc, token);
		}
		else if (token == "$hbox")
		{
			cmd_hitbox(qc, token);
		}
		else if (token == "$attachment")
		{
			cmd_attachment(qc, token);
		}
		else if (token == "$renamebone")
		{
			cmd_renamebone(qc, token);
		}
		else if (token == "$texrendermode")
		{
			cmd_texrendermode(token);
		}
		else
		{
			error("Incorrect/Unsupported command " + token + "\n");
		}
	}
}

void usage(const char* program_name)
{
    std::cerr << "Usage: " << program_name << " <inputfile.qc> <flags>\n"
              << "  Flags:\n"
              << "    [-f]                Invert normals\n"
              << "    [-a <angle>]        Set vertex normal blend angle override\n"
              << "    [-b]                Keep all unused bones\n";
    std::exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    std::filesystem::path qc_input_path, qc_absolute_path;
	std::filesystem::path mdl_output_path;
    static QC qc;

    g_flaginvertnormals = false;
    g_flagkeepallbones = false;
    g_flagnormalblendangle = cosf(to_radians(0));

    if (argc == 1)
    {
        usage(argv[0]);
    }

    const char *ext = strrchr(argv[1], '.');
    if (!ext || strcmp(ext, ".qc") != 0)
    {
        error("The first argument must be a .qc file\n");
    }
    qc_input_path = argv[1];

    for (int i = 2; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            switch (argv[i][1])
            {
            case 'f':
                g_flaginvertnormals = true;
                break;
            case 'a':
                if (i + 1 >= argc)
                {
                    error("Missing value for -a flag.\n");
                }
                i++;
                try
                {
                    g_flagnormalblendangle = cosf(to_radians(std::stof(argv[i])));
                }
                catch (...)
                {
                    error("Invalid value for -a flag. Expected a numeric angle.\n");
                }
                break;
            case 'b':
                g_flagkeepallbones = true;
                break;
            default:
				error("Unknown flag.\n");
            }
        }
        else
        {
            error("Unexpected argument" + std::string(argv[i]) + "\n");
        }
    }
	qc_absolute_path = std::filesystem::absolute(qc_input_path);
    load_qc_file(qc_absolute_path);       
    parse_qc_file(qc_absolute_path, qc); 
    set_skin_values(qc); 
    simplify_model(qc);

	// mdl file is writed in qc parent path
	mdl_output_path = qc_absolute_path.parent_path();
    write_file(mdl_output_path, qc);      

    return 0;
}