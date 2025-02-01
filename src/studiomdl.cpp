// studiomdl.c: generates a studio .mdl file from a .qc script
// models/<scriptname>.mdl.

#include <cstring>
#include <string>
#include <filesystem>
#include <algorithm>


#include "studiomdl.hpp"
#include "utils/cmdlib.hpp"
#include "utils/mathlib.hpp"
#include "format/image/bmp.hpp"
#include "format/mdl.hpp"
#include "format/qc.hpp"
#include "writemdl.hpp"

#include "monsters/activity.hpp"
#include "monsters/activitymap.hpp"

#define strnicmp strncasecmp
#define stricmp strcasecmp
#define strcpyn(a, b) std::strncpy(a, b, sizeof(a))

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

std::array<BoneTable, MAXSTUDIOSRCBONES> g_bonetable;
int g_bonescount;

std::array<Texture, MAXSTUDIOSKINS> g_textures;
int g_texturescount;

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

void extract_motion(QC &qc_cmd)
{
	int i, j, k;
	int blendIndex;

	// extract linear motion
	for (i = 0; i < g_num_sequence; i++)
	{
		if (qc_cmd.sequence[i].numframes > 1)
		{
			// assume 0 for now.
			Vector3 motion{0, 0, 0};
			int typeMotion = qc_cmd.sequence[i].motiontype;
			Vector3 *ptrPos = qc_cmd.sequence[i].panim[0]->pos[0];

			k = qc_cmd.sequence[i].numframes - 1;

			if (typeMotion & STUDIO_LX)
				motion[0] = ptrPos[k][0] - ptrPos[0][0];
			if (typeMotion & STUDIO_LY)
				motion[1] = ptrPos[k][1] - ptrPos[0][1];
			if (typeMotion & STUDIO_LZ)
				motion[2] = ptrPos[k][2] - ptrPos[0][2];

			for (j = 0; j < qc_cmd.sequence[i].numframes; j++)
			{
				Vector3 adjustedPosition;
				for (k = 0; k < qc_cmd.sequence[i].panim[0]->numbones; k++)
				{
					if (qc_cmd.sequence[i].panim[0]->node[k].parent == -1)
					{
						ptrPos = qc_cmd.sequence[i].panim[0]->pos[k];

						adjustedPosition = motion * (static_cast<float>(j) / static_cast<float>(qc_cmd.sequence[i].numframes - 1));
						for (blendIndex = 0; blendIndex < qc_cmd.sequence[i].numblends; blendIndex++)
						{
							qc_cmd.sequence[i].panim[blendIndex]->pos[k][j] = qc_cmd.sequence[i].panim[blendIndex]->pos[k][j] - adjustedPosition;
						}
					}
				}
			}

			qc_cmd.sequence[i].linearmovement = motion;
		}
		else
		{ //
			qc_cmd.sequence[i].linearmovement = qc_cmd.sequence[i].linearmovement - qc_cmd.sequence[i].linearmovement;
		}
	}

	// extract unused motion
	for (i = 0; i < g_num_sequence; i++)
	{
		int typeUnusedMotion = qc_cmd.sequence[i].motiontype;
		for (k = 0; k < qc_cmd.sequence[i].panim[0]->numbones; k++)
		{
			if (qc_cmd.sequence[i].panim[0]->node[k].parent == -1)
			{
				for (blendIndex = 0; blendIndex < qc_cmd.sequence[i].numblends; blendIndex++)
				{
					float motion[6];
					motion[0] = qc_cmd.sequence[i].panim[blendIndex]->pos[k][0][0];
					motion[1] = qc_cmd.sequence[i].panim[blendIndex]->pos[k][0][1];
					motion[2] = qc_cmd.sequence[i].panim[blendIndex]->pos[k][0][2];
					motion[3] = qc_cmd.sequence[i].panim[blendIndex]->rot[k][0][0];
					motion[4] = qc_cmd.sequence[i].panim[blendIndex]->rot[k][0][1];
					motion[5] = qc_cmd.sequence[i].panim[blendIndex]->rot[k][0][2];

					for (j = 0; j < qc_cmd.sequence[i].numframes; j++)
					{
						/*
						if (typeUnusedMotion & STUDIO_X)
							sequence[i].panim[blendIndex]->pos[k][j][0] = motion[0];
						if (typeUnusedMotion & STUDIO_Y)
							sequence[i].panim[blendIndex]->pos[k][j][1] = motion[1];
						if (typeUnusedMotion & STUDIO_Z)
							sequence[i].panim[blendIndex]->pos[k][j][2] = motion[2];
						*/
						if (typeUnusedMotion & STUDIO_XR)
							qc_cmd.sequence[i].panim[blendIndex]->rot[k][j][0] = motion[3];
						if (typeUnusedMotion & STUDIO_YR)
							qc_cmd.sequence[i].panim[blendIndex]->rot[k][j][1] = motion[4];
						if (typeUnusedMotion & STUDIO_ZR)
							qc_cmd.sequence[i].panim[blendIndex]->rot[k][j][2] = motion[5];
					}
				}
			}
		}
	}

	// extract auto motion
	for (i = 0; i < g_num_sequence; i++)
	{
		// assume 0 for now.
		Vector3 *ptrAutoPos;
		Vector3 *ptrAutoRot;
		Vector3 motion{0, 0, 0};
		Vector3 angles{0, 0, 0};

		int typeAutoMotion = qc_cmd.sequence[i].motiontype;
		for (j = 0; j < qc_cmd.sequence[i].numframes; j++)
		{
			ptrAutoPos = qc_cmd.sequence[i].panim[0]->pos[0];
			ptrAutoRot = qc_cmd.sequence[i].panim[0]->rot[0];

			if (typeAutoMotion & STUDIO_AX)
				motion[0] = ptrAutoPos[j][0] - ptrAutoPos[0][0];
			if (typeAutoMotion & STUDIO_AY)
				motion[1] = ptrAutoPos[j][1] - ptrAutoPos[0][1];
			if (typeAutoMotion & STUDIO_AZ)
				motion[2] = ptrAutoPos[j][2] - ptrAutoPos[0][2];
			if (typeAutoMotion & STUDIO_AXR)
				angles[0] = ptrAutoRot[j][0] - ptrAutoRot[0][0];
			if (typeAutoMotion & STUDIO_AYR)
				angles[1] = ptrAutoRot[j][1] - ptrAutoRot[0][1];
			if (typeAutoMotion & STUDIO_AZR)
				angles[2] = ptrAutoRot[j][2] - ptrAutoRot[0][2];

			qc_cmd.sequence[i].automovepos[j] = motion;
			qc_cmd.sequence[i].automoveangle[j] = angles;
		}
	}
}

void optimize_animations(QC &qc_cmd)
{
	int startFrame, endFrame;
	int typeMotion;
	int iError = 0;

	// optimize animations
	for (int i = 0; i < g_num_sequence; i++)
	{
		qc_cmd.sequence[i].numframes = qc_cmd.sequence[i].panim[0]->endframe - qc_cmd.sequence[i].panim[0]->startframe + 1;

		// force looping animations to be looping
		if (qc_cmd.sequence[i].flags & STUDIO_LOOPING)
		{
			for (int j = 0; j < qc_cmd.sequence[i].panim[0]->numbones; j++)
			{
				for (int blends = 0; blends < qc_cmd.sequence[i].numblends; blends++)
				{
					Vector3 *ppos = qc_cmd.sequence[i].panim[blends]->pos[j];
					Vector3 *prot = qc_cmd.sequence[i].panim[blends]->rot[j];

					startFrame = 0;								 // sequence[i].panim[q]->startframe;
					endFrame = qc_cmd.sequence[i].numframes - 1; // sequence[i].panim[q]->endframe;

					typeMotion = qc_cmd.sequence[i].motiontype;
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

		for (int j = 0; j < qc_cmd.sequence[i].numevents; j++)
		{
			if (qc_cmd.sequence[i].event[j].frame < qc_cmd.sequence[i].panim[0]->startframe)
			{
				printf("sequence %s has event (%d) before first frame (%d)\n", qc_cmd.sequence[i].name, qc_cmd.sequence[i].event[j].frame, qc_cmd.sequence[i].panim[0]->startframe);
				qc_cmd.sequence[i].event[j].frame = qc_cmd.sequence[i].panim[0]->startframe;
				iError++;
			}
			if (qc_cmd.sequence[i].event[j].frame > qc_cmd.sequence[i].panim[0]->endframe)
			{
				printf("sequence %s has event (%d) after last frame (%d)\n", qc_cmd.sequence[i].name, qc_cmd.sequence[i].event[j].frame, qc_cmd.sequence[i].panim[0]->endframe);
				qc_cmd.sequence[i].event[j].frame = qc_cmd.sequence[i].panim[0]->endframe;
				iError++;
			}
		}

		qc_cmd.sequence[i].frameoffset = qc_cmd.sequence[i].panim[0]->startframe;
	}
}

int find_node(std::string name)
{
	for (int k = 0; k < g_bonescount; k++)
	{
		if (g_bonetable[k].name == name)
		{
			return k;
		}
	}
	return -1;
}

void make_transitions(QC &qc_cmd)
{
	int i, j;
	int iHit;

	// Add in direct node transitions
	for (i = 0; i < g_num_sequence; i++)
	{
		if (qc_cmd.sequence[i].entrynode != qc_cmd.sequence[i].exitnode)
		{
			g_xnode[qc_cmd.sequence[i].entrynode - 1][qc_cmd.sequence[i].exitnode - 1] = qc_cmd.sequence[i].exitnode;
			if (qc_cmd.sequence[i].nodeflags)
			{
				g_xnode[qc_cmd.sequence[i].exitnode - 1][qc_cmd.sequence[i].entrynode - 1] = qc_cmd.sequence[i].entrynode;
			}
		}
		if (qc_cmd.sequence[i].entrynode > g_numxnodes)
			g_numxnodes = qc_cmd.sequence[i].entrynode;
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

void simplify_model(QC &qc_cmd)
{
	int i, j, k;
	int n, m, q;
	Vector3 *defaultpos[MAXSTUDIOSRCBONES] = {0};
	Vector3 *defaultrot[MAXSTUDIOSRCBONES] = {0};
	int iError = 0;

	optimize_animations(qc_cmd);
	extract_motion(qc_cmd);
	make_transitions(qc_cmd);

	// find used bones TODO: find_used_bones()
	for (i = 0; i < g_num_submodels; i++)
	{
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			qc_cmd.submodel[i]->boneref[k] = g_flagkeepallbones;
		}
		for (j = 0; j < qc_cmd.submodel[i]->numverts; j++)
		{
			qc_cmd.submodel[i]->boneref[qc_cmd.submodel[i]->vert[j].bone] = 1;
		}
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			// tag parent bones as used if child has been used
			if (qc_cmd.submodel[i]->boneref[k])
			{
				n = qc_cmd.submodel[i]->node[k].parent;
				while (n != -1 && !qc_cmd.submodel[i]->boneref[n])
				{
					qc_cmd.submodel[i]->boneref[n] = 1;
					n = qc_cmd.submodel[i]->node[n].parent;
				}
			}
		}
	}

	// rename model bones if needed TODO: rename_submodel_bones()
	for (i = 0; i < g_num_submodels; i++)
	{
		for (j = 0; j < qc_cmd.submodel[i]->numbones; j++)
		{
			for (k = 0; k < qc_cmd.renamebones.size(); k++)
			{
				if (qc_cmd.submodel[i]->node[j].name == qc_cmd.renamebones[k].from)
				{
					qc_cmd.submodel[i]->node[j].name = qc_cmd.renamebones[k].to;
					break;
				}
			}
		}
	}

	// union of all used bones TODO:create_bone_union()
	g_bonescount = 0;
	for (i = 0; i < g_num_submodels; i++)
	{
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			qc_cmd.submodel[i]->boneimap[k] = -1;
		}
		for (j = 0; j < qc_cmd.submodel[i]->numbones; j++)
		{
			if (qc_cmd.submodel[i]->boneref[j])
			{
				k = find_node(qc_cmd.submodel[i]->node[j].name);
				if (k == -1)
				{
					// create new bone
					k = g_bonescount;
					g_bonetable[k].name = qc_cmd.submodel[i]->node[j].name;
					if ((n = qc_cmd.submodel[i]->node[j].parent) != -1)
						g_bonetable[k].parent = find_node(qc_cmd.submodel[i]->node[n].name);
					else
						g_bonetable[k].parent = -1;
					g_bonetable[k].bonecontroller = 0;
					g_bonetable[k].flags = 0;
					// set defaults
					defaultpos[k] = (Vector3 *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(Vector3));
					defaultrot[k] = (Vector3 *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(Vector3));
					for (n = 0; n < MAXSTUDIOANIMATIONS; n++)
					{
						defaultpos[k][n] = qc_cmd.submodel[i]->skeleton[j].pos;
						defaultrot[k][n] = qc_cmd.submodel[i]->skeleton[j].rot;
					}
					g_bonetable[k].pos = qc_cmd.submodel[i]->skeleton[j].pos;
					g_bonetable[k].rot = qc_cmd.submodel[i]->skeleton[j].rot;
					g_bonescount++;
				}
				else
				{
					// double check parent assignments
					n = qc_cmd.submodel[i]->node[j].parent;
					if (n != -1)
						n = find_node(qc_cmd.submodel[i]->node[n].name);
					m = g_bonetable[k].parent;

					if (n != m)
					{
						printf("illegal parent bone replacement in model \"%s\"\n\t\"%s\" has \"%s\", previously was \"%s\"\n",
							   qc_cmd.submodel[i]->name,
							   qc_cmd.submodel[i]->node[j].name,
							   (n != -1) ? g_bonetable[n].name : "ROOT",
							   (m != -1) ? g_bonetable[m].name : "ROOT");
						iError++;
					}
				}
				qc_cmd.submodel[i]->bonemap[j] = k;
				qc_cmd.submodel[i]->boneimap[k] = j;
			}
		}
	}

	if (iError)
	{
		exit(1);
	}

	if (g_bonescount >= MAXSTUDIOBONES)
	{
		error("Too many bones used in model, used %d, max %d\n", g_bonescount, MAXSTUDIOBONES);
	}

	// rename sequence bones if needed TODO: rename_sequence_bones()
	for (i = 0; i < g_num_sequence; i++)
	{
		for (j = 0; j < qc_cmd.sequence[i].panim[0]->numbones; j++)
		{
			for (k = 0; k < qc_cmd.renamebones.size(); k++)
			{
				if (qc_cmd.sequence[i].panim[0]->node[j].name == qc_cmd.renamebones[k].from)
				{
					qc_cmd.sequence[i].panim[0]->node[j].name = qc_cmd.renamebones[k].to;
					break;
				}
			}
		}
	}

	// map each sequences bone list to master list TODO: map_sequence_bones()
	for (i = 0; i < g_num_sequence; i++)
	{
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			qc_cmd.sequence[i].panim[0]->boneimap[k] = -1;
		}
		for (j = 0; j < qc_cmd.sequence[i].panim[0]->numbones; j++)
		{
			k = find_node(qc_cmd.sequence[i].panim[0]->node[j].name);

			if (k == -1)
			{
				qc_cmd.sequence[i].panim[0]->bonemap[j] = -1;
			}
			else
			{
				char *szAnim = "ROOT";
				char *szNode = "ROOT";

				// whoa, check parent connections!
				if (qc_cmd.sequence[i].panim[0]->node[j].parent != -1)
					szAnim = const_cast<char*>((qc_cmd.sequence[i].panim[0]->node[qc_cmd.sequence[i].panim[0]->node[j].parent].name).c_str());

				if (g_bonetable[k].parent != -1)
					szNode = const_cast<char*>(g_bonetable[g_bonetable[k].parent].name.c_str());

				if (std::strcmp(szAnim, szNode))
				{
					printf("illegal parent bone replacement in sequence \"%s\"\n\t\"%s\" has \"%s\", reference has \"%s\"\n",
						   qc_cmd.sequence[i].name,
						   qc_cmd.sequence[i].panim[0]->node[j].name,
						   szAnim,
						   szNode);
					iError++;
				}
				qc_cmd.sequence[i].panim[0]->bonemap[j] = k;
				qc_cmd.sequence[i].panim[0]->boneimap[k] = j;
			}
		}
	}
	if (iError)
	{
		exit(1);
	}

	// link bonecontrollers TODO: link_bone_controllers()
	for (i = 0; i < qc_cmd.bonecontrollers.size(); i++)
	{
		for (j = 0; j < g_bonescount; j++)
		{
			if (qc_cmd.bonecontrollers[i].name == g_bonetable[j].name)
				break;
		}
		if (j >= g_bonescount)
		{
			error("unknown bonecontroller link '%s'\n", qc_cmd.bonecontrollers[i].name);
		}
		qc_cmd.bonecontrollers[i].bone = j;
	}

	// link attachments TODO: link_attachments()
	for (i = 0; i < qc_cmd.attachments.size(); i++)
	{
		for (j = 0; j < g_bonescount; j++)
		{
			if (qc_cmd.attachments[i].bonename == g_bonetable[j].name)
				break;
		}
		if (j >= g_bonescount)
		{
			error("unknown attachment link '%s'\n", qc_cmd.attachments[i].bonename);
		}
		qc_cmd.attachments[i].bone = j;
	}

	// relink model TODO: relink_model()
	for (i = 0; i < g_num_submodels; i++)
	{
		for (j = 0; j < qc_cmd.submodel[i]->numverts; j++)
		{
			qc_cmd.submodel[i]->vert[j].bone = qc_cmd.submodel[i]->bonemap[qc_cmd.submodel[i]->vert[j].bone];
		}
		for (j = 0; j < qc_cmd.submodel[i]->numnorms; j++)
		{
			qc_cmd.submodel[i]->normal[j].bone = qc_cmd.submodel[i]->bonemap[qc_cmd.submodel[i]->normal[j].bone];
		}
	}

	// set hitgroups TODO: set_hit_groups()
	for (k = 0; k < g_bonescount; k++)
	{
		g_bonetable[k].group = -9999;
	}
	for (j = 0; j < qc_cmd.hitgroups.size(); j++)
	{
		for (k = 0; k < g_bonescount; k++)
		{
			if (g_bonetable[k].name == qc_cmd.hitgroups[j].name)
			{
				g_bonetable[k].group = qc_cmd.hitgroups[j].group;
				break;
			}
		}
		if (k >= g_bonescount)
			error("cannot find bone %s for hitgroup %d\n", qc_cmd.hitgroups[j].name, qc_cmd.hitgroups[j].group);
	}
	for (k = 0; k < g_bonescount; k++)
	{
		if (g_bonetable[k].group == -9999)
		{
			if (g_bonetable[k].parent != -1)
				g_bonetable[k].group = g_bonetable[g_bonetable[k].parent].group;
			else
				g_bonetable[k].group = 0;
		}
	}

	if (qc_cmd.hitboxes.empty()) // TODO: find_or_create_hitboxes()
	{
		// find intersection box volume for each bone
		for (k = 0; k < g_bonescount; k++)
		{
			for (j = 0; j < 3; j++)
			{
				g_bonetable[k].bmin[j] = 0.0;
				g_bonetable[k].bmax[j] = 0.0;
			}
		}
		// try all the connect vertices
		for (i = 0; i < g_num_submodels; i++)
		{
			Vector3 p;
			for (j = 0; j < qc_cmd.submodel[i]->numverts; j++)
			{
				p = qc_cmd.submodel[i]->vert[j].org;
				k = qc_cmd.submodel[i]->vert[j].bone;

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
		for (k = 0; k < g_bonescount; k++)
		{
			if ((j = g_bonetable[k].parent) != -1)
			{
				if (g_bonetable[k].pos[0] < g_bonetable[j].bmin[0])
					g_bonetable[j].bmin[0] = g_bonetable[k].pos[0];
				if (g_bonetable[k].pos[1] < g_bonetable[j].bmin[1])
					g_bonetable[j].bmin[1] = g_bonetable[k].pos[1];
				if (g_bonetable[k].pos[2] < g_bonetable[j].bmin[2])
					g_bonetable[j].bmin[2] = g_bonetable[k].pos[2];
				if (g_bonetable[k].pos[0] > g_bonetable[j].bmax[0])
					g_bonetable[j].bmax[0] = g_bonetable[k].pos[0];
				if (g_bonetable[k].pos[1] > g_bonetable[j].bmax[1])
					g_bonetable[j].bmax[1] = g_bonetable[k].pos[1];
				if (g_bonetable[k].pos[2] > g_bonetable[j].bmax[2])
					g_bonetable[j].bmax[2] = g_bonetable[k].pos[2];
			}
		}

		for (k = 0; k < g_bonescount; k++)
		{
			if (g_bonetable[k].bmin[0] < g_bonetable[k].bmax[0] - 1 && g_bonetable[k].bmin[1] < g_bonetable[k].bmax[1] - 1 && g_bonetable[k].bmin[2] < g_bonetable[k].bmax[2] - 1)
			{
				HitBox genhbox;
				genhbox.bone = k;
				genhbox.group = g_bonetable[k].group;
				genhbox.bmin = g_bonetable[k].bmin;
				genhbox.bmax = g_bonetable[k].bmax;
				qc_cmd.hitboxes.push_back(genhbox);
			}
		}
	}
	else
	{
		for (j = 0; j < qc_cmd.hitboxes.size(); j++)
		{
			for (k = 0; k < g_bonescount; k++)
			{
				if (g_bonetable[k].name == qc_cmd.hitboxes[j].name)
				{
					qc_cmd.hitboxes[j].bone = k;
					break;
				}
			}
			if (k >= g_bonescount)
				error("cannot find bone %s for bbox\n", qc_cmd.hitboxes[j].name);
		}
	}

	// relink animations TODO: relink_animations()
	for (i = 0; i < g_num_sequence; i++)
	{
		Vector3 *origpos[MAXSTUDIOSRCBONES] = {nullptr};
		Vector3 *origrot[MAXSTUDIOSRCBONES] = {nullptr};

		for (q = 0; q < qc_cmd.sequence[i].numblends; q++)
		{
			// save pointers to original animations
			for (j = 0; j < qc_cmd.sequence[i].panim[q]->numbones; j++)
			{
				origpos[j] = qc_cmd.sequence[i].panim[q]->pos[j];
				origrot[j] = qc_cmd.sequence[i].panim[q]->rot[j];
			}

			for (j = 0; j < g_bonescount; j++)
			{
				if ((k = qc_cmd.sequence[i].panim[0]->boneimap[j]) >= 0)
				{
					// link to original animations
					qc_cmd.sequence[i].panim[q]->pos[j] = origpos[k];
					qc_cmd.sequence[i].panim[q]->rot[j] = origrot[k];
				}
				else
				{
					// link to dummy animations
					qc_cmd.sequence[i].panim[q]->pos[j] = defaultpos[j];
					qc_cmd.sequence[i].panim[q]->rot[j] = defaultrot[j];
				}
			}
		}
	}

	// find scales for all bones TODO: find_bone_scales()
	for (j = 0; j < g_bonescount; j++)
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

			for (i = 0; i < g_num_sequence; i++)
			{
				for (q = 0; q < qc_cmd.sequence[i].numblends; q++)
				{
					for (n = 0; n < qc_cmd.sequence[i].numframes; n++)
					{
						float v;
						switch (k)
						{
						case 0:
						case 1:
						case 2:
							v = (qc_cmd.sequence[i].panim[q]->pos[j][n][k] - g_bonetable[j].pos[k]);
							break;
						case 3:
						case 4:
						case 5:
							v = (qc_cmd.sequence[i].panim[q]->rot[j][n][k - 3] - g_bonetable[j].rot[k - 3]);
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
	for (i = 0; i < g_num_sequence; i++)
	{
		Vector3 bmin, bmax;

		// find intersection box volume for each bone
		for (j = 0; j < 3; j++)
		{
			bmin[j] = 9999.0;
			bmax[j] = -9999.0;
		}

		for (q = 0; q < qc_cmd.sequence[i].numblends; q++)
		{
			for (n = 0; n < qc_cmd.sequence[i].numframes; n++)
			{
				float bonetransform[MAXSTUDIOBONES][3][4]; // bone transformation matrix
				float bonematrix[3][4];					   // local transformation matrix
				Vector3 pos;

				for (j = 0; j < g_bonescount; j++)
				{

					Vector3 angles;
					angles.x = to_degrees(qc_cmd.sequence[i].panim[q]->rot[j][n][0]);
					angles.y = to_degrees(qc_cmd.sequence[i].panim[q]->rot[j][n][1]);
					angles.z = to_degrees(qc_cmd.sequence[i].panim[q]->rot[j][n][2]);

					angle_matrix(angles, bonematrix);

					bonematrix[0][3] = qc_cmd.sequence[i].panim[q]->pos[j][n][0];
					bonematrix[1][3] = qc_cmd.sequence[i].panim[q]->pos[j][n][1];
					bonematrix[2][3] = qc_cmd.sequence[i].panim[q]->pos[j][n][2];

					if (g_bonetable[j].parent == -1)
					{
						matrix_copy(bonematrix, bonetransform[j]);
					}
					else
					{
						r_concat_transforms(bonetransform[g_bonetable[j].parent], bonematrix, bonetransform[j]);
					}
				}

				for (k = 0; k < g_num_submodels; k++)
				{
					for (j = 0; j < qc_cmd.submodel[k]->numverts; j++)
					{
						vector_transform(qc_cmd.submodel[k]->vert[j].org, bonetransform[qc_cmd.submodel[k]->vert[j].bone], pos);

						if (pos[0] < bmin[0])
							bmin[0] = pos[0];
						if (pos[1] < bmin[1])
							bmin[1] = pos[1];
						if (pos[2] < bmin[2])
							bmin[2] = pos[2];
						if (pos[0] > bmax[0])
							bmax[0] = pos[0];
						if (pos[1] > bmax[1])
							bmax[1] = pos[1];
						if (pos[2] > bmax[2])
							bmax[2] = pos[2];
					}
				}
			}
		}

		qc_cmd.sequence[i].bmin = bmin;
		qc_cmd.sequence[i].bmax = bmax;
	}

	// reduce animations TODO: reduce_animations()
	{
		int changes = 0;
		int p;

		for (i = 0; i < g_num_sequence; i++)
		{
			for (q = 0; q < qc_cmd.sequence[i].numblends; q++)
			{
				for (j = 0; j < g_bonescount; j++)
				{
					for (k = 0; k < DEGREESOFFREEDOM; k++)
					{
						float v;
						short value[MAXSTUDIOANIMATIONS];
						StudioAnimationValue data[MAXSTUDIOANIMATIONS];

						for (n = 0; n < qc_cmd.sequence[i].numframes; n++)
						{
							switch (k)
							{
							case 0:
							case 1:
							case 2:
								value[n] = static_cast<short>((qc_cmd.sequence[i].panim[q]->pos[j][n][k] - g_bonetable[j].pos[k]) / g_bonetable[j].posscale[k]);
								break;
							case 3:
							case 4:
							case 5:
								v = (qc_cmd.sequence[i].panim[q]->rot[j][n][k - 3] - g_bonetable[j].rot[k - 3]);
								if (v >= Q_PI)
									v -= Q_PI * 2;
								if (v < -Q_PI)
									v += Q_PI * 2;

								value[n] = static_cast<short>(v / g_bonetable[j].rotscale[k - 3]);
								break;
							}
						}
						if (n == 0)
							error("no animation frames: \"%s\"\n", qc_cmd.sequence[i].name);

						qc_cmd.sequence[i].panim[q]->numanim[j][k] = 0;

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

						qc_cmd.sequence[i].panim[q]->numanim[j][k] = static_cast<int>(pvalue - data);
						if (qc_cmd.sequence[i].panim[q]->numanim[j][k] == 2 && value[0] == 0)
						{
							qc_cmd.sequence[i].panim[q]->numanim[j][k] = 0;
						}
						else
						{
							qc_cmd.sequence[i].panim[q]->anim[j][k] = (StudioAnimationValue *)std::calloc(pvalue - data, sizeof(StudioAnimationValue));
							std::memcpy(qc_cmd.sequence[i].panim[q]->anim[j][k], data, (pvalue - data) * sizeof(StudioAnimationValue));
						}
					}
				}
			}
		}
	}
}

int lookup_control(const char *string)
{
	if (stricmp(string, "X") == 0)
		return STUDIO_X;
	if (stricmp(string, "Y") == 0)
		return STUDIO_Y;
	if (stricmp(string, "Z") == 0)
		return STUDIO_Z;
	if (stricmp(string, "XR") == 0)
		return STUDIO_XR;
	if (stricmp(string, "YR") == 0)
		return STUDIO_YR;
	if (stricmp(string, "ZR") == 0)
		return STUDIO_ZR;
	if (stricmp(string, "LX") == 0)
		return STUDIO_LX;
	if (stricmp(string, "LY") == 0)
		return STUDIO_LY;
	if (stricmp(string, "LZ") == 0)
		return STUDIO_LZ;
	if (stricmp(string, "AX") == 0)
		return STUDIO_AX;
	if (stricmp(string, "AY") == 0)
		return STUDIO_AY;
	if (stricmp(string, "AZ") == 0)
		return STUDIO_AZ;
	if (stricmp(string, "AXR") == 0)
		return STUDIO_AXR;
	if (stricmp(string, "AYR") == 0)
		return STUDIO_AYR;
	if (stricmp(string, "AZR") == 0)
		return STUDIO_AZR;
	return -1;
}

int find_texture_index(std::string texturename)
{
	int i;
	for (i = 0; i < g_texturescount; i++)
	{
		if (g_textures[i].name == texturename)
		{
			return i;
		}
	}
	g_textures[i].name = texturename;

	// XDM: allow such names as "tex_chrome_bright" - chrome and full brightness effects
    std::string lower_texname = texturename;
    std::transform(lower_texname.begin(), lower_texname.end(), lower_texname.begin(), ::tolower);
	if (lower_texname.find("chrome") != std::string::npos)
		g_textures[i].flags = STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME;
	else if (lower_texname.find("bright") != std::string::npos)
		g_textures[i].flags = STUDIO_NF_FLATSHADE | STUDIO_NF_FULLBRIGHT;
	else
		g_textures[i].flags = 0;

	g_texturescount++;
	return i;
}

Mesh *find_mesh_by_texture(Model *pmodel, char *texturename)
{
	int i;
	int j = find_texture_index(std::string(texturename));

	for (i = 0; i < pmodel->nummesh; i++)
	{
		if (pmodel->pmesh[i]->skinref == j)
		{
			return pmodel->pmesh[i];
		}
	}

	if (i >= MAXSTUDIOMESHES)
	{
		error("too many textures in model: \"%s\"\n", pmodel->name);
	}

	pmodel->nummesh = i + 1;
	pmodel->pmesh[i] = (Mesh *)std::calloc(1, sizeof(Mesh));
	pmodel->pmesh[i]->skinref = j;

	return pmodel->pmesh[i];
}

TriangleVert *find_mesh_triangle_by_index(Mesh *pmesh, int index)
{
	if (index >= pmesh->alloctris)
	{
		int start = pmesh->alloctris;
		pmesh->alloctris = index + 256;
		if (pmesh->triangle)
		{
			pmesh->triangle = static_cast<TriangleVert(*)[3]>(realloc(pmesh->triangle, pmesh->alloctris * sizeof(*pmesh->triangle)));
			std::memset(&pmesh->triangle[start], 0, (pmesh->alloctris - start) * sizeof(*pmesh->triangle));
		}
		else
		{
			pmesh->triangle = static_cast<TriangleVert(*)[3]>(std::calloc(pmesh->alloctris, sizeof(*pmesh->triangle)));
		}
	}

	return pmesh->triangle[index];
}

int find_vertex_normal_index(Model *pmodel, Normal *pnormal)
{
	int i;
	for (i = 0; i < pmodel->numnorms; i++)
	{
		if (pmodel->normal[i].org.dot(pnormal->org) > g_flagnormalblendangle && pmodel->normal[i].bone == pnormal->bone && pmodel->normal[i].skinref == pnormal->skinref)
		{
			return i;
		}
	}
	if (i >= MAXSTUDIOVERTS)
	{
		error("too many normals in model: \"%s\"\n", pmodel->name);
	}
	pmodel->normal[i].org = pnormal->org;
	pmodel->normal[i].bone = pnormal->bone;
	pmodel->normal[i].skinref = pnormal->skinref;
	pmodel->numnorms = i + 1;
	return i;
}

int find_vertex_index(Model *pmodel, Vertex *pv)
{
	int i;

	// assume 2 digits of accuracy
	pv->org[0] = static_cast<int>(pv->org[0] * 100.0f) / 100.0f;
	pv->org[1] = static_cast<int>(pv->org[1] * 100.0f) / 100.0f;
	pv->org[2] = static_cast<int>(pv->org[2] * 100.0f) / 100.0f;

	for (i = 0; i < pmodel->numverts; i++)
	{
		if ((pmodel->vert[i].org == pv->org) && pmodel->vert[i].bone == pv->bone)
		{
			return i;
		}
	}
	if (i >= MAXSTUDIOVERTS)
	{
		error("too many vertices in model: \"%s\"\n", pmodel->name);
	}
	pmodel->vert[i].org = pv->org;
	pmodel->vert[i].bone = pv->bone;
	pmodel->numverts = i + 1;
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
				pmesh->triangle[i][j].s = 0;
				pmesh->triangle[i][j].t = +1; // ??
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
			pmesh->triangle[i][j].s = static_cast<int>(std::round(pmesh->triangle[i][j].u * static_cast<float>(ptexture->srcwidth)));
			pmesh->triangle[i][j].t = static_cast<int>(std::round(pmesh->triangle[i][j].v * static_cast<float>(ptexture->srcheight)));
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
			pmesh->triangle[i][j].t = -pmesh->triangle[i][j].t + ptexture->srcheight - static_cast<int>(ptexture->min_t);
		}
	}
}

void grab_bmp(const char *filename, Texture *ptexture)
{
	if (int result = load_bmp(filename, &ptexture->ppicture, (std::uint8_t **)&ptexture->ppal, &ptexture->srcwidth, &ptexture->srcheight))
	{
		error("error %d reading BMP image \"%s\"\n", result, filename);
	}
}

void resize_texture(QC &qc_cmd, Texture *ptexture)
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
	if (qc_cmd.gamma != 1.8f)
	// gamma correct the monster textures to a gamma of 1.8
	{
		std::uint8_t *psrc = (std::uint8_t *)ptexture->ppal;
		float g = qc_cmd.gamma / 1.8f;
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

void grab_skin(QC &qc_cmd, Texture *ptexture)
{
	std::filesystem::path textureFilePath = qc_cmd.cdtexture / ptexture->name;
	if (!std::filesystem::exists(textureFilePath))
	{
		textureFilePath = qc_cmd.cdAbsolute / ptexture->name;
		if (!std::filesystem::exists(textureFilePath))
		{
			error("cannot find %s texture in \"%s\" nor \"%s\"\nor those path don't exist\n", ptexture->name.c_str(), qc_cmd.cdtexture.c_str(), qc_cmd.cdAbsolute.c_str());
		}
	}
	if (textureFilePath.extension() == ".bmp")
	{
		grab_bmp(textureFilePath.string().c_str(), ptexture);
	}
	else
	{
		error(const_cast<char *>("unknown graphics type: \"%s\"\n"), textureFilePath.string().c_str());
	}
}

void set_skin_values(QC &qc_cmd)
{
	int i, j;

	for (i = 0; i < g_texturescount; i++)
	{
		grab_skin(qc_cmd, &g_textures[i]);

		g_textures[i].max_s = -9999999;
		g_textures[i].min_s = 9999999;
		g_textures[i].max_t = -9999999;
		g_textures[i].min_t = 9999999;
	}

	for (i = 0; i < g_num_submodels; i++)
	{
		for (j = 0; j < qc_cmd.submodel[i]->nummesh; j++)
		{
			texture_coord_ranges(qc_cmd.submodel[i]->pmesh[j], &g_textures[qc_cmd.submodel[i]->pmesh[j]->skinref]);
		}
	}

	for (i = 0; i < g_texturescount; i++)
	{
		if (g_textures[i].max_s < g_textures[i].min_s)
		{
			// must be a replacement texture
			if (g_textures[i].flags & STUDIO_NF_CHROME)
			{
				g_textures[i].max_s = 63;
				g_textures[i].min_s = 0;
				g_textures[i].max_t = 63;
				g_textures[i].min_t = 0;
			}
			else
			{
				g_textures[i].max_s = g_textures[g_textures[i].parent].max_s;
				g_textures[i].min_s = g_textures[g_textures[i].parent].min_s;
				g_textures[i].max_t = g_textures[g_textures[i].parent].max_t;
				g_textures[i].min_t = g_textures[g_textures[i].parent].min_t;
			}
		}

		resize_texture(qc_cmd, &g_textures[i]);
	}

	for (i = 0; i < g_num_submodels; i++)
	{
		for (j = 0; j < qc_cmd.submodel[i]->nummesh; j++)
		{
			reset_texture_coord_ranges(qc_cmd.submodel[i]->pmesh[j], &g_textures[qc_cmd.submodel[i]->pmesh[j]->skinref]);
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
	for (i = 0; i < qc_cmd.texturegroup_rows; i++)
	{
		for (j = 0; j < qc_cmd.texturegroup_cols; j++)
		{
			g_skinref[i][qc_cmd.texturegroup[0][j]] = qc_cmd.texturegroup[i][j];
		}
	}
	if (i != 0)
	{
		g_skinfamiliescount = i;
	}
	else
	{
		g_skinfamiliescount = 1;
		g_skinrefcount = g_texturescount;
	}
}

void build_reference(Model *pmodel)
{
	Vector3 boneAngle{};

	for (int i = 0; i < pmodel->numbones; i++)
	{
		// convert to degrees
		boneAngle[0] = to_degrees(pmodel->skeleton[i].rot[0]);
		boneAngle[1] = to_degrees(pmodel->skeleton[i].rot[1]);
		boneAngle[2] = to_degrees(pmodel->skeleton[i].rot[2]);

		int parent = pmodel->node[i].parent;
		if (parent == -1)
		{
			// scale the done pos.
			// calc rotational matrices
			angle_matrix(boneAngle, g_bonefixup[i].m);
			angle_i_matrix(boneAngle, g_bonefixup[i].im);
			g_bonefixup[i].worldorg = pmodel->skeleton[i].pos;
		}
		else
		{
			Vector3 truePos;
			float rotationMatrix[3][4];
			// calc compound rotational matrices
			// FIXME : Hey, it's orthogical so inv(A) == transpose(A)
			angle_matrix(boneAngle, rotationMatrix);
			r_concat_transforms(g_bonefixup[parent].m, rotationMatrix, g_bonefixup[i].m);
			angle_i_matrix(boneAngle, rotationMatrix);
			r_concat_transforms(rotationMatrix, g_bonefixup[parent].im, g_bonefixup[i].im);

			// calc true world coord.
			vector_transform(pmodel->skeleton[i].pos, g_bonefixup[parent].m, truePos);
			g_bonefixup[i].worldorg = truePos + g_bonefixup[parent].worldorg;
		}
	}
}

void grab_smd_triangles(QC &qc_cmd, Model *pmodel)
{
	int i;
	int trianglesCount = 0;
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

			// check for end
			if (std::strcmp("end\n", g_currentsmdline) == 0)
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
				if (stricmp(triangleMaterial, g_sourcetexture[i]) == 0)
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
							   &triangleVertex.org[0], &triangleVertex.org[1], &triangleVertex.org[2],
							   &triangleNormal.org[0], &triangleNormal.org[1], &triangleNormal.org[2],
							   &ptriangleVert->u, &ptriangleVert->v) == 9)
					{
						if (parentBone < 0 || parentBone >= pmodel->numbones)
						{
							fprintf(stderr, "bogus bone index\n");
							fprintf(stderr, "%d %s :\n%s", g_smdlinecount, g_smdpath.c_str(), g_currentsmdline);
							exit(1);
						}

						triangleVertices[j] = triangleVertex.org;
						triangleNormals[j] = triangleNormal.org;

						triangleVertex.bone = parentBone;
						triangleNormal.bone = parentBone;
						triangleNormal.skinref = pmesh->skinref;

						if (triangleVertex.org[2] < vmin[2])
							vmin[2] = triangleVertex.org[2];

						triangleVertex.org -= qc_cmd.sequenceOrigin; // adjust vertex to origin
						triangleVertex.org *= qc_cmd.scaleBodyAndSequenceOption; // scale vertex

						// move vertex position to object space.
						Vector3 tmp;
						tmp = triangleVertex.org - g_bonefixup[triangleVertex.bone].worldorg;
						vector_transform(tmp, g_bonefixup[triangleVertex.bone].im, triangleVertex.org);

						// move normal to object space.
						tmp = triangleNormal.org;
						vector_transform(tmp, g_bonefixup[triangleVertex.bone].im, triangleNormal.org);
						triangleNormal.org.normalize();

						ptriangleVert->normindex = find_vertex_normal_index(pmodel, &triangleNormal);
						ptriangleVert->vertindex = find_vertex_index(pmodel, &triangleVertex);

						// tag bone as being used
						// pmodel->bone[bone].ref = 1;
					}
					else
					{
						error("%s: error on line %d: %s", g_smdpath, g_smdlinecount, g_currentsmdline);
					}
				}
			}

			pmodel->trimesh[trianglesCount] = pmesh;
			pmodel->trimap[trianglesCount] = pmesh->numtris++;
			trianglesCount++;
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

void grab_smd_skeleton(QC &qc_cmd, Node *pnodes, Bone *pbones)
{
	float posX, posY, posZ, rotX, rotY, rotZ;
	char cmd[1024];
	int node;

	while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != nullptr)
	{
		g_smdlinecount++;
		if (sscanf(g_currentsmdline, "%d %f %f %f %f %f %f", &node, &posX, &posY, &posZ, &rotX, &rotY, &rotZ) == 7)
		{
			pbones[node].pos[0] = posX;
			pbones[node].pos[1] = posY;
			pbones[node].pos[2] = posZ;

			pbones[node].pos *= qc_cmd.scaleBodyAndSequenceOption; // scale vertex

			if (pnodes[node].mirrored)
				pbones[node].pos = pbones[node].pos * -1.0;

			pbones[node].rot[0] = rotX;
			pbones[node].rot[1] = rotY;
			pbones[node].rot[2] = rotZ;

			clip_rotations(pbones[node].rot);
		}
		else if (sscanf(g_currentsmdline, "%s %d", cmd, &node)) // Delete this
		{
			if (std::strcmp(cmd, "time") == 0)
			{
				// pbones = pnode->bones[index] = std::calloc(1, sizeof( s_bones_t ));
			}
			else if (std::strcmp(cmd, "end") == 0)
			{
				return;
			}
		}
	}
}

int grab_smd_nodes(QC &qc_cmd, Node *pnodes)
{
	int index;
	char boneName[1024];
	int parent;
	int numBones = 0;

	while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != nullptr)
	{
		g_smdlinecount++;
		if (sscanf(g_currentsmdline, "%d \"%[^\"]\" %d", &index, boneName, &parent) == 3)
		{

			pnodes[index].name = boneName;
			pnodes[index].parent = parent;
			numBones = index;
			// check for mirrored bones;
			for (int i = 0; i < qc_cmd.mirroredbones.size(); i++)
			{
				if (std::strcmp(boneName, qc_cmd.mirroredbones[i].data()) == 0)
					pnodes[index].mirrored = 1;
			}
			if ((!pnodes[index].mirrored) && parent != -1)
			{
				pnodes[index].mirrored = pnodes[pnodes[index].parent].mirrored;
			}
		}
		else
		{
			return numBones + 1;
		}
	}
	error("Unexpected EOF at line %d\n", g_smdlinecount);
	return 0;
}

void parse_smd(QC &qc_cmd, Model *pmodel)
{
	char cmd[1024];
	int option;

	g_smdpath = qc_cmd.cdAbsolute / (std::string(pmodel->name) + ".smd");
	if (!std::filesystem::exists(g_smdpath))
		error("%s doesn't exist", g_smdpath.c_str());

	printf("grabbing %s\n", g_smdpath.c_str());

	if ((g_smdfile = fopen(g_smdpath.string().c_str(), "r")) == 0)
	{
		fprintf(stderr, "reader: could not open file '%s'\n", g_smdpath.c_str());
	}
	g_smdlinecount = 0;

	while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != nullptr)
	{
		g_smdlinecount++;
		sscanf(g_currentsmdline, "%s %d", cmd, &option);
		if (std::strcmp(cmd, "version") == 0)
		{
			if (option != 1)
			{
				error("bad version\n");
			}
		}
		else if (std::strcmp(cmd, "nodes") == 0)
		{
			pmodel->numbones = grab_smd_nodes(qc_cmd, pmodel->node);
		}
		else if (std::strcmp(cmd, "skeleton") == 0)
		{
			grab_smd_skeleton(qc_cmd, pmodel->node, pmodel->skeleton);
		}
		else if (std::strcmp(cmd, "triangles") == 0)
		{
			grab_smd_triangles(qc_cmd, pmodel);
		}
		else
		{
			printf("unknown studio command\n");
		}
	}
	fclose(g_smdfile);
}

void cmd_eyeposition(QC &qc_cmd, std::string &token)
{
	// rotate points into frame of reference so model points down the positive x
	// axis
	get_token(false, token);
	qc_cmd.eyeposition[1] = std::stof(token);

	get_token(false, token);
	qc_cmd.eyeposition[0] = -std::stof(token);

	get_token(false, token);
	qc_cmd.eyeposition[2] = std::stof(token);
}

void cmd_flags(QC &qc_cmd, std::string &token)
{
	get_token(false, token);
	qc_cmd.flags = std::stoi(token);
}

void cmd_modelname(QC &qc_cmd, std::string &token)
{
	get_token(false, token);
	qc_cmd.modelname = token;
}

void cmd_body_option_studio(QC &qc_cmd, std::string &token)
{
	if (!get_token(false, token))
		return;

	qc_cmd.submodel[g_num_submodels] = new Model();
	qc_cmd.bodypart[g_num_bodygroup].pmodel[qc_cmd.bodypart[g_num_bodygroup].nummodels] = qc_cmd.submodel[g_num_submodels];

	qc_cmd.submodel[g_num_submodels]->name = token;

	qc_cmd.scaleBodyAndSequenceOption = qc_cmd.scale;

	while (token_available())
	{
		get_token(false, token);
		if (stricmp("reverse", token.c_str()) == 0)
		{
			g_flaginvertnormals = true;
		}
		else if (stricmp("scale", token.c_str()) == 0)
		{
			get_token(false, token);
			qc_cmd.scaleBodyAndSequenceOption = std::stof(token);
		}
	}

	parse_smd(qc_cmd, qc_cmd.submodel[g_num_submodels]);

	qc_cmd.bodypart[g_num_bodygroup].nummodels++;
	g_num_submodels++;

	qc_cmd.scaleBodyAndSequenceOption = qc_cmd.scale;
}

int cmd_body_option_blank(QC &qc_cmd)
{
	qc_cmd.submodel[g_num_submodels] = new Model();
	qc_cmd.bodypart[g_num_bodygroup].pmodel[qc_cmd.bodypart[g_num_bodygroup].nummodels] = qc_cmd.submodel[g_num_submodels];

	qc_cmd.submodel[g_num_submodels]->name = "blank";

	qc_cmd.bodypart[g_num_bodygroup].nummodels++;
	g_num_submodels++;
	return 0;
}

void cmd_bodygroup(QC &qc_cmd, std::string &token)
{
	if (!get_token(false, token))
		return;

	if (g_num_bodygroup == 0)
	{
		qc_cmd.bodypart[g_num_bodygroup].base = 1;
	}
	else
	{
		qc_cmd.bodypart[g_num_bodygroup].base = qc_cmd.bodypart[g_num_bodygroup - 1].base * qc_cmd.bodypart[g_num_bodygroup - 1].nummodels;
	}
	qc_cmd.bodypart[g_num_bodygroup].name = token;

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
		else if (stricmp("studio", token.c_str()) == 0)
		{
			cmd_body_option_studio(qc_cmd, token);
		}
		else if (stricmp("blank", token.c_str()) == 0)
		{
			cmd_body_option_blank(qc_cmd);
		}
	}

	g_num_bodygroup++;
	return;
}

void cmd_body(QC &qc_cmd, std::string &token)
{
	if (!get_token(false, token))
		return;

	if (g_num_bodygroup == 0)
	{
		qc_cmd.bodypart[g_num_bodygroup].base = 1;
	}
	else
	{
		qc_cmd.bodypart[g_num_bodygroup].base = qc_cmd.bodypart[g_num_bodygroup - 1].base * qc_cmd.bodypart[g_num_bodygroup - 1].nummodels;
	}
	qc_cmd.bodypart[g_num_bodygroup].name = token;

	cmd_body_option_studio(qc_cmd, token);

	g_num_bodygroup++;
}

void grab_option_animation(QC &qc_cmd, Animation *panim)
{
	Vector3 pos;
	Vector3 rot;
	char cmd[1024];
	int index;
	int t = -99999999;
	int start = 99999;
	int end = 0;

	for (index = 0; index < panim->numbones; index++)
	{
		panim->pos[index] = (Vector3 *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(Vector3));
		panim->rot[index] = (Vector3 *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(Vector3));
	}

	float cz = cosf(qc_cmd.rotate);
	float sz = sinf(qc_cmd.rotate);

	while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != nullptr)
	{
		g_smdlinecount++;
		if (sscanf(g_currentsmdline, "%d %f %f %f %f %f %f", &index, &pos[0], &pos[1], &pos[2], &rot[0], &rot[1], &rot[2]) == 7)
		{
			if (t >= panim->startframe && t <= panim->endframe)
			{
				if (panim->node[index].parent == -1)
				{
					pos -= qc_cmd.sequenceOrigin; // adjust vertex to origin
					panim->pos[index][t][0] = cz * pos[0] - sz * pos[1];
					panim->pos[index][t][1] = sz * pos[0] + cz * pos[1];
					panim->pos[index][t][2] = pos[2];
					// rotate model
					rot[2] += qc_cmd.rotate;
				}
				else
				{
					panim->pos[index][t] = pos;
				}
				if (t > end)
					end = t;
				if (t < start)
					start = t;

				if (panim->node[index].mirrored)
					panim->pos[index][t] = panim->pos[index][t] * -1.0;

				panim->pos[index][t] *= qc_cmd.scaleBodyAndSequenceOption; // scale vertex

				clip_rotations(rot);

				panim->rot[index][t] = rot;
			}
		}
		else if (sscanf(g_currentsmdline, "%s %d", cmd, &index))
		{
			if (std::strcmp(cmd, "time") == 0)
			{
				t = index;
			}
			else if (std::strcmp(cmd, "end") == 0)
			{
				panim->startframe = start;
				panim->endframe = end;
				return;
			}
			else
			{
				error("Error(%d) : %s", g_smdlinecount, g_currentsmdline);
			}
		}
		else
		{
			error("Error(%d) : %s", g_smdlinecount, g_currentsmdline);
		}
	}
	error("unexpected EOF: %s\n", panim->name);
}

void shift_option_animation(Animation *panim)
{
	int size = (panim->endframe - panim->startframe + 1) * sizeof(Vector3);
	// shift
	for (int j = 0; j < panim->numbones; j++)
	{
		Vector3 *ppos = (Vector3 *)std::calloc(1, size);
		Vector3 *prot = (Vector3 *)std::calloc(1, size);

		std::memcpy(ppos, &panim->pos[j][panim->startframe], size);
		std::memcpy(prot, &panim->rot[j][panim->startframe], size);

		free(panim->pos[j]);
		free(panim->rot[j]);

		panim->pos[j] = ppos;
		panim->rot[j] = prot;
	}
}

void cmd_sequence_option_animation(QC &qc_cmd, std::string &name, Animation *panim)
{
	char cmd[1024];
	int option;

	panim->name = name;

	g_smdpath = qc_cmd.cdAbsolute / (std::string(panim->name) + ".smd");
	if (!std::filesystem::exists(g_smdpath))
		error("%s doesn't exist", g_smdpath.c_str());

	printf("grabbing %s\n", g_smdpath.c_str());

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
		if (std::strcmp(cmd, "version") == 0)
		{
			if (option != 1)
			{
				error("bad version\n");
			}
		}
		else if (std::strcmp(cmd, "nodes") == 0)
		{
			panim->numbones = grab_smd_nodes(qc_cmd, panim->node);
		}
		else if (std::strcmp(cmd, "skeleton") == 0)
		{
			grab_option_animation(qc_cmd, panim);
			shift_option_animation(panim);
		}
		else
		{
			printf("unknown studio command : %s\n", cmd);
			while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != nullptr)
			{
				g_smdlinecount++;
				if (strncmp(g_currentsmdline, "end", 3) == 0)
					break;
			}
		}
	}
	fclose(g_smdfile);
}

int cmd_sequence_option_event(std::string &token, Sequence *psequence)
{
	if (psequence->numevents + 1 >= MAXSTUDIOEVENTS)
	{
		printf("too many events\n");
		exit(0);
	}

	get_token(false, token);
	int event = std::stoi(token);
	psequence->event[psequence->numevents].event = event;

	get_token(false, token);
	psequence->event[psequence->numevents].frame = std::stoi(token);

	psequence->numevents++;

	// option token
	if (token_available())
	{
		get_token(false, token);
		if (token[0] == '}') // opps, hit the end
			return 1;
		// found an option
		psequence->event[psequence->numevents - 1].options = token;
	}

	return 0;
}

int cmd_sequence_option_fps(std::string &token, Sequence *psequence)
{
	get_token(false, token);

	psequence->fps = std::stof(token);

	return 0;
}


void cmd_origin(QC &qc_cmd, std::string &token)
{
	get_token(false, token);
	qc_cmd.origin[0] = std::stof(token);

	get_token(false, token);
	qc_cmd.origin[1] = std::stof(token);

	get_token(false, token);
	qc_cmd.origin[2] = std::stof(token);

	if (token_available())
	{
		get_token(false, token);
		qc_cmd.originRotation = to_radians(std::stof(token) + ENGINE_ORIENTATION);
	}
}

void cmd_sequence_option_origin(QC &qc_cmd, std::string &token)
{
	get_token(false, token);
	qc_cmd.sequenceOrigin[0] = std::stof(token);

	get_token(false, token);
	qc_cmd.sequenceOrigin[1] = std::stof(token);

	get_token(false, token);
	qc_cmd.sequenceOrigin[2] = std::stof(token);
}

void cmd_sequence_option_rotate(QC &qc_cmd, std::string &token)
{
	get_token(false, token);
	qc_cmd.rotate = to_radians(std::stof(token) + ENGINE_ORIENTATION);
}

void cmd_scale(QC &qc_cmd, std::string &token)
{
	get_token(false, token);
	qc_cmd.scale = qc_cmd.scaleBodyAndSequenceOption = std::stof(token);
}

void cmd_rotate(QC &qc_cmd, std::string &token) // XDM
{
	if (!get_token(false, token))
		return;
	qc_cmd.rotate = to_radians(std::stof(token) + ENGINE_ORIENTATION);
}

void cmd_sequence_option_scale(QC &qc_cmd, std::string &token)
{
	get_token(false, token);
	qc_cmd.scaleBodyAndSequenceOption = std::stof(token);
}

int cmd_sequence_option_action(std::string &szActivity)
{
	for (int i = 0; activity_map[i].name; i++)
	{
		if (stricmp(szActivity.c_str(), activity_map[i].name) == 0)
		{
			return activity_map[i].type;
		}
	}
	// match ACT_#
	if (strnicmp(szActivity.c_str(), "ACT_", 4) == 0)
	{
		return std::stoi(&szActivity[4]);
	}
	return 0;
}

int cmd_sequence(QC &qc_cmd, std::string &token)
{
	int depth = 0;
	std::vector<std::string> smd_files;
	int i;
	int start = 0;
	int end = MAXSTUDIOANIMATIONS - 1;

	if (!get_token(false, token))
		return 0;

	qc_cmd.sequence[g_num_sequence].name = token;

	qc_cmd.sequenceOrigin = qc_cmd.origin;
	qc_cmd.scaleBodyAndSequenceOption = qc_cmd.scale;

	qc_cmd.rotate = qc_cmd.originRotation;
	qc_cmd.sequence[g_num_sequence].fps = 30.0;
	qc_cmd.sequence[g_num_sequence].seqgroup = 0; // 0 since $sequencegroup is deprecated
	qc_cmd.sequence[g_num_sequence].blendstart[0] = 0.0;
	qc_cmd.sequence[g_num_sequence].blendend[0] = 1.0;

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
			depth -= cmd_sequence_option_event(token, &qc_cmd.sequence[g_num_sequence]);
		}
		else if (token == "fps")
		{
			cmd_sequence_option_fps(token, &qc_cmd.sequence[g_num_sequence]);
		}
		else if (token == "origin")
		{
			cmd_sequence_option_origin(qc_cmd, token);
		}
		else if (token == "rotate")
		{
			cmd_sequence_option_rotate(qc_cmd, token);
		}
		else if (token == "scale")
		{
			cmd_sequence_option_scale(qc_cmd, token);
		}
		else if (token == "loop")
		{
			qc_cmd.sequence[g_num_sequence].flags |= STUDIO_LOOPING;
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
			qc_cmd.sequence[g_num_sequence].blendtype[0] = static_cast<float>(lookup_control(token.c_str()));
			get_token(false, token);
			qc_cmd.sequence[g_num_sequence].blendstart[0] = std::stof(token);
			get_token(false, token);
			qc_cmd.sequence[g_num_sequence].blendend[0] = std::stof(token);
		}
		else if (token == "node")
		{
			get_token(false, token);
			qc_cmd.sequence[g_num_sequence].entrynode = qc_cmd.sequence[g_num_sequence].exitnode = std::stoi(token);
		}
		else if (token == "transition")
		{
			get_token(false, token);
			qc_cmd.sequence[g_num_sequence].entrynode = std::stoi(token);
			get_token(false, token);
			qc_cmd.sequence[g_num_sequence].exitnode = std::stoi(token);
		}
		else if (token == "rtransition")
		{
			get_token(false, token);
			qc_cmd.sequence[g_num_sequence].entrynode = std::stoi(token);
			get_token(false, token);
			qc_cmd.sequence[g_num_sequence].exitnode = std::stoi(token);
			qc_cmd.sequence[g_num_sequence].nodeflags |= 1;
		}
		else if (lookup_control(token.c_str()) != -1) // motion flags [motion extraction]
		{
			qc_cmd.sequence[g_num_sequence].motiontype |= lookup_control(token.c_str());
		}
		else if (token == "animation")
		{
			get_token(false, token);
			smd_files.push_back(token);
		}
		else if ((i = cmd_sequence_option_action(token)) != 0)
		{
			qc_cmd.sequence[g_num_sequence].activity = i;
			get_token(false, token);
			qc_cmd.sequence[g_num_sequence].actweight = std::stoi(token);
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

	if (smd_files.size() == 0)
	{
		printf("no animations found\n");
		exit(1);
	}
	for (i = 0; i < smd_files.size(); i++)
	{
		qc_cmd.sequenceAnimationOption.push_back(new Animation());
		Animation* newAnim = qc_cmd.sequenceAnimationOption.back();
		qc_cmd.sequence[g_num_sequence].panim[i] = newAnim;

		newAnim->startframe = start;
		newAnim->endframe = end;
		newAnim->flags = 0;

		cmd_sequence_option_animation(qc_cmd, smd_files[i], newAnim);

	}

	qc_cmd.sequence[g_num_sequence].numblends = smd_files.size(); // MAXSTUDIOBLENDS

	g_num_sequence++;

	return 0;
}

int cmd_controller(QC &qc_cmd, std::string &token)
{
	BoneController newbc;
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
				printf("unknown bonecontroller type '%s'\n", token);
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
			qc_cmd.bonecontrollers.push_back(newbc);
		}
	}
	return 1;
}

void cmd_bbox(QC &qc_cmd, std::string &token)
{	// min
	get_token(false, token);
	qc_cmd.bbox[0].x = std::stof(token);

	get_token(false, token);
	qc_cmd.bbox[0].y = std::stof(token);

	get_token(false, token);
	qc_cmd.bbox[0].z = std::stof(token);
	// max
	get_token(false, token);
	qc_cmd.bbox[1].x = std::stof(token);

	get_token(false, token);
	qc_cmd.bbox[1].y = std::stof(token);

	get_token(false, token);
	qc_cmd.bbox[1].z = std::stof(token);
}

void cmd_cbox(QC &qc_cmd, std::string &token)
{	// min
	get_token(false, token);
	qc_cmd.cbox[0].x = std::stof(token);

	get_token(false, token);
	qc_cmd.cbox[0].y = std::stof(token);

	get_token(false, token);
	qc_cmd.cbox[0].z = std::stof(token);
	// max
	get_token(false, token);
	qc_cmd.cbox[1].x = std::stof(token);

	get_token(false, token);
	qc_cmd.cbox[1].y = std::stof(token);

	get_token(false, token);
	qc_cmd.cbox[1].z = std::stof(token);
}

void cmd_mirror(QC &qc_cmd, std::string &token)
{	
	get_token(false, token);
	std::string bonename = token;
	qc_cmd.mirroredbones.push_back(bonename);
}

void cmd_gamma(QC &qc_cmd, std::string &token)
{
	get_token(false, token);
	qc_cmd.gamma = std::stof(token);
}

int cmd_texturegroup(QC &qc_cmd, std::string &token)
{
	int i;
	int depth = 0;
	int col_index = 0;
	int row_index = 0;

	if (g_texturescount == 0)
		error("texturegroups must follow model loading\n");

	if (!get_token(false, token))
		return 0;

	if (g_skinrefcount == 0)
		g_skinrefcount = g_texturescount;

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
			qc_cmd.texturegroup[row_index][col_index] = i;
			if (row_index != 0)
				g_textures[i].parent = qc_cmd.texturegroup[0][col_index];
			col_index++;
			qc_cmd.texturegroup_cols = col_index;
			qc_cmd.texturegroup_rows = row_index + 1;
		}
	}

	return 0;
}

int cmd_hitgroup(QC &qc_cmd, std::string &token)
{
	HitGroup newhg;
	get_token(false, token);
	newhg.group = std::stoi(token);
	get_token(false, token);
	newhg.name = token;
	qc_cmd.hitgroups.push_back(newhg);

	return 0;
}

int cmd_hitbox(QC &qc_cmd, std::string &token)
{
	HitBox newhb;
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

	qc_cmd.hitboxes.push_back(newhb);

	return 0;
}

int cmd_attachment(QC &qc_cmd, std::string &token)
{
	Attachment newattach;
	// index
	get_token(false, token);
	newattach.index = std::stoi(token);

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

	qc_cmd.attachments.push_back(newattach);
	return 0;
}

void cmd_renamebone(QC &qc_cmd, std::string &token)
{
	RenameBone rename;
	get_token(false, token);
	rename.from = token;
	get_token(false, token);
	rename.to = token;
	qc_cmd.renamebones.push_back(rename);
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
		printf("Texture '%s' has unknown render mode '%s'!\n", tex_name, token);
}

void parse_qc_file(QC &qc_cmd)
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

		// Process recognized commands
		if (token == "$modelname")
		{
			cmd_modelname(qc_cmd, token);
		}
		else if (token == "$cd")
		{
			if (iscdalreadyset)
				error("Two $cd in one model");
			iscdalreadyset = true;
			get_token(false, token);

			qc_cmd.cd = token;
			qc_cmd.cdAbsolute = std::filesystem::absolute(qc_cmd.cd);
		}
		else if (token == "$cdtexture")
		{
			if (iscdtexturealreadyset)
				error("Two $cdtexture in one model");
			iscdtexturealreadyset = true;
			get_token(false, token);
			qc_cmd.cdtexture = token;
		}
		else if (token == "$scale")
		{
			cmd_scale(qc_cmd, token);
		}
		else if (token == "$rotate") // XDM
		{
			cmd_rotate(qc_cmd, token);
		}
		else if (token == "$controller")
		{
			cmd_controller(qc_cmd, token);
		}
		else if (token == "$body")
		{
			cmd_body(qc_cmd, token);
		}
		else if (token == "$bodygroup")
		{
			cmd_bodygroup(qc_cmd, token);
		}
		else if (token == "$sequence")
		{
			cmd_sequence(qc_cmd, token);
		}
		else if (token == "$eyeposition")
		{
			cmd_eyeposition(qc_cmd, token);
		}
		else if (token == "$origin")
		{
			cmd_origin(qc_cmd, token);
		}
		else if (token == "$bbox")
		{
			cmd_bbox(qc_cmd, token);
		}
		else if (token == "$cbox")
		{
			cmd_cbox(qc_cmd, token);
		}
		else if (token == "$mirrorbone")
		{
			cmd_mirror(qc_cmd, token);
		}
		else if (token == "$gamma")
		{
			cmd_gamma(qc_cmd, token);
		}
		else if (token == "$flags")
		{
			cmd_flags(qc_cmd, token);
		}
		else if (token == "$texturegroup")
		{
			cmd_texturegroup(qc_cmd, token);
		}
		else if (token == "$hgroup")
		{
			cmd_hitgroup(qc_cmd, token);
		}
		else if (token == "$hbox")
		{
			cmd_hitbox(qc_cmd, token);
		}
		else if (token == "$attachment")
		{
			cmd_attachment(qc_cmd, token);
		}
		else if (token == "$renamebone")
		{
			cmd_renamebone(qc_cmd, token);
		}
		else if (token == "$texrendermode")
		{
			cmd_texrendermode(token);
		}
		else
		{
			error("Incorrect/Unsupported command %s\n", token.c_str());
		}
	}
}


int main(int argc, char **argv)
{
    char path[1024] = {0};
    static QC qc_cmd;

    g_flaginvertnormals = false;
    g_flagkeepallbones = false;
    g_flagnormalblendangle = cosf(to_radians(0));

    if (argc == 1)
    {
        error("Usage: studiomdl <inputfile.qc> <flags>\n"
              "  Flags:\n"
              "    [-f]                Invert normals\n"
              "    [-a <angle>]        Set vertex normal blend angle override\n"
              "    [-b]                Keep all unused bones\n");
    }

    const char *ext = strrchr(argv[1], '.');
    if (!ext || strcmp(ext, ".qc") != 0)
    {
        error("Error: The first argument must be a .qc file\n");
    }
    std::strcpy(path, argv[1]);

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
                    error("Error: Missing value for -a flag.\n");
                }
                i++;
                try
                {
                    g_flagnormalblendangle = cosf(to_radians(std::stof(argv[i])));
                }
                catch (...)
                {
                    error("Error: Invalid value for -a flag. Expected a numeric angle.\n");
                }
                break;
            case 'b':
                g_flagkeepallbones = true;
                break;
            default:
                error("Error: Unknown flag '%s'.\n", argv[i]);
            }
        }
        else
        {
            error("Error: Unexpected argument '%s'.\n", argv[i]);
        }
    }

    load_qc_file(path);       
    parse_qc_file(qc_cmd); 
    set_skin_values(qc_cmd); 
    simplify_model(qc_cmd); 
    write_file(qc_cmd);      

    return 0;
}
