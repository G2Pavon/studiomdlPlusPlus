// studiomdl.c: generates a studio .mdl file from a .qc script
// models/<scriptname>.mdl.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>
#include <string>
#include <filesystem>

#include "cmdlib.hpp"
#include "qctokenizer.hpp"
#include "mathlib.hpp"
#define extern // ??
#include "studio.hpp"
#include "studiomdl.hpp"
#include "monsters/activity.hpp"
#include "monsters/activitymap.hpp"
#include "writemdl.hpp"

#define strnicmp strncasecmp
#define stricmp strcasecmp
#define strcpyn(a, b) std::strncpy(a, b, sizeof(a))

constexpr float ENGINE_ORIENTATION = 90.0f; // Z rotation to match with engine forward direction (x axis)

// studiomdl.exe args -----------
int g_flagreversedtriangles;
int g_flagbadnormals;
int g_flagfliptriangles;
float g_flagnormalblendangle;
int g_flagdumphitboxes;
int g_flagignorewarnings;
bool g_flagkeepallbones;

// QC Command variables -----------------
std::filesystem::path g_cdCommand;				   // $cd
std::filesystem::path g_cdCommandAbsolute;		   // $cd
std::filesystem::path g_cdtextureCommand;		   // $cdtexture
float g_scaleCommand;							   // $scale
float g_scaleBodyAndSequenceOption;				   // $body studio <value> // also for $sequence
Vector3 g_originCommand;						   // $origin
float g_originCommandRotation;					   // $origin <X> <Y> <Z> <rotation>
float g_rotateCommand;							   // $rotate and $sequence <sequence name> <SMD path> {[rotate <zrotation>]} only z axis
Vector3 g_sequenceOrigin;						   // $sequence <sequence name> <SMD path>  {[origin <X> <Y> <Z>]}
float g_gammaCommand;							   // $$gamma
RenameBone g_renameboneCommand[MAXSTUDIOSRCBONES]; // $renamebone
int g_renamebonecount;
HitGroup g_hitgroupCommand[MAXSTUDIOSRCBONES]; // $hgroup
int g_hitgroupscount;
char g_mirrorboneCommand[MAXSTUDIOSRCBONES][64]; // $mirrorbone
int g_mirroredcount;
Animation *g_animationSequenceOption[MAXSTUDIOSEQUENCES * MAXSTUDIOBLENDS]; // $sequence, each sequence can have 16 blends
int g_animationcount;
int g_texturegroupCommand[32][32][32]; // $texturegroup
int g_texturegroupCount;			   // unnecessary? since engine doesn't support multiple texturegroups
int g_texturegrouplayers[32];
int g_texturegroupreps[32];

// SMD variables --------------------------
std::filesystem::path g_smdpath;
FILE *g_smdfile;
char g_currentsmdline[1024];
int g_smdlinecount;

char g_defaulttextures[16][256];
char g_sourcetexture[16][256];
int g_numtextureteplacements;
BoneFixUp g_bonefixup[MAXSTUDIOSRCBONES];

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

void extract_motion()
{
	int i, j, k;
	int blendIndex;

	// extract linear motion
	for (i = 0; i < g_sequencecount; i++)
	{
		if (g_sequenceCommand[i].numframes > 1)
		{
			// assume 0 for now.
			int typeMotion;
			Vector3 *ptrPos;
			Vector3 motion = {0, 0, 0};
			typeMotion = g_sequenceCommand[i].motiontype;
			ptrPos = g_sequenceCommand[i].panim[0]->pos[0];

			k = g_sequenceCommand[i].numframes - 1;

			if (typeMotion & STUDIO_LX)
				motion[0] = ptrPos[k][0] - ptrPos[0][0];
			if (typeMotion & STUDIO_LY)
				motion[1] = ptrPos[k][1] - ptrPos[0][1];
			if (typeMotion & STUDIO_LZ)
				motion[2] = ptrPos[k][2] - ptrPos[0][2];

			for (j = 0; j < g_sequenceCommand[i].numframes; j++)
			{
				Vector3 adjustedPosition;
				for (k = 0; k < g_sequenceCommand[i].panim[0]->numbones; k++)
				{
					if (g_sequenceCommand[i].panim[0]->node[k].parent == -1)
					{
						ptrPos = g_sequenceCommand[i].panim[0]->pos[k];

						adjustedPosition = motion * (static_cast<float>(j) / static_cast<float>(g_sequenceCommand[i].numframes - 1));
						for (blendIndex = 0; blendIndex < g_sequenceCommand[i].numblends; blendIndex++)
						{
							g_sequenceCommand[i].panim[blendIndex]->pos[k][j] = g_sequenceCommand[i].panim[blendIndex]->pos[k][j] - adjustedPosition;
						}
					}
				}
			}

			g_sequenceCommand[i].linearmovement = motion;
		}
		else
		{ //
			g_sequenceCommand[i].linearmovement = g_sequenceCommand[i].linearmovement - g_sequenceCommand[i].linearmovement;
		}
	}

	// extract unused motion
	for (i = 0; i < g_sequencecount; i++)
	{
		int typeUnusedMotion;
		typeUnusedMotion = g_sequenceCommand[i].motiontype;
		for (k = 0; k < g_sequenceCommand[i].panim[0]->numbones; k++)
		{
			if (g_sequenceCommand[i].panim[0]->node[k].parent == -1)
			{
				for (blendIndex = 0; blendIndex < g_sequenceCommand[i].numblends; blendIndex++)
				{
					float motion[6];
					motion[0] = g_sequenceCommand[i].panim[blendIndex]->pos[k][0][0];
					motion[1] = g_sequenceCommand[i].panim[blendIndex]->pos[k][0][1];
					motion[2] = g_sequenceCommand[i].panim[blendIndex]->pos[k][0][2];
					motion[3] = g_sequenceCommand[i].panim[blendIndex]->rot[k][0][0];
					motion[4] = g_sequenceCommand[i].panim[blendIndex]->rot[k][0][1];
					motion[5] = g_sequenceCommand[i].panim[blendIndex]->rot[k][0][2];

					for (j = 0; j < g_sequenceCommand[i].numframes; j++)
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
							g_sequenceCommand[i].panim[blendIndex]->rot[k][j][0] = motion[3];
						if (typeUnusedMotion & STUDIO_YR)
							g_sequenceCommand[i].panim[blendIndex]->rot[k][j][1] = motion[4];
						if (typeUnusedMotion & STUDIO_ZR)
							g_sequenceCommand[i].panim[blendIndex]->rot[k][j][2] = motion[5];
					}
				}
			}
		}
	}

	// extract auto motion
	for (i = 0; i < g_sequencecount; i++)
	{
		// assume 0 for now.
		int typeAutoMotion;
		Vector3 *ptrAutoPos;
		Vector3 *ptrAutoRot;
		Vector3 motion = {0, 0, 0};
		Vector3 angles = {0, 0, 0};

		typeAutoMotion = g_sequenceCommand[i].motiontype;
		for (j = 0; j < g_sequenceCommand[i].numframes; j++)
		{
			ptrAutoPos = g_sequenceCommand[i].panim[0]->pos[0];
			ptrAutoRot = g_sequenceCommand[i].panim[0]->rot[0];

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

			g_sequenceCommand[i].automovepos[j] = motion;
			g_sequenceCommand[i].automoveangle[j] = angles;
		}
	}
}

void optimize_animations()
{
	int startFrame, endFrame;
	int typeMotion;
	int iError = 0;

	// optimize animations
	for (int i = 0; i < g_sequencecount; i++)
	{
		g_sequenceCommand[i].numframes = g_sequenceCommand[i].panim[0]->endframe - g_sequenceCommand[i].panim[0]->startframe + 1;

		// force looping animations to be looping
		if (g_sequenceCommand[i].flags & STUDIO_LOOPING)
		{
			for (int j = 0; j < g_sequenceCommand[i].panim[0]->numbones; j++)
			{
				for (int blends = 0; blends < g_sequenceCommand[i].numblends; blends++)
				{
					Vector3 *ppos = g_sequenceCommand[i].panim[blends]->pos[j];
					Vector3 *prot = g_sequenceCommand[i].panim[blends]->rot[j];

					startFrame = 0;								   // sequence[i].panim[q]->startframe;
					endFrame = g_sequenceCommand[i].numframes - 1; // sequence[i].panim[q]->endframe;

					typeMotion = g_sequenceCommand[i].motiontype;
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

		for (int j = 0; j < g_sequenceCommand[i].numevents; j++)
		{
			if (g_sequenceCommand[i].event[j].frame < g_sequenceCommand[i].panim[0]->startframe)
			{
				printf("sequence %s has event (%d) before first frame (%d)\n", g_sequenceCommand[i].name, g_sequenceCommand[i].event[j].frame, g_sequenceCommand[i].panim[0]->startframe);
				g_sequenceCommand[i].event[j].frame = g_sequenceCommand[i].panim[0]->startframe;
				iError++;
			}
			if (g_sequenceCommand[i].event[j].frame > g_sequenceCommand[i].panim[0]->endframe)
			{
				printf("sequence %s has event (%d) after last frame (%d)\n", g_sequenceCommand[i].name, g_sequenceCommand[i].event[j].frame, g_sequenceCommand[i].panim[0]->endframe);
				g_sequenceCommand[i].event[j].frame = g_sequenceCommand[i].panim[0]->endframe;
				iError++;
			}
		}

		g_sequenceCommand[i].frameoffset = g_sequenceCommand[i].panim[0]->startframe;
	}
}

int find_node(char *name)
{
	for (int k = 0; k < g_bonescount; k++)
	{
		if (std::strcmp(g_bonetable[k].name, name) == 0)
		{
			return k;
		}
	}
	return -1;
}

void make_transitions()
{
	int i, j;
	int iHit;

	// Add in direct node transitions
	for (i = 0; i < g_sequencecount; i++)
	{
		if (g_sequenceCommand[i].entrynode != g_sequenceCommand[i].exitnode)
		{
			g_xnode[g_sequenceCommand[i].entrynode - 1][g_sequenceCommand[i].exitnode - 1] = g_sequenceCommand[i].exitnode;
			if (g_sequenceCommand[i].nodeflags)
			{
				g_xnode[g_sequenceCommand[i].exitnode - 1][g_sequenceCommand[i].entrynode - 1] = g_sequenceCommand[i].entrynode;
			}
		}
		if (g_sequenceCommand[i].entrynode > g_numxnodes)
			g_numxnodes = g_sequenceCommand[i].entrynode;
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

void simplify_model()
{
	int i, j, k;
	int n, m, q;
	Vector3 *defaultpos[MAXSTUDIOSRCBONES] = {0};
	Vector3 *defaultrot[MAXSTUDIOSRCBONES] = {0};
	int iError = 0;

	optimize_animations();
	extract_motion();
	make_transitions();

	// find used bones
	for (i = 0; i < g_submodelscount; i++)
	{
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			g_submodel[i]->boneref[k] = g_flagkeepallbones;
		}
		for (j = 0; j < g_submodel[i]->numverts; j++)
		{
			g_submodel[i]->boneref[g_submodel[i]->vert[j].bone] = 1;
		}
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			// tag parent bones as used if child has been used
			if (g_submodel[i]->boneref[k])
			{
				n = g_submodel[i]->node[k].parent;
				while (n != -1 && !g_submodel[i]->boneref[n])
				{
					g_submodel[i]->boneref[n] = 1;
					n = g_submodel[i]->node[n].parent;
				}
			}
		}
	}

	// rename model bones if needed
	for (i = 0; i < g_submodelscount; i++)
	{
		for (j = 0; j < g_submodel[i]->numbones; j++)
		{
			for (k = 0; k < g_renamebonecount; k++)
			{
				if (!std::strcmp(g_submodel[i]->node[j].name, g_renameboneCommand[k].from))
				{
					std::strcpy(g_submodel[i]->node[j].name, g_renameboneCommand[k].to);
					break;
				}
			}
		}
	}

	// union of all used bones
	g_bonescount = 0;
	for (i = 0; i < g_submodelscount; i++)
	{
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			g_submodel[i]->boneimap[k] = -1;
		}
		for (j = 0; j < g_submodel[i]->numbones; j++)
		{
			if (g_submodel[i]->boneref[j])
			{
				k = find_node(g_submodel[i]->node[j].name);
				if (k == -1)
				{
					// create new bone
					k = g_bonescount;
					strcpyn(g_bonetable[k].name, g_submodel[i]->node[j].name);
					if ((n = g_submodel[i]->node[j].parent) != -1)
						g_bonetable[k].parent = find_node(g_submodel[i]->node[n].name);
					else
						g_bonetable[k].parent = -1;
					g_bonetable[k].bonecontroller = 0;
					g_bonetable[k].flags = 0;
					// set defaults
					defaultpos[k] = (Vector3 *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(Vector3));
					defaultrot[k] = (Vector3 *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(Vector3));
					for (n = 0; n < MAXSTUDIOANIMATIONS; n++)
					{
						defaultpos[k][n] = g_submodel[i]->skeleton[j].pos;
						defaultrot[k][n] = g_submodel[i]->skeleton[j].rot;
					}
					g_bonetable[k].pos = g_submodel[i]->skeleton[j].pos;
					g_bonetable[k].rot = g_submodel[i]->skeleton[j].rot;
					g_bonescount++;
				}
				else
				{
					// double check parent assignments
					n = g_submodel[i]->node[j].parent;
					if (n != -1)
						n = find_node(g_submodel[i]->node[n].name);
					m = g_bonetable[k].parent;

					if (n != m)
					{
						printf("illegal parent bone replacement in model \"%s\"\n\t\"%s\" has \"%s\", previously was \"%s\"\n",
							   g_submodel[i]->name,
							   g_submodel[i]->node[j].name,
							   (n != -1) ? g_bonetable[n].name : "ROOT",
							   (m != -1) ? g_bonetable[m].name : "ROOT");
						iError++;
					}
				}
				g_submodel[i]->bonemap[j] = k;
				g_submodel[i]->boneimap[k] = j;
			}
		}
	}

	if (iError && !(g_flagignorewarnings))
	{
		exit(1);
	}

	if (g_bonescount >= MAXSTUDIOBONES)
	{
		error("Too many bones used in model, used %d, max %d\n", g_bonescount, MAXSTUDIOBONES);
	}

	// rename sequence bones if needed
	for (i = 0; i < g_sequencecount; i++)
	{
		for (j = 0; j < g_sequenceCommand[i].panim[0]->numbones; j++)
		{
			for (k = 0; k < g_renamebonecount; k++)
			{
				if (!std::strcmp(g_sequenceCommand[i].panim[0]->node[j].name, g_renameboneCommand[k].from))
				{
					std::strcpy(g_sequenceCommand[i].panim[0]->node[j].name, g_renameboneCommand[k].to);
					break;
				}
			}
		}
	}

	// map each sequences bone list to master list
	for (i = 0; i < g_sequencecount; i++)
	{
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			g_sequenceCommand[i].panim[0]->boneimap[k] = -1;
		}
		for (j = 0; j < g_sequenceCommand[i].panim[0]->numbones; j++)
		{
			k = find_node(g_sequenceCommand[i].panim[0]->node[j].name);

			if (k == -1)
			{
				g_sequenceCommand[i].panim[0]->bonemap[j] = -1;
			}
			else
			{
				char *szAnim = "ROOT";
				char *szNode = "ROOT";

				// whoa, check parent connections!
				if (g_sequenceCommand[i].panim[0]->node[j].parent != -1)
					szAnim = g_sequenceCommand[i].panim[0]->node[g_sequenceCommand[i].panim[0]->node[j].parent].name;

				if (g_bonetable[k].parent != -1)
					szNode = g_bonetable[g_bonetable[k].parent].name;

				if (std::strcmp(szAnim, szNode))
				{
					printf("illegal parent bone replacement in sequence \"%s\"\n\t\"%s\" has \"%s\", reference has \"%s\"\n",
						   g_sequenceCommand[i].name,
						   g_sequenceCommand[i].panim[0]->node[j].name,
						   szAnim,
						   szNode);
					iError++;
				}
				g_sequenceCommand[i].panim[0]->bonemap[j] = k;
				g_sequenceCommand[i].panim[0]->boneimap[k] = j;
			}
		}
	}
	if (iError && !(g_flagignorewarnings))
	{
		exit(1);
	}

	// link bonecontrollers
	for (i = 0; i < g_bonecontrollerscount; i++)
	{
		for (j = 0; j < g_bonescount; j++)
		{
			if (stricmp(g_bonecontrollerCommand[i].name, g_bonetable[j].name) == 0)
				break;
		}
		if (j >= g_bonescount)
		{
			error("unknown bonecontroller link '%s'\n", g_bonecontrollerCommand[i].name);
		}
		g_bonecontrollerCommand[i].bone = j;
	}

	// link attachments
	for (i = 0; i < g_attachmentscount; i++)
	{
		for (j = 0; j < g_bonescount; j++)
		{
			if (stricmp(g_attachmentCommand[i].bonename, g_bonetable[j].name) == 0)
				break;
		}
		if (j >= g_bonescount)
		{
			error("unknown attachment link '%s'\n", g_attachmentCommand[i].bonename);
		}
		g_attachmentCommand[i].bone = j;
	}

	// relink model
	for (i = 0; i < g_submodelscount; i++)
	{
		for (j = 0; j < g_submodel[i]->numverts; j++)
		{
			g_submodel[i]->vert[j].bone = g_submodel[i]->bonemap[g_submodel[i]->vert[j].bone];
		}
		for (j = 0; j < g_submodel[i]->numnorms; j++)
		{
			g_submodel[i]->normal[j].bone = g_submodel[i]->bonemap[g_submodel[i]->normal[j].bone];
		}
	}

	// set hitgroups
	for (k = 0; k < g_bonescount; k++)
	{
		g_bonetable[k].group = -9999;
	}
	for (j = 0; j < g_hitgroupscount; j++)
	{
		for (k = 0; k < g_bonescount; k++)
		{
			if (std::strcmp(g_bonetable[k].name, g_hitgroupCommand[j].name) == 0)
			{
				g_bonetable[k].group = g_hitgroupCommand[j].group;
				break;
			}
		}
		if (k >= g_bonescount)
			error("cannot find bone %s for hitgroup %d\n", g_hitgroupCommand[j].name, g_hitgroupCommand[j].group);
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

	if (g_hitboxescount == 0)
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
		for (i = 0; i < g_submodelscount; i++)
		{
			Vector3 p;
			for (j = 0; j < g_submodel[i]->numverts; j++)
			{
				p = g_submodel[i]->vert[j].org;
				k = g_submodel[i]->vert[j].bone;

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
				g_hitboxCommand[g_hitboxescount].bone = k;
				g_hitboxCommand[g_hitboxescount].group = g_bonetable[k].group;
				g_hitboxCommand[g_hitboxescount].bmin = g_bonetable[k].bmin;
				g_hitboxCommand[g_hitboxescount].bmax = g_bonetable[k].bmax;

				if (g_flagdumphitboxes)
				{
					printf("$hbox %d \"%s\" %.2f %.2f %.2f  %.2f %.2f %.2f\n",
						   g_hitboxCommand[g_hitboxescount].group,
						   g_bonetable[g_hitboxCommand[g_hitboxescount].bone].name,
						   g_hitboxCommand[g_hitboxescount].bmin[0], g_hitboxCommand[g_hitboxescount].bmin[1], g_hitboxCommand[g_hitboxescount].bmin[2],
						   g_hitboxCommand[g_hitboxescount].bmax[0], g_hitboxCommand[g_hitboxescount].bmax[1], g_hitboxCommand[g_hitboxescount].bmax[2]);
				}
				g_hitboxescount++;
			}
		}
	}
	else
	{
		for (j = 0; j < g_hitboxescount; j++)
		{
			for (k = 0; k < g_bonescount; k++)
			{
				if (std::strcmp(g_bonetable[k].name, g_hitboxCommand[j].name) == 0)
				{
					g_hitboxCommand[j].bone = k;
					break;
				}
			}
			if (k >= g_bonescount)
				error("cannot find bone %s for bbox\n", g_hitboxCommand[j].name);
		}
	}

	// relink animations
	for (i = 0; i < g_sequencecount; i++)
	{
		Vector3 *origpos[MAXSTUDIOSRCBONES] = {0};
		Vector3 *origrot[MAXSTUDIOSRCBONES] = {0};

		for (q = 0; q < g_sequenceCommand[i].numblends; q++)
		{
			// save pointers to original animations
			for (j = 0; j < g_sequenceCommand[i].panim[q]->numbones; j++)
			{
				origpos[j] = g_sequenceCommand[i].panim[q]->pos[j];
				origrot[j] = g_sequenceCommand[i].panim[q]->rot[j];
			}

			for (j = 0; j < g_bonescount; j++)
			{
				if ((k = g_sequenceCommand[i].panim[0]->boneimap[j]) >= 0)
				{
					// link to original animations
					g_sequenceCommand[i].panim[q]->pos[j] = origpos[k];
					g_sequenceCommand[i].panim[q]->rot[j] = origrot[k];
				}
				else
				{
					// link to dummy animations
					g_sequenceCommand[i].panim[q]->pos[j] = defaultpos[j];
					g_sequenceCommand[i].panim[q]->rot[j] = defaultrot[j];
				}
			}
		}
	}

	// find scales for all bones
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

			for (i = 0; i < g_sequencecount; i++)
			{
				for (q = 0; q < g_sequenceCommand[i].numblends; q++)
				{
					for (n = 0; n < g_sequenceCommand[i].numframes; n++)
					{
						float v;
						switch (k)
						{
						case 0:
						case 1:
						case 2:
							v = (g_sequenceCommand[i].panim[q]->pos[j][n][k] - g_bonetable[j].pos[k]);
							break;
						case 3:
						case 4:
						case 5:
							v = (g_sequenceCommand[i].panim[q]->rot[j][n][k - 3] - g_bonetable[j].rot[k - 3]);
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

	// find bounding box for each sequence
	for (i = 0; i < g_sequencecount; i++)
	{
		Vector3 bmin, bmax;

		// find intersection box volume for each bone
		for (j = 0; j < 3; j++)
		{
			bmin[j] = 9999.0;
			bmax[j] = -9999.0;
		}

		for (q = 0; q < g_sequenceCommand[i].numblends; q++)
		{
			for (n = 0; n < g_sequenceCommand[i].numframes; n++)
			{
				float bonetransform[MAXSTUDIOBONES][3][4]; // bone transformation matrix
				float bonematrix[3][4];					   // local transformation matrix
				Vector3 pos;

				for (j = 0; j < g_bonescount; j++)
				{

					Vector3 angles;
					angles[0] = to_degrees(g_sequenceCommand[i].panim[q]->rot[j][n][0]);
					angles[1] = to_degrees(g_sequenceCommand[i].panim[q]->rot[j][n][1]);
					angles[2] = to_degrees(g_sequenceCommand[i].panim[q]->rot[j][n][2]);

					angle_matrix(angles, bonematrix);

					bonematrix[0][3] = g_sequenceCommand[i].panim[q]->pos[j][n][0];
					bonematrix[1][3] = g_sequenceCommand[i].panim[q]->pos[j][n][1];
					bonematrix[2][3] = g_sequenceCommand[i].panim[q]->pos[j][n][2];

					if (g_bonetable[j].parent == -1)
					{
						matrix_copy(bonematrix, bonetransform[j]);
					}
					else
					{
						r_concat_transforms(bonetransform[g_bonetable[j].parent], bonematrix, bonetransform[j]);
					}
				}

				for (k = 0; k < g_submodelscount; k++)
				{
					for (j = 0; j < g_submodel[k]->numverts; j++)
					{
						vector_transform(g_submodel[k]->vert[j].org, bonetransform[g_submodel[k]->vert[j].bone], pos);

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

		g_sequenceCommand[i].bmin = bmin;
		g_sequenceCommand[i].bmax = bmax;
	}

	// reduce animations
	{
		int total = 0;
		int changes = 0;
		int p;

		for (i = 0; i < g_sequencecount; i++)
		{
			for (q = 0; q < g_sequenceCommand[i].numblends; q++)
			{
				for (j = 0; j < g_bonescount; j++)
				{
					for (k = 0; k < DEGREESOFFREEDOM; k++)
					{
						StudioAnimationValue *pcount, *pvalue;
						float v;
						short value[MAXSTUDIOANIMATIONS];
						StudioAnimationValue data[MAXSTUDIOANIMATIONS];

						for (n = 0; n < g_sequenceCommand[i].numframes; n++)
						{
							switch (k)
							{
							case 0:
							case 1:
							case 2:
								value[n] = static_cast<short>((g_sequenceCommand[i].panim[q]->pos[j][n][k] - g_bonetable[j].pos[k]) / g_bonetable[j].posscale[k]);
								break;
							case 3:
							case 4:
							case 5:
								v = (g_sequenceCommand[i].panim[q]->rot[j][n][k - 3] - g_bonetable[j].rot[k - 3]);
								if (v >= Q_PI)
									v -= Q_PI * 2;
								if (v < -Q_PI)
									v += Q_PI * 2;

								value[n] = static_cast<short>(v / g_bonetable[j].rotscale[k - 3]);
								break;
							}
						}
						if (n == 0)
							error("no animation frames: \"%s\"\n", g_sequenceCommand[i].name);

						g_sequenceCommand[i].panim[q]->numanim[j][k] = 0;

						std::memset(data, 0, sizeof(data));
						pcount = data;
						pvalue = pcount + 1;

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
								total++;
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

						g_sequenceCommand[i].panim[q]->numanim[j][k] = static_cast<int>(pvalue - data);
						if (g_sequenceCommand[i].panim[q]->numanim[j][k] == 2 && value[0] == 0)
						{
							g_sequenceCommand[i].panim[q]->numanim[j][k] = 0;
						}
						else
						{
							g_sequenceCommand[i].panim[q]->anim[j][k] = (StudioAnimationValue *)std::calloc(pvalue - data, sizeof(StudioAnimationValue));
							std::memcpy(g_sequenceCommand[i].panim[q]->anim[j][k], data, (pvalue - data) * sizeof(StudioAnimationValue));
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

// search case-insensitive for string2 in string
char *stristr(const char *string, const char *string2)
{
	int c, len;
	c = tolower(*string2);
	len = strlen(string2);

	while (string)
	{
		for (; *string && tolower(*string) != c; string++)
			;
		if (*string)
		{
			if (strnicmp(string, string2, len) == 0)
			{
				break;
			}
			string++;
		}
		else
		{
			return NULL;
		}
	}
	return (char *)string;
}

int find_texture_index(std::string texturename)
{
	int i;
	for (i = 0; i < g_texturescount; i++)
	{
		if (stricmp(g_texture[i].name, texturename.c_str()) == 0)
		{
			return i;
		}
	}

	strcpyn(g_texture[i].name, texturename.c_str());

	// XDM: allow such names as "tex_chrome_bright" - chrome and full brightness effects
	if (stristr(texturename.c_str(), "chrome") != NULL)
		g_texture[i].flags = STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME;
	else if (stristr(texturename.c_str(), "bright") != NULL)
		g_texture[i].flags = STUDIO_NF_FLATSHADE | STUDIO_NF_FULLBRIGHT;
	else
		g_texture[i].flags = 0;

	g_texturescount++;
	return i;
}

Mesh *find_mesh_by_texture(Model *pmodel, char *texturename)
{
	int i, j;

	j = find_texture_index(std::string(texturename));

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

int finx_vertex_normal_index(Model *pmodel, Normal *pnormal)
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

void adjust_vertex_to_origin(Vector3 org)
{
	org[0] = (org[0] - g_sequenceOrigin[0]);
	org[1] = (org[1] - g_sequenceOrigin[1]);
	org[2] = (org[2] - g_sequenceOrigin[2]);
}

void ScaleVertexByQcScale(Vector3 org)
{
	org[0] = org[0] * g_scaleBodyAndSequenceOption;
	org[1] = org[1] * g_scaleBodyAndSequenceOption;
	org[2] = org[2] * g_scaleBodyAndSequenceOption;
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
	int result;

	if (result = load_bmp(filename, &ptexture->ppicture, (std::uint8_t **)&ptexture->ppal, &ptexture->srcwidth, &ptexture->srcheight))
	{
		error("error %d reading BMP image \"%s\"\n", result, filename);
	}
}

void resize_texture(Texture *ptexture)
{
	int i, t;
	std::uint8_t *pdest;
	int srcadjustedwidth;

	// Keep the original texture without resizing to avoid uv shift
	ptexture->skintop = static_cast<int>(ptexture->min_t);
	ptexture->skinleft = static_cast<int>(ptexture->min_s);
	ptexture->skinwidth = ptexture->srcwidth;
	ptexture->skinheight = ptexture->srcheight;

	ptexture->size = ptexture->skinwidth * ptexture->skinheight + 256 * 3;

	printf("BMP %s [%d %d] (%.0f%%)  %6d bytes\n", ptexture->name, ptexture->skinwidth, ptexture->skinheight,
		   (static_cast<float>(ptexture->skinwidth * ptexture->skinheight) / static_cast<float>(ptexture->srcwidth * ptexture->srcheight)) * 100.0f,
		   ptexture->size);

	if (ptexture->size > 1024 * 1024)
	{
		printf("%.0f %.0f %.0f %.0f\n", ptexture->min_s, ptexture->max_s, ptexture->min_t, ptexture->max_t);
		error("texture too large\n");
	}
	pdest = (std::uint8_t *)malloc(ptexture->size);
	ptexture->pdata = pdest;

	// Data is saved as a multiple of 4
	srcadjustedwidth = (ptexture->srcwidth + 3) & ~3;

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
	if (g_gammaCommand != 1.8f)
	// gamma correct the monster textures to a gamma of 1.8
	{
		float g;
		std::uint8_t *psrc = (std::uint8_t *)ptexture->ppal;
		g = g_gammaCommand / 1.8f;
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

void grab_skin(Texture *ptexture)
{
	std::filesystem::path textureFilePath = g_cdtextureCommand / ptexture->name;
	if (!std::filesystem::exists(textureFilePath))
	{
		textureFilePath = g_cdCommandAbsolute / ptexture->name;
		if (!std::filesystem::exists(textureFilePath))
		{
			error("cannot find %s texture in \"%s\" nor \"%s\"\nor those path don't exist\n", ptexture->name, g_cdtextureCommand.c_str(), g_cdCommandAbsolute.c_str());
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

void set_skin_values()
{
	int i, j;

	for (i = 0; i < g_texturescount; i++)
	{
		grab_skin(&g_texture[i]);

		g_texture[i].max_s = -9999999;
		g_texture[i].min_s = 9999999;
		g_texture[i].max_t = -9999999;
		g_texture[i].min_t = 9999999;
	}

	for (i = 0; i < g_submodelscount; i++)
	{
		for (j = 0; j < g_submodel[i]->nummesh; j++)
		{
			texture_coord_ranges(g_submodel[i]->pmesh[j], &g_texture[g_submodel[i]->pmesh[j]->skinref]);
		}
	}

	for (i = 0; i < g_texturescount; i++)
	{
		if (g_texture[i].max_s < g_texture[i].min_s)
		{
			// must be a replacement texture
			if (g_texture[i].flags & STUDIO_NF_CHROME)
			{
				g_texture[i].max_s = 63;
				g_texture[i].min_s = 0;
				g_texture[i].max_t = 63;
				g_texture[i].min_t = 0;
			}
			else
			{
				g_texture[i].max_s = g_texture[g_texture[i].parent].max_s;
				g_texture[i].min_s = g_texture[g_texture[i].parent].min_s;
				g_texture[i].max_t = g_texture[g_texture[i].parent].max_t;
				g_texture[i].min_t = g_texture[g_texture[i].parent].min_t;
			}
		}

		resize_texture(&g_texture[i]);
	}

	for (i = 0; i < g_submodelscount; i++)
	{
		for (j = 0; j < g_submodel[i]->nummesh; j++)
		{
			reset_texture_coord_ranges(g_submodel[i]->pmesh[j], &g_texture[g_submodel[i]->pmesh[j]->skinref]);
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
	for (i = 0; i < g_texturegrouplayers[0]; i++)
	{
		for (j = 0; j < g_texturegroupreps[0]; j++)
		{
			g_skinref[i][g_texturegroupCommand[0][0][j]] = g_texturegroupCommand[0][i][j];
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
		int parent;

		// convert to degrees
		boneAngle[0] = to_degrees(pmodel->skeleton[i].rot[0]);
		boneAngle[1] = to_degrees(pmodel->skeleton[i].rot[1]);
		boneAngle[2] = to_degrees(pmodel->skeleton[i].rot[2]);

		parent = pmodel->node[i].parent;
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

void grab_smd_triangles(Model *pmodel)
{
	int i;
	int trianglesCount = 0;
	int badNormalsCount = 0;
	Vector3 vmin, vmax;

	vmin[0] = vmin[1] = vmin[2] = 99999;
	vmax[0] = vmax[1] = vmax[2] = -99999;

	build_reference(pmodel);

	// load the base triangles
	while (true)
	{
		if (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != NULL)
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
				if (g_flagfliptriangles)
					// quake wants them in the reverse order
					ptriangleVert = find_mesh_triangle_by_index(pmesh, pmesh->numtris) + 2 - j;
				else
					ptriangleVert = find_mesh_triangle_by_index(pmesh, pmesh->numtris) + j;

				if (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != NULL)
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

						adjust_vertex_to_origin(triangleVertex.org);
						ScaleVertexByQcScale(triangleVertex.org);

						// move vertex position to object space.
						Vector3 tmp;
						tmp = triangleVertex.org - g_bonefixup[triangleVertex.bone].worldorg;
						vector_transform(tmp, g_bonefixup[triangleVertex.bone].im, triangleVertex.org);

						// move normal to object space.
						tmp = triangleNormal.org;
						vector_transform(tmp, g_bonefixup[triangleVertex.bone].im, triangleNormal.org);
						triangleNormal.org.normalize();

						ptriangleVert->normindex = finx_vertex_normal_index(pmodel, &triangleNormal);
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

			if (g_flagreversedtriangles || g_flagbadnormals)
			{
				// check triangle direction

				if (triangleNormals[0].dot(triangleNormals[1]) < 0.0 || triangleNormals[1].dot(triangleNormals[2]) < 0.0 || triangleNormals[2].dot(triangleNormals[0]) < 0.0)
				{
					badNormalsCount++;

					if (g_flagbadnormals)
					{
						// steal the triangle and make it white
						TriangleVert *ptriv2;
						pmesh = find_mesh_by_texture(pmodel, "..\\white.bmp");
						ptriv2 = find_mesh_triangle_by_index(pmesh, pmesh->numtris);

						ptriv2[0] = ptriangleVert[0];
						ptriv2[1] = ptriangleVert[1];
						ptriv2[2] = ptriangleVert[2];
					}
				}
				else
				{
					Vector3 triangleEdge1, triangleEdge2, surfaceNormal;
					float x, y, z;

					triangleEdge1 = triangleVertices[1] - triangleVertices[0];
					triangleEdge2 = triangleVertices[2] - triangleVertices[0];
					surfaceNormal = triangleEdge1.cross(triangleEdge2);
					surfaceNormal.normalize();

					x = surfaceNormal.dot(triangleNormals[0]);
					y = surfaceNormal.dot(triangleNormals[1]);
					z = surfaceNormal.dot(triangleNormals[2]);
					if (x < 0.0 || y < 0.0 || z < 0.0)
					{
						if (g_flagreversedtriangles)
						{
							// steal the triangle and make it white
							TriangleVert *ptriv2;

							printf("triangle reversed (%f %f %f)\n",
								   triangleNormals[0].dot(triangleNormals[1]),
								   triangleNormals[1].dot(triangleNormals[2]),
								   triangleNormals[2].dot(triangleNormals[0]));

							pmesh = find_mesh_by_texture(pmodel, "..\\white.bmp");
							ptriv2 = find_mesh_triangle_by_index(pmesh, pmesh->numtris);

							ptriv2[0] = ptriangleVert[0];
							ptriv2[1] = ptriangleVert[1];
							ptriv2[2] = ptriangleVert[2];
						}
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

	if (badNormalsCount)
		printf("%d triangles with misdirected normals\n", badNormalsCount);

	if (vmin[2] != 0.0)
	{
		printf("lowest vector at %f\n", vmin[2]);
	}
}

void grab_smd_skeleton(Node *pnodes, Bone *pbones)
{
	float posX, posY, posZ, rotX, rotY, rotZ;
	char cmd[1024];
	int node;

	while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != NULL)
	{
		g_smdlinecount++;
		if (sscanf(g_currentsmdline, "%d %f %f %f %f %f %f", &node, &posX, &posY, &posZ, &rotX, &rotY, &rotZ) == 7)
		{
			pbones[node].pos[0] = posX;
			pbones[node].pos[1] = posY;
			pbones[node].pos[2] = posZ;

			ScaleVertexByQcScale(pbones[node].pos);

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

int grab_smd_nodes(Node *pnodes)
{
	int index;
	char boneName[1024];
	int parent;
	int numBones = 0;

	while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != NULL)
	{
		g_smdlinecount++;
		if (sscanf(g_currentsmdline, "%d \"%[^\"]\" %d", &index, boneName, &parent) == 3)
		{

			strcpyn(pnodes[index].name, boneName);
			pnodes[index].parent = parent;
			numBones = index;
			// check for mirrored bones;
			for (int i = 0; i < g_mirroredcount; i++)
			{
				if (std::strcmp(boneName, g_mirrorboneCommand[i]) == 0)
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

void parse_smd(Model *pmodel)
{
	char cmd[1024];
	int option;

	g_smdpath = g_cdCommandAbsolute / (std::string(pmodel->name) + ".smd");
	if (!std::filesystem::exists(g_smdpath))
		error("%s doesn't exist", g_smdpath.c_str());

	printf("grabbing %s\n", g_smdpath.c_str());

	if ((g_smdfile = fopen(g_smdpath.string().c_str(), "r")) == 0)
	{
		fprintf(stderr, "reader: could not open file '%s'\n", g_smdpath.c_str());
	}
	g_smdlinecount = 0;

	while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != NULL)
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
			pmodel->numbones = grab_smd_nodes(pmodel->node);
		}
		else if (std::strcmp(cmd, "skeleton") == 0)
		{
			grab_smd_skeleton(pmodel->node, pmodel->skeleton);
		}
		else if (std::strcmp(cmd, "triangles") == 0)
		{
			grab_smd_triangles(pmodel);
		}
		else
		{
			printf("unknown studio command\n");
		}
	}
	fclose(g_smdfile);
}

void cmd_eyeposition(std::string &token)
{
	// rotate points into frame of reference so model points down the positive x
	// axis
	get_token(false, token);
	g_eyepositionCommand[1] = std::stof(token);

	get_token(false, token);
	g_eyepositionCommand[0] = -std::stof(token);

	get_token(false, token);
	g_eyepositionCommand[2] = std::stof(token);
}

void cmd_flags(std::string &token)
{
	get_token(false, token);
	g_flagsCommand = std::stoi(token);
}

void cmd_modelname(std::string &token)
{
	get_token(false, token);
	strcpyn(g_modelnameCommand, token.c_str());
}

void cmd_body_optionstudio(std::string &token)
{
	if (!get_token(false, token))
		return;

	g_submodel[g_submodelscount] = (Model *)std::calloc(1, sizeof(Model));
	g_bodypart[g_bodygroupcount].pmodel[g_bodypart[g_bodygroupcount].nummodels] = g_submodel[g_submodelscount];

	strcpyn(g_submodel[g_submodelscount]->name, token.c_str());

	g_flagfliptriangles = 1;

	g_scaleBodyAndSequenceOption = g_scaleCommand;

	while (token_available())
	{
		get_token(false, token);
		if (stricmp("reverse", token.c_str()) == 0)
		{
			g_flagfliptriangles = 0;
		}
		else if (stricmp("scale", token.c_str()) == 0)
		{
			get_token(false, token);
			g_scaleBodyAndSequenceOption = std::stof(token);
		}
	}

	parse_smd(g_submodel[g_submodelscount]);

	g_bodypart[g_bodygroupcount].nummodels++;
	g_submodelscount++;

	g_scaleBodyAndSequenceOption = g_scaleCommand;
}

int cmd_body_optionblank()
{
	g_submodel[g_submodelscount] = (Model *)(1, sizeof(Model));
	g_bodypart[g_bodygroupcount].pmodel[g_bodypart[g_bodygroupcount].nummodels] = g_submodel[g_submodelscount];

	strcpyn(g_submodel[g_submodelscount]->name, "blank");

	g_bodypart[g_bodygroupcount].nummodels++;
	g_submodelscount++;
	return 0;
}

void cmd_bodygroup(std::string &token)
{
	if (!get_token(false, token))
		return;

	if (g_bodygroupcount == 0)
	{
		g_bodypart[g_bodygroupcount].base = 1;
	}
	else
	{
		g_bodypart[g_bodygroupcount].base = g_bodypart[g_bodygroupcount - 1].base * g_bodypart[g_bodygroupcount - 1].nummodels;
	}
	strcpyn(g_bodypart[g_bodygroupcount].name, token.c_str());

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
			cmd_body_optionstudio(token);
		}
		else if (stricmp("blank", token.c_str()) == 0)
		{
			cmd_body_optionblank();
		}
	}

	g_bodygroupcount++;
	return;
}

void cmd_body(std::string &token)
{
	if (!get_token(false, token))
		return;

	if (g_bodygroupcount == 0)
	{
		g_bodypart[g_bodygroupcount].base = 1;
	}
	else
	{
		g_bodypart[g_bodygroupcount].base = g_bodypart[g_bodygroupcount - 1].base * g_bodypart[g_bodygroupcount - 1].nummodels;
	}
	strcpyn(g_bodypart[g_bodygroupcount].name, token.c_str());

	cmd_body_optionstudio(token);

	g_bodygroupcount++;
}

void grab_option_animation(Animation *panim)
{
	Vector3 pos;
	Vector3 rot;
	char cmd[1024];
	int index;
	int t = -99999999;
	float cz, sz;
	int start = 99999;
	int end = 0;

	for (index = 0; index < panim->numbones; index++)
	{
		panim->pos[index] = (Vector3 *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(Vector3));
		panim->rot[index] = (Vector3 *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(Vector3));
	}

	cz = cosf(g_rotateCommand);
	sz = sinf(g_rotateCommand);

	while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != NULL)
	{
		g_smdlinecount++;
		if (sscanf(g_currentsmdline, "%d %f %f %f %f %f %f", &index, &pos[0], &pos[1], &pos[2], &rot[0], &rot[1], &rot[2]) == 7)
		{
			if (t >= panim->startframe && t <= panim->endframe)
			{
				if (panim->node[index].parent == -1)
				{
					adjust_vertex_to_origin(pos);
					panim->pos[index][t][0] = cz * pos[0] - sz * pos[1];
					panim->pos[index][t][1] = sz * pos[0] + cz * pos[1];
					panim->pos[index][t][2] = pos[2];
					// rotate model
					rot[2] += g_rotateCommand;
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

				ScaleVertexByQcScale(panim->pos[index][t]);

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
	int size;

	size = (panim->endframe - panim->startframe + 1) * sizeof(Vector3);
	// shift
	for (int j = 0; j < panim->numbones; j++)
	{
		Vector3 *ppos;
		Vector3 *prot;

		ppos = (Vector3 *)std::calloc(1, size);
		prot = (Vector3 *)std::calloc(1, size);

		std::memcpy(ppos, &panim->pos[j][panim->startframe], size);
		std::memcpy(prot, &panim->rot[j][panim->startframe], size);

		free(panim->pos[j]);
		free(panim->rot[j]);

		panim->pos[j] = ppos;
		panim->rot[j] = prot;
	}
}

void cmd_sequence_option_animation(char *name, Animation *panim)
{
	char cmd[1024];
	int option;

	strcpyn(panim->name, name);

	g_smdpath = g_cdCommandAbsolute / (std::string(panim->name) + ".smd");
	if (!std::filesystem::exists(g_smdpath))
		error("%s doesn't exist", g_smdpath.c_str());

	printf("grabbing %s\n", g_smdpath.c_str());

	if ((g_smdfile = fopen(g_smdpath.string().c_str(), "r")) == 0)
	{
		fprintf(stderr, "reader: could not open file '%s'\n", g_smdpath.c_str());
		error(0);
	}
	g_smdlinecount = 0;

	while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != NULL)
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
			panim->numbones = grab_smd_nodes(panim->node);
		}
		else if (std::strcmp(cmd, "skeleton") == 0)
		{
			grab_option_animation(panim);
			shift_option_animation(panim);
		}
		else
		{
			printf("unknown studio command : %s\n", cmd);
			while (fgets(g_currentsmdline, sizeof(g_currentsmdline), g_smdfile) != NULL)
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
	int event;

	if (psequence->numevents + 1 >= MAXSTUDIOEVENTS)
	{
		printf("too many events\n");
		exit(0);
	}

	get_token(false, token);
	event = std::stoi(token);
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
		std::strcpy(psequence->event[psequence->numevents - 1].options, token.c_str());
	}

	return 0;
}

int cmd_sequence_option_fps(std::string &token, Sequence *psequence)
{
	get_token(false, token);

	psequence->fps = std::stof(token);

	return 0;
}

int cmd_sequence_option_addpivot(std::string &token, Sequence *psequence)
{
	if (psequence->numpivots + 1 >= MAXSTUDIOPIVOTS)
	{
		printf("too many pivot points\n");
		exit(0);
	}

	get_token(false, token);
	psequence->pivot[psequence->numpivots].index = std::stoi(token);

	get_token(false, token);
	psequence->pivot[psequence->numpivots].start = std::stoi(token);

	get_token(false, token);
	psequence->pivot[psequence->numpivots].end = std::stoi(token);

	psequence->numpivots++;

	return 0;
}

void cmd_origin(std::string &token)
{
	get_token(false, token);
	g_originCommand[0] = std::stof(token);

	get_token(false, token);
	g_originCommand[1] = std::stof(token);

	get_token(false, token);
	g_originCommand[2] = std::stof(token);

	if (token_available())
	{
		get_token(false, token);
		g_originCommandRotation = to_radians(std::stof(token) + ENGINE_ORIENTATION);
	}
}

void Cmd_Sequence_OptionOrigin(std::string &token)
{
	get_token(false, token);
	g_sequenceOrigin[0] = std::stof(token);

	get_token(false, token);
	g_sequenceOrigin[1] = std::stof(token);

	get_token(false, token);
	g_sequenceOrigin[2] = std::stof(token);
}

void cmd_sequence_option_rotate(std::string &token)
{
	get_token(false, token);
	g_rotateCommand = to_radians(std::stof(token) + ENGINE_ORIENTATION);
}

void cmd_scale(std::string &token)
{
	get_token(false, token);
	g_scaleCommand = g_scaleBodyAndSequenceOption = std::stof(token);
}

void cmd_rotate(std::string &token) // XDM
{
	if (!get_token(false, token))
		return;
	g_rotateCommand = to_radians(std::stof(token) + ENGINE_ORIENTATION);
}

void cmd_sequence_option_scale(std::string &token)
{
	get_token(false, token);
	g_scaleBodyAndSequenceOption = std::stof(token);
}

int cmd_sequencegroup(std::string &token)
{
	get_token(false, token);
	strcpyn(g_sequencegroupCommand[g_sequencegroupcount].label, token.c_str());
	g_sequencegroupcount++;

	return 0;
}

int Cmd_Sequence_OptionAction(std::string &szActivity)
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

int cmd_sequence(std::string &token)
{
	int depth = 0;
	char smdfilename[MAXSTUDIOBLENDS][1024];
	int i;
	int numblends = 0;
	int start = 0;
	int end = MAXSTUDIOANIMATIONS - 1;

	if (!get_token(false, token))
		return 0;

	strcpyn(g_sequenceCommand[g_sequencecount].name, token.c_str());

	g_sequenceOrigin = g_originCommand;
	g_scaleBodyAndSequenceOption = g_scaleCommand;

	g_rotateCommand = g_originCommandRotation;
	g_sequenceCommand[g_sequencecount].fps = 30.0;
	g_sequenceCommand[g_sequencecount].seqgroup = g_sequencegroupcount - 1;
	g_sequenceCommand[g_sequencecount].blendstart[0] = 0.0;
	g_sequenceCommand[g_sequencecount].blendend[0] = 1.0;

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
			depth -= cmd_sequence_option_event(token, &g_sequenceCommand[g_sequencecount]);
		}
		else if (token == "pivot")
		{
			cmd_sequence_option_addpivot(token, &g_sequenceCommand[g_sequencecount]);
		}
		else if (token == "fps")
		{
			cmd_sequence_option_fps(token, &g_sequenceCommand[g_sequencecount]);
		}
		else if (token == "origin")
		{
			Cmd_Sequence_OptionOrigin(token);
		}
		else if (token == "rotate")
		{
			cmd_sequence_option_rotate(token);
		}
		else if (token == "scale")
		{
			cmd_sequence_option_scale(token);
		}
		else if (token == "loop")
		{
			g_sequenceCommand[g_sequencecount].flags |= STUDIO_LOOPING;
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
			g_sequenceCommand[g_sequencecount].blendtype[0] = static_cast<float>(lookup_control(token.c_str()));
			get_token(false, token);
			g_sequenceCommand[g_sequencecount].blendstart[0] = std::stof(token);
			get_token(false, token);
			g_sequenceCommand[g_sequencecount].blendend[0] = std::stof(token);
		}
		else if (token == "node")
		{
			get_token(false, token);
			g_sequenceCommand[g_sequencecount].entrynode = g_sequenceCommand[g_sequencecount].exitnode = std::stoi(token);
		}
		else if (token == "transition")
		{
			get_token(false, token);
			g_sequenceCommand[g_sequencecount].entrynode = std::stoi(token);
			get_token(false, token);
			g_sequenceCommand[g_sequencecount].exitnode = std::stoi(token);
		}
		else if (token == "rtransition")
		{
			get_token(false, token);
			g_sequenceCommand[g_sequencecount].entrynode = std::stoi(token);
			get_token(false, token);
			g_sequenceCommand[g_sequencecount].exitnode = std::stoi(token);
			g_sequenceCommand[g_sequencecount].nodeflags |= 1;
		}
		else if (lookup_control(token.c_str()) != -1) // motion flags [motion extraction]
		{
			g_sequenceCommand[g_sequencecount].motiontype |= lookup_control(token.c_str());
		}
		else if (token == "animation")
		{
			get_token(false, token);
			strcpyn(smdfilename[numblends++], token.c_str());
		}
		else if ((i = Cmd_Sequence_OptionAction(token)) != 0)
		{
			g_sequenceCommand[g_sequencecount].activity = i;
			get_token(false, token);
			g_sequenceCommand[g_sequencecount].actweight = std::stoi(token);
		}
		else
		{
			strcpyn(smdfilename[numblends++], token.c_str());
		}

		if (depth < 0)
		{
			printf("missing {\n");
			exit(1);
		}
	};

	if (numblends == 0)
	{
		printf("no animations found\n");
		exit(1);
	}
	for (i = 0; i < numblends; i++)
	{
		g_animationSequenceOption[g_animationcount] = (Animation *)malloc(sizeof(Animation));
		g_sequenceCommand[g_sequencecount].panim[i] = g_animationSequenceOption[g_animationcount];
		g_sequenceCommand[g_sequencecount].panim[i]->startframe = start;
		g_sequenceCommand[g_sequencecount].panim[i]->endframe = end;
		g_sequenceCommand[g_sequencecount].panim[i]->flags = 0;
		cmd_sequence_option_animation(smdfilename[i], g_animationSequenceOption[g_animationcount]);
		g_animationcount++;
	}
	g_sequenceCommand[g_sequencecount].numblends = numblends;

	g_sequencecount++;

	return 0;
}

int cmd_controller(std::string &token)
{
	if (get_token(false, token))
	{
		if (token == "mouth")
		{
			g_bonecontrollerCommand[g_bonecontrollerscount].index = 4;
		}
		else
		{
			g_bonecontrollerCommand[g_bonecontrollerscount].index = std::stoi(token);
		}
		if (get_token(false, token))
		{
			strcpyn(g_bonecontrollerCommand[g_bonecontrollerscount].name, token.c_str());
			get_token(false, token);
			if ((g_bonecontrollerCommand[g_bonecontrollerscount].type = lookup_control(token.c_str())) == -1)
			{
				printf("unknown bonecontroller type '%s'\n", token);
				return 0;
			}
			get_token(false, token);
			g_bonecontrollerCommand[g_bonecontrollerscount].start = std::stof(token);
			get_token(false, token);
			g_bonecontrollerCommand[g_bonecontrollerscount].end = std::stof(token);

			if (g_bonecontrollerCommand[g_bonecontrollerscount].type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
			{
				if ((static_cast<int>(g_bonecontrollerCommand[g_bonecontrollerscount].start + 360) % 360) == (static_cast<int>(g_bonecontrollerCommand[g_bonecontrollerscount].end + 360) % 360))
				{
					g_bonecontrollerCommand[g_bonecontrollerscount].type |= STUDIO_RLOOP;
				}
			}
			g_bonecontrollerscount++;
		}
	}
	return 1;
}

void cmd_bbox(std::string &token)
{
	get_token(false, token);
	g_bboxCommand[0][0] = std::stof(token);

	get_token(false, token);
	g_bboxCommand[0][1] = std::stof(token);

	get_token(false, token);
	g_bboxCommand[0][2] = std::stof(token);

	get_token(false, token);
	g_bboxCommand[1][0] = std::stof(token);

	get_token(false, token);
	g_bboxCommand[1][1] = std::stof(token);

	get_token(false, token);
	g_bboxCommand[1][2] = std::stof(token);
}

void cmd_cbox(std::string &token)
{
	get_token(false, token);
	g_cboxCommand[0][0] = std::stof(token);

	get_token(false, token);
	g_cboxCommand[0][1] = std::stof(token);

	get_token(false, token);
	g_cboxCommand[0][2] = std::stof(token);

	get_token(false, token);
	g_cboxCommand[1][0] = std::stof(token);

	get_token(false, token);
	g_cboxCommand[1][1] = std::stof(token);

	get_token(false, token);
	g_cboxCommand[1][2] = std::stof(token);
}

void cmd_mirror(std::string &token)
{
	get_token(false, token);
	strcpyn(g_mirrorboneCommand[g_mirroredcount++], token.c_str());
}

void cmd_gamma(std::string &token)
{
	get_token(false, token);
	g_gammaCommand = std::stof(token);
}

int cmd_texturegroup(std::string &token)
{
	int i;
	int depth = 0;
	int index = 0;
	int group = 0;

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
			group++;
			index = 0;
		}
		else if (depth == 2)
		{
			i = find_texture_index(token);
			g_texturegroupCommand[g_texturegroupCount][group][index] = i;
			if (group != 0)
				g_texture[i].parent = g_texturegroupCommand[g_texturegroupCount][0][index];
			index++;
			g_texturegroupreps[g_texturegroupCount] = index;
			g_texturegrouplayers[g_texturegroupCount] = group + 1;
		}
	}

	g_texturegroupCount++;

	return 0;
}

int cmd_hitgroup(std::string &token)
{
	get_token(false, token);
	g_hitgroupCommand[g_hitgroupscount].group = std::stoi(token);
	get_token(false, token);
	strcpyn(g_hitgroupCommand[g_hitgroupscount].name, token.c_str());
	g_hitgroupscount++;

	return 0;
}

int cmd_hitbox(std::string &token)
{
	get_token(false, token);
	g_hitboxCommand[g_hitboxescount].group = std::stoi(token);
	get_token(false, token);
	strcpyn(g_hitboxCommand[g_hitboxescount].name, token.c_str());
	get_token(false, token);
	g_hitboxCommand[g_hitboxescount].bmin[0] = std::stof(token);
	get_token(false, token);
	g_hitboxCommand[g_hitboxescount].bmin[1] = std::stof(token);
	get_token(false, token);
	g_hitboxCommand[g_hitboxescount].bmin[2] = std::stof(token);
	get_token(false, token);
	g_hitboxCommand[g_hitboxescount].bmax[0] = std::stof(token);
	get_token(false, token);
	g_hitboxCommand[g_hitboxescount].bmax[1] = std::stof(token);
	get_token(false, token);
	g_hitboxCommand[g_hitboxescount].bmax[2] = std::stof(token);

	g_hitboxescount++;

	return 0;
}

int cmd_attachment(std::string &token)
{
	// index
	get_token(false, token);
	g_attachmentCommand[g_attachmentscount].index = std::stoi(token);

	// bone name
	get_token(false, token);
	strcpyn(g_attachmentCommand[g_attachmentscount].bonename, token.c_str());

	// position
	get_token(false, token);
	g_attachmentCommand[g_attachmentscount].org[0] = std::stof(token);
	get_token(false, token);
	g_attachmentCommand[g_attachmentscount].org[1] = std::stof(token);
	get_token(false, token);
	g_attachmentCommand[g_attachmentscount].org[2] = std::stof(token);

	if (token_available())
		get_token(false, token);

	if (token_available())
		get_token(false, token);

	g_attachmentscount++;
	return 0;
}

void cmd_renamebone(std::string &token)
{
	// from
	get_token(false, token);
	std::strcpy(g_renameboneCommand[g_renamebonecount].from, token.c_str());

	// to
	get_token(false, token);
	std::strcpy(g_renameboneCommand[g_renamebonecount].to, token.c_str());

	g_renamebonecount++;
}

void cmd_texrendermode(std::string &token)
{
	std::string tex_name;
	get_token(false, token);
	tex_name = token;

	get_token(false, token);
	if (token == "additive")
	{
		g_texture[find_texture_index(tex_name)].flags |= STUDIO_NF_ADDITIVE;
	}
	else if (token == "masked")
	{
		g_texture[find_texture_index(tex_name)].flags |= STUDIO_NF_MASKED;
	}
	else if (token == "fullbright")
	{
		g_texture[find_texture_index(tex_name)].flags |= STUDIO_NF_FULLBRIGHT;
	}
	else if (token == "flatshade")
	{
		g_texture[find_texture_index(tex_name)].flags |= STUDIO_NF_FLATSHADE;
	}
	else
		printf("Texture '%s' has unknown render mode '%s'!\n", tex_name, token);
}

void parse_qc_file()
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
			cmd_modelname(token);
		}
		else if (token == "$cd")
		{
			if (iscdalreadyset)
				error("Two $cd in one model");
			iscdalreadyset = true;
			get_token(false, token);

			g_cdCommand = token;
			g_cdCommandAbsolute = std::filesystem::absolute(g_cdCommand);
		}
		else if (token == "$cdtexture")
		{
			if (iscdtexturealreadyset)
				error("Two $cdtexture in one model");
			iscdtexturealreadyset = true;
			get_token(false, token);
			g_cdtextureCommand = token;
		}
		else if (token == "$scale")
		{
			cmd_scale(token);
		}
		else if (token == "$rotate") // XDM
		{
			cmd_rotate(token);
		}
		else if (token == "$controller")
		{
			cmd_controller(token);
		}
		else if (token == "$body")
		{
			cmd_body(token);
		}
		else if (token == "$bodygroup")
		{
			cmd_bodygroup(token);
		}
		else if (token == "$sequence")
		{
			cmd_sequence(token);
		}
		else if (token == "$sequencegroup")
		{
			cmd_sequencegroup(token);
		}
		else if (token == "$eyeposition")
		{
			cmd_eyeposition(token);
		}
		else if (token == "$origin")
		{
			cmd_origin(token);
		}
		else if (token == "$bbox")
		{
			cmd_bbox(token);
		}
		else if (token == "$cbox")
		{
			cmd_cbox(token);
		}
		else if (token == "$mirrorbone")
		{
			cmd_mirror(token);
		}
		else if (token == "$gamma")
		{
			cmd_gamma(token);
		}
		else if (token == "$flags")
		{
			cmd_flags(token);
		}
		else if (token == "$texturegroup")
		{
			cmd_texturegroup(token);
		}
		else if (token == "$hgroup")
		{
			cmd_hitgroup(token);
		}
		else if (token == "$hbox")
		{
			cmd_hitbox(token);
		}
		else if (token == "$attachment")
		{
			cmd_attachment(token);
		}
		else if (token == "$renamebone")
		{
			cmd_renamebone(token);
		}
		else if (token == "$texrendermode")
		{
			cmd_texrendermode(token);
		}
		else
		{
			error("Incorrect/Unsupported command %s\n", token);
		}
	}
}

int main(int argc, char **argv)
{
	int i;
	char path[1024];

	g_scaleCommand = 1.0f;
	g_originCommandRotation = to_radians(ENGINE_ORIENTATION);

	g_numtextureteplacements = 0;
	g_flagreversedtriangles = 0;
	g_flagbadnormals = 0;
	g_flagfliptriangles = 1;
	g_flagkeepallbones = false;

	g_flagnormalblendangle = cosf(to_radians(2.0));

	g_gammaCommand = 1.8f;

	if (argc == 1)
		error("usage: studiomdl <flags>\n [-t texture]\n -r(tag reversed)\n -n(tag bad normals)\n -f(flip all triangles)\n [-a normal_blend_angle]\n -h(dump hboxes)\n -i(ignore warnings) \n b(keep all unused bones)\n file.qc");

	for (i = 1; i < argc - 1; i++)
	{
		if (argv[i][0] == '-')
		{
			switch (argv[i][1])
			{
			case 't':
				i++;
				std::strcpy(g_defaulttextures[g_numtextureteplacements], argv[i]);
				if (i < argc - 2 && argv[i + 1][0] != '-')
				{
					i++;
					std::strcpy(g_sourcetexture[g_numtextureteplacements], argv[i]);
					printf("Replaceing %s with %s\n", g_sourcetexture[g_numtextureteplacements], g_defaulttextures[g_numtextureteplacements]);
				}
				printf("Using default texture: %s\n", g_defaulttextures);
				g_numtextureteplacements++;
				break;
			case 'r':
				g_flagreversedtriangles = 1;
				break;
			case 'n':
				g_flagbadnormals = 1;
				break;
			case 'f':
				g_flagfliptriangles = 0;
				break;
			case 'a':
				i++;
				g_flagnormalblendangle = cosf(to_radians(std::stof(argv[i])));
				break;
			case 'h':
				g_flagdumphitboxes = 1;
				break;
			case 'i':
				g_flagignorewarnings = 1;
				break;
			case 'b':
				g_flagkeepallbones = true;
				break;
			}
		}
	}

	std::strcpy(g_sequencegroupCommand[g_sequencegroupcount].label, "default");
	g_sequencegroupcount = 1;
	// load the script
	std::strcpy(path, argv[i]);
	load_qc_file(path);
	// parse it
	parse_qc_file();
	set_skin_values();
	simplify_model();
	write_file();

	return 0;
}
