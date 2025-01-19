// studiomdl.c: generates a studio .mdl file from a .qc script
// models/<scriptname>.mdl.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>

#include "cmdlib.h"
#include "scriplib.h"
#include "mathlib.h"
#define extern // ??
#include "studio.h"
#include "studiomdl.h"
#include "monsters/activity.h"
#include "monsters/activitymap.h"
#include "write.h"

#define strnicmp strncasecmp
#define stricmp strcasecmp
#define strcpyn(a, b) std::strncpy(a, b, sizeof(a))

char g_inputfilename[1024];
FILE *g_inputFile;
char g_currentline[1024];
int g_linecount;
char g_defaulttextures[16][256];
char g_sourcetexture[16][256];
int g_numtextureteplacements;
s_bonefixup_t g_bonefixup[MAXSTUDIOSRCBONES];

// studiomdl.exe args -----------
int g_flagreversedtriangles;
int g_flagbadnormals;
int g_flagfliptriangles;
float g_flagnormalblendangle;
int g_flagdumphitboxes;
int g_flagignorewarnings;
bool g_flagkeepallbones;

// QC Command variables -----------------
char g_cdPartialPath[256];							   // $cd
char g_cdCommand[256];								   // $cd
int g_cdtexturepathcount;							   // $cdtexture <paths> (max paths = 18, idk why)
char g_cdtextureCommand[16][256];					   // $cdtexture
float g_scaleCommand;								   // $scale
float g_scaleBodyAndSequenceOption;					   // $body studio <value> // also for $sequence
vec3_t g_originCommand;								   // $origin
float g_originCommandRotation;						   // $origin <X> <Y> <Z> <rotation>
float g_rotateCommand;								   // $rotate and $sequence <sequence name> <SMD path> {[rotate <zrotation>]} only z axis
vec3_t g_sequenceOrigin;							   // $sequence <sequence name> <SMD path>  {[origin <X> <Y> <Z>]}
float g_gammaCommand;								   // $$gamma
s_renamebone_t g_renameboneCommand[MAXSTUDIOSRCBONES]; // $renamebone
int g_renamebonecount;
s_hitgroup_t g_hitgroupCommand[MAXSTUDIOSRCBONES]; // $hgroup
int g_hitgroupscount;
char g_mirrorboneCommand[MAXSTUDIOSRCBONES][64]; // $mirrorbone
int g_mirroredcount;
s_animation_t *g_animationSequenceOption[MAXSTUDIOSEQUENCES * MAXSTUDIOBLENDS]; // $sequence, each sequence can have 16 blends
int g_animationcount;
int g_texturegroupCommand[32][32][32]; // $texturegroup
int g_texturegroupCount;			   // unnecessary? since engine doesn't support multiple texturegroups
int g_texturegrouplayers[32];
int g_texturegroupreps[32];
// ---------------------------------------

void clip_rotations(vec3_t rot)
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

void ExtractMotion()
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
			vec3_t *ptrPos;
			vec3_t motion = {0, 0, 0};
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
				vec3_t adjustedPosition;
				for (k = 0; k < g_sequenceCommand[i].panim[0]->numbones; k++)
				{
					if (g_sequenceCommand[i].panim[0]->node[k].parent == -1)
					{
						ptrPos = g_sequenceCommand[i].panim[0]->pos[k];

						adjustedPosition = motion * (j * 1.0 / (g_sequenceCommand[i].numframes - 1));
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
		vec3_t *ptrAutoPos;
		vec3_t *ptrAutoRot;
		vec3_t motion = {0, 0, 0};
		vec3_t angles = {0, 0, 0};

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

void OptimizeAnimations()
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
					vec3_t *ppos = g_sequenceCommand[i].panim[blends]->pos[j];
					vec3_t *prot = g_sequenceCommand[i].panim[blends]->rot[j];

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

int findNode(char *name)
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

void MakeTransitions()
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

void SimplifyModel()
{
	int i, j, k;
	int n, m, q;
	vec3_t *defaultpos[MAXSTUDIOSRCBONES] = {0};
	vec3_t *defaultrot[MAXSTUDIOSRCBONES] = {0};
	int iError = 0;

	OptimizeAnimations();
	ExtractMotion();
	MakeTransitions();

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
				k = findNode(g_submodel[i]->node[j].name);
				if (k == -1)
				{
					// create new bone
					k = g_bonescount;
					strcpyn(g_bonetable[k].name, g_submodel[i]->node[j].name);
					if ((n = g_submodel[i]->node[j].parent) != -1)
						g_bonetable[k].parent = findNode(g_submodel[i]->node[n].name);
					else
						g_bonetable[k].parent = -1;
					g_bonetable[k].bonecontroller = 0;
					g_bonetable[k].flags = 0;
					// set defaults
					defaultpos[k] = (vec3_t *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(vec3_t));
					defaultrot[k] = (vec3_t *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(vec3_t));
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
						n = findNode(g_submodel[i]->node[n].name);
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
		Error("Too many bones used in model, used %d, max %d\n", g_bonescount, MAXSTUDIOBONES);
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
			k = findNode(g_sequenceCommand[i].panim[0]->node[j].name);

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
			Error("unknown bonecontroller link '%s'\n", g_bonecontrollerCommand[i].name);
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
			Error("unknown attachment link '%s'\n", g_attachmentCommand[i].bonename);
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
			Error("cannot find bone %s for hitgroup %d\n", g_hitgroupCommand[j].name, g_hitgroupCommand[j].group);
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
			vec3_t p;
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
				Error("cannot find bone %s for bbox\n", g_hitboxCommand[j].name);
		}
	}

	// relink animations
	for (i = 0; i < g_sequencecount; i++)
	{
		vec3_t *origpos[MAXSTUDIOSRCBONES] = {0};
		vec3_t *origrot[MAXSTUDIOSRCBONES] = {0};

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
					scale = minv / -32768.0;
				}
				else
				{
					scale = maxv / 32767;
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
		vec3_t bmin, bmax;

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
				vec3_t pos;

				for (j = 0; j < g_bonescount; j++)
				{
					// convert to degrees
					vec3_t angles = g_sequenceCommand[i].panim[q]->rot[j][n] * (180.0 / Q_PI);

					AngleMatrix(angles, bonematrix);

					bonematrix[0][3] = g_sequenceCommand[i].panim[q]->pos[j][n][0];
					bonematrix[1][3] = g_sequenceCommand[i].panim[q]->pos[j][n][1];
					bonematrix[2][3] = g_sequenceCommand[i].panim[q]->pos[j][n][2];

					if (g_bonetable[j].parent == -1)
					{
						MatrixCopy(bonematrix, bonetransform[j]);
					}
					else
					{
						R_ConcatTransforms(bonetransform[g_bonetable[j].parent], bonematrix, bonetransform[j]);
					}
				}

				for (k = 0; k < g_submodelscount; k++)
				{
					for (j = 0; j < g_submodel[k]->numverts; j++)
					{
						VectorTransform(g_submodel[k]->vert[j].org, bonetransform[g_submodel[k]->vert[j].bone], pos);

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
						mstudioanimvalue_t *pcount, *pvalue;
						float v;
						short value[MAXSTUDIOANIMATIONS];
						mstudioanimvalue_t data[MAXSTUDIOANIMATIONS];

						for (n = 0; n < g_sequenceCommand[i].numframes; n++)
						{
							switch (k)
							{
							case 0:
							case 1:
							case 2:
								value[n] = (g_sequenceCommand[i].panim[q]->pos[j][n][k] - g_bonetable[j].pos[k]) / g_bonetable[j].posscale[k];
								break;
							case 3:
							case 4:
							case 5:
								v = (g_sequenceCommand[i].panim[q]->rot[j][n][k - 3] - g_bonetable[j].rot[k - 3]);
								if (v >= Q_PI)
									v -= Q_PI * 2;
								if (v < -Q_PI)
									v += Q_PI * 2;

								value[n] = v / g_bonetable[j].rotscale[k - 3];
								break;
							}
						}
						if (n == 0)
							Error("no animation frames: \"%s\"\n", g_sequenceCommand[i].name);

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

						g_sequenceCommand[i].panim[q]->numanim[j][k] = pvalue - data;
						if (g_sequenceCommand[i].panim[q]->numanim[j][k] == 2 && value[0] == 0)
						{
							g_sequenceCommand[i].panim[q]->numanim[j][k] = 0;
						}
						else
						{
							g_sequenceCommand[i].panim[q]->anim[j][k] = (mstudioanimvalue_t *)std::calloc(pvalue - data, sizeof(mstudioanimvalue_t));
							std::memcpy(g_sequenceCommand[i].panim[q]->anim[j][k], data, (pvalue - data) * sizeof(mstudioanimvalue_t));
						}
					}
				}
			}
		}
	}
}

int lookupControl(char *string)
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

int FindTextureIndex(char *texturename)
{
	int i;
	for (i = 0; i < g_texturescount; i++)
	{
		if (stricmp(g_texture[i].name, texturename) == 0)
		{
			return i;
		}
	}

	strcpyn(g_texture[i].name, texturename);

	// XDM: allow such names as "tex_chrome_bright" - chrome and full brightness effects
	if (stristr(texturename, "chrome") != NULL)
		g_texture[i].flags = STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME;
	else if (stristr(texturename, "bright") != NULL)
		g_texture[i].flags = STUDIO_NF_FLATSHADE | STUDIO_NF_FULLBRIGHT;
	else
		g_texture[i].flags = 0;

	g_texturescount++;
	return i;
}

s_mesh_t *FindMeshByTexture(s_model_t *pmodel, char *texturename)
{
	int i, j;

	j = FindTextureIndex(texturename);

	for (i = 0; i < pmodel->nummesh; i++)
	{
		if (pmodel->pmesh[i]->skinref == j)
		{
			return pmodel->pmesh[i];
		}
	}

	if (i >= MAXSTUDIOMESHES)
	{
		Error("too many textures in model: \"%s\"\n", pmodel->name);
	}

	pmodel->nummesh = i + 1;
	pmodel->pmesh[i] = (s_mesh_t *)std::calloc(1, sizeof(s_mesh_t));
	pmodel->pmesh[i]->skinref = j;

	return pmodel->pmesh[i];
}

s_trianglevert_t *FindMeshTriangleByIndex(s_mesh_t *pmesh, int index)
{
	if (index >= pmesh->alloctris)
	{
		int start = pmesh->alloctris;
		pmesh->alloctris = index + 256;
		if (pmesh->triangle)
		{
			pmesh->triangle = static_cast<s_trianglevert_t(*)[3]>(realloc(pmesh->triangle, pmesh->alloctris * sizeof(*pmesh->triangle)));
			std::memset(&pmesh->triangle[start], 0, (pmesh->alloctris - start) * sizeof(*pmesh->triangle));
		}
		else
		{
			pmesh->triangle = static_cast<s_trianglevert_t(*)[3]>(std::calloc(pmesh->alloctris, sizeof(*pmesh->triangle)));
		}
	}

	return pmesh->triangle[index];
}

int FindVertexNormalIndex(s_model_t *pmodel, s_normal_t *pnormal)
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
		Error("too many normals in model: \"%s\"\n", pmodel->name);
	}
	pmodel->normal[i].org = pnormal->org;
	pmodel->normal[i].bone = pnormal->bone;
	pmodel->normal[i].skinref = pnormal->skinref;
	pmodel->numnorms = i + 1;
	return i;
}

int FindVertexIndex(s_model_t *pmodel, s_vertex_t *pv)
{
	int i;

	// assume 2 digits of accuracy
	pv->org[0] = static_cast<int>(pv->org[0] * 100) / 100.0;
	pv->org[1] = static_cast<int>(pv->org[1] * 100) / 100.0;
	pv->org[2] = static_cast<int>(pv->org[2] * 100) / 100.0;

	for (i = 0; i < pmodel->numverts; i++)
	{
		if ((pmodel->vert[i].org == pv->org) && pmodel->vert[i].bone == pv->bone)
		{
			return i;
		}
	}
	if (i >= MAXSTUDIOVERTS)
	{
		Error("too many vertices in model: \"%s\"\n", pmodel->name);
	}
	pmodel->vert[i].org = pv->org;
	pmodel->vert[i].bone = pv->bone;
	pmodel->numverts = i + 1;
	return i;
}

void AdjustVertexToQcOrigin(vec3_t org)
{
	org[0] = (org[0] - g_sequenceOrigin[0]);
	org[1] = (org[1] - g_sequenceOrigin[1]);
	org[2] = (org[2] - g_sequenceOrigin[2]);
}

void ScaleVertexByQcScale(vec3_t org)
{
	org[0] = org[0] * g_scaleBodyAndSequenceOption;
	org[1] = org[1] * g_scaleBodyAndSequenceOption;
	org[2] = org[2] * g_scaleBodyAndSequenceOption;
}

// Called for the base frame
void TextureCoordRanges(s_mesh_t *pmesh, s_texture_t *ptexture)
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
			pmesh->triangle[i][j].s = static_cast<int>(std::round(pmesh->triangle[i][j].u * (ptexture->srcwidth)));
			pmesh->triangle[i][j].t = static_cast<int>(std::round(pmesh->triangle[i][j].v * (ptexture->srcheight)));
		}
	}

	// find the range
	ptexture->max_s = ptexture->srcwidth;
	ptexture->min_s = 0;
	ptexture->max_t = ptexture->srcheight;
	ptexture->min_t = 0;
}

void ResetTextureCoordRanges(s_mesh_t *pmesh, s_texture_t *ptexture)
{
	// adjust top, left edge
	for (int i = 0; i < pmesh->numtris; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			pmesh->triangle[i][j].t = -pmesh->triangle[i][j].t + ptexture->srcheight - ptexture->min_t;
		}
	}
}

void Grab_BMP(char *filename, s_texture_t *ptexture)
{
	int result;

	if (result = LoadBMP(filename, &ptexture->ppicture, (std::uint8_t **)&ptexture->ppal, &ptexture->srcwidth, &ptexture->srcheight))
	{
		Error("error %d reading BMP image \"%s\"\n", result, filename);
	}
}

void ResizeTexture(s_texture_t *ptexture)
{
	int i, t;
	std::uint8_t *pdest;
	int srcadjustedwidth;

	// Keep the original texture without resizing to avoid uv shift
	ptexture->skintop = ptexture->min_t;
	ptexture->skinleft = ptexture->min_s;
	ptexture->skinwidth = ptexture->srcwidth;
	ptexture->skinheight = ptexture->srcheight;

	ptexture->size = ptexture->skinwidth * ptexture->skinheight + 256 * 3;

	printf("BMP %s [%d %d] (%.0f%%)  %6d bytes\n", ptexture->name, ptexture->skinwidth, ptexture->skinheight,
		   ((ptexture->skinwidth * ptexture->skinheight) / static_cast<float>(ptexture->srcwidth * ptexture->srcheight)) * 100.0,
		   ptexture->size);

	if (ptexture->size > 1024 * 1024)
	{
		printf("%.0f %.0f %.0f %.0f\n", ptexture->min_s, ptexture->max_s, ptexture->min_t, ptexture->max_t);
		Error("texture too large\n");
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
	if (g_gammaCommand != 1.8)
	// gamma correct the monster textures to a gamma of 1.8
	{
		float g;
		std::uint8_t *psrc = (std::uint8_t *)ptexture->ppal;
		g = g_gammaCommand / 1.8;
		for (i = 0; i < 768; i++)
		{
			pdest[i] = pow(psrc[i] / 255.0, g) * 255;
		}
	}
	else
	{
		memcpy(pdest, ptexture->ppal, 256 * sizeof(rgb_t));
	}

	free(ptexture->ppicture);
	free(ptexture->ppal);
}

void Grab_Skin(s_texture_t *ptexture)
{
	char textureFilePath[1024];

	sprintf(textureFilePath, "%s/%s", g_cdPartialPath, ptexture->name);
	ExpandPathAndArchive(textureFilePath);

	if (g_cdtexturepathcount)
	{
		int time1 = -1;
		for (int i = 0; i < g_cdtexturepathcount; i++)
		{
			sprintf(textureFilePath, "%s/%s", g_cdtextureCommand[i], ptexture->name);
			time1 = FileTime(textureFilePath);
			if (time1 != -1)
			{
				break;
			}
		}
		if (time1 == -1)
			Error("%s not found", textureFilePath);
	}
	else
	{
		sprintf(textureFilePath, "%s/%s", g_cdCommand, ptexture->name);
	}

	if (stricmp(".bmp", &textureFilePath[strlen(textureFilePath) - 4]) == 0)
	{
		Grab_BMP(textureFilePath, ptexture);
	}
	else
	{
		Error("unknown graphics type: \"%s\"\n", textureFilePath);
	}
}

void SetSkinValues()
{
	int i, j;

	for (i = 0; i < g_texturescount; i++)
	{
		Grab_Skin(&g_texture[i]);

		g_texture[i].max_s = -9999999;
		g_texture[i].min_s = 9999999;
		g_texture[i].max_t = -9999999;
		g_texture[i].min_t = 9999999;
	}

	for (i = 0; i < g_submodelscount; i++)
	{
		for (j = 0; j < g_submodel[i]->nummesh; j++)
		{
			TextureCoordRanges(g_submodel[i]->pmesh[j], &g_texture[g_submodel[i]->pmesh[j]->skinref]);
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

		ResizeTexture(&g_texture[i]);
	}

	for (i = 0; i < g_submodelscount; i++)
	{
		for (j = 0; j < g_submodel[i]->nummesh; j++)
		{
			ResetTextureCoordRanges(g_submodel[i]->pmesh[j], &g_texture[g_submodel[i]->pmesh[j]->skinref]);
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

void Build_Reference(s_model_t *pmodel)
{
	vec3_t boneAngle{};

	for (int i = 0; i < pmodel->numbones; i++)
	{
		int parent;

		// convert to degrees
		boneAngle[0] = pmodel->skeleton[i].rot[0] * (180.0 / Q_PI);
		boneAngle[1] = pmodel->skeleton[i].rot[1] * (180.0 / Q_PI);
		boneAngle[2] = pmodel->skeleton[i].rot[2] * (180.0 / Q_PI);

		parent = pmodel->node[i].parent;
		if (parent == -1)
		{
			// scale the done pos.
			// calc rotational matrices
			AngleMatrix(boneAngle, g_bonefixup[i].m);
			AngleIMatrix(boneAngle, g_bonefixup[i].im);
			g_bonefixup[i].worldorg = pmodel->skeleton[i].pos;
		}
		else
		{
			vec3_t truePos;
			float rotationMatrix[3][4];
			// calc compound rotational matrices
			// FIXME : Hey, it's orthogical so inv(A) == transpose(A)
			AngleMatrix(boneAngle, rotationMatrix);
			R_ConcatTransforms(g_bonefixup[parent].m, rotationMatrix, g_bonefixup[i].m);
			AngleIMatrix(boneAngle, rotationMatrix);
			R_ConcatTransforms(rotationMatrix, g_bonefixup[parent].im, g_bonefixup[i].im);

			// calc true world coord.
			VectorTransform(pmodel->skeleton[i].pos, g_bonefixup[parent].m, truePos);
			g_bonefixup[i].worldorg = truePos + g_bonefixup[parent].worldorg;
		}
	}
}

void Grab_SMDTriangles(s_model_t *pmodel)
{
	int i;
	int trianglesCount = 0;
	int badNormalsCount = 0;
	vec3_t vmin, vmax;

	vmin[0] = vmin[1] = vmin[2] = 99999;
	vmax[0] = vmax[1] = vmax[2] = -99999;

	Build_Reference(pmodel);

	// load the base triangles
	while (true)
	{
		if (fgets(g_currentline, sizeof(g_currentline), g_inputFile) != NULL)
		{
			s_mesh_t *pmesh;
			char triangleMaterial[64];
			s_trianglevert_t *ptriangleVert;
			int parentBone;

			vec3_t triangleVertices[3];
			vec3_t triangleNormals[3];

			g_linecount++;

			// check for end
			if (std::strcmp("end\n", g_currentline) == 0)
				return;

			// strip off trailing smag
			std::strcpy(triangleMaterial, g_currentline);
			for (i = strlen(triangleMaterial) - 1; i >= 0 && !isgraph(triangleMaterial[i]); i--)
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
				fgets(g_currentline, sizeof(g_currentline), g_inputFile);
				fgets(g_currentline, sizeof(g_currentline), g_inputFile);
				fgets(g_currentline, sizeof(g_currentline), g_inputFile);
				g_linecount += 3;
				continue;
			}

			pmesh = FindMeshByTexture(pmodel, triangleMaterial);

			for (int j = 0; j < 3; j++)
			{
				if (g_flagfliptriangles)
					// quake wants them in the reverse order
					ptriangleVert = FindMeshTriangleByIndex(pmesh, pmesh->numtris) + 2 - j;
				else
					ptriangleVert = FindMeshTriangleByIndex(pmesh, pmesh->numtris) + j;

				if (fgets(g_currentline, sizeof(g_currentline), g_inputFile) != NULL)
				{
					s_vertex_t triangleVertex;
					s_normal_t triangleNormal;

					g_linecount++;
					if (sscanf(g_currentline, "%d %f %f %f %f %f %f %f %f",
							   &parentBone,
							   &triangleVertex.org[0], &triangleVertex.org[1], &triangleVertex.org[2],
							   &triangleNormal.org[0], &triangleNormal.org[1], &triangleNormal.org[2],
							   &ptriangleVert->u, &ptriangleVert->v) == 9)
					{
						if (parentBone < 0 || parentBone >= pmodel->numbones)
						{
							fprintf(stderr, "bogus bone index\n");
							fprintf(stderr, "%d %s :\n%s", g_linecount, g_inputfilename, g_currentline);
							exit(1);
						}

						triangleVertices[j] = triangleVertex.org;
						triangleNormals[j] = triangleNormal.org;

						triangleVertex.bone = parentBone;
						triangleNormal.bone = parentBone;
						triangleNormal.skinref = pmesh->skinref;

						if (triangleVertex.org[2] < vmin[2])
							vmin[2] = triangleVertex.org[2];

						AdjustVertexToQcOrigin(triangleVertex.org);
						ScaleVertexByQcScale(triangleVertex.org);

						// move vertex position to object space.
						vec3_t tmp;
						tmp = triangleVertex.org - g_bonefixup[triangleVertex.bone].worldorg;
						VectorTransform(tmp, g_bonefixup[triangleVertex.bone].im, triangleVertex.org);

						// move normal to object space.
						tmp = triangleNormal.org;
						VectorTransform(tmp, g_bonefixup[triangleVertex.bone].im, triangleNormal.org);
						triangleNormal.org.normalize();

						ptriangleVert->normindex = FindVertexNormalIndex(pmodel, &triangleNormal);
						ptriangleVert->vertindex = FindVertexIndex(pmodel, &triangleVertex);

						// tag bone as being used
						// pmodel->bone[bone].ref = 1;
					}
					else
					{
						Error("%s: error on line %d: %s", g_inputfilename, g_linecount, g_currentline);
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
						s_trianglevert_t *ptriv2;
						pmesh = FindMeshByTexture(pmodel, "..\\white.bmp");
						ptriv2 = FindMeshTriangleByIndex(pmesh, pmesh->numtris);

						ptriv2[0] = ptriangleVert[0];
						ptriv2[1] = ptriangleVert[1];
						ptriv2[2] = ptriangleVert[2];
					}
				}
				else
				{
					vec3_t triangleEdge1, triangleEdge2, surfaceNormal;
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
							s_trianglevert_t *ptriv2;

							printf("triangle reversed (%f %f %f)\n",
								   triangleNormals[0].dot(triangleNormals[1]),
								   triangleNormals[1].dot(triangleNormals[2]),
								   triangleNormals[2].dot(triangleNormals[0]));

							pmesh = FindMeshByTexture(pmodel, "..\\white.bmp");
							ptriv2 = FindMeshTriangleByIndex(pmesh, pmesh->numtris);

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

void Grab_SMDSkeleton(s_node_t *pnodes, s_bone_t *pbones)
{
	float posX, posY, posZ, rotX, rotY, rotZ;
	char cmd[1024];
	int node;

	while (fgets(g_currentline, sizeof(g_currentline), g_inputFile) != NULL)
	{
		g_linecount++;
		if (sscanf(g_currentline, "%d %f %f %f %f %f %f", &node, &posX, &posY, &posZ, &rotX, &rotY, &rotZ) == 7)
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
		else if (sscanf(g_currentline, "%s %d", cmd, &node)) // Delete this
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

int Grab_SMDNodes(s_node_t *pnodes)
{
	int index;
	char boneName[1024];
	int parent;
	int numBones = 0;

	while (fgets(g_currentline, sizeof(g_currentline), g_inputFile) != NULL)
	{
		g_linecount++;
		if (sscanf(g_currentline, "%d \"%[^\"]\" %d", &index, boneName, &parent) == 3)
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
	Error("Unexpected EOF at line %d\n", g_linecount);
	return 0;
}

void ParseSMD(s_model_t *pmodel)
{
	int time1;
	char cmd[1024];
	int option;

	sprintf(g_inputfilename, "%s/%s.smd", g_cdCommand, pmodel->name);
	time1 = FileTime(g_inputfilename);
	if (time1 == -1)
		Error("%s doesn't exist", g_inputfilename);

	printf("grabbing %s\n", g_inputfilename);

	if ((g_inputFile = fopen(g_inputfilename, "r")) == 0)
	{
		fprintf(stderr, "reader: could not open file '%s'\n", g_inputfilename);
	}
	g_linecount = 0;

	while (fgets(g_currentline, sizeof(g_currentline), g_inputFile) != NULL)
	{
		g_linecount++;
		sscanf(g_currentline, "%s %d", cmd, &option);
		if (std::strcmp(cmd, "version") == 0)
		{
			if (option != 1)
			{
				Error("bad version\n");
			}
		}
		else if (std::strcmp(cmd, "nodes") == 0)
		{
			pmodel->numbones = Grab_SMDNodes(pmodel->node);
		}
		else if (std::strcmp(cmd, "skeleton") == 0)
		{
			Grab_SMDSkeleton(pmodel->node, pmodel->skeleton);
		}
		else if (std::strcmp(cmd, "triangles") == 0)
		{
			Grab_SMDTriangles(pmodel);
		}
		else
		{
			printf("unknown studio command\n");
		}
	}
	fclose(g_inputFile);
}

void Cmd_Eyeposition(char* token)
{
	// rotate points into frame of reference so model points down the positive x
	// axis
	GetToken(false, token);
	g_eyepositionCommand[1] = atof(token);

	GetToken(false, token);
	g_eyepositionCommand[0] = -atof(token);

	GetToken(false, token);
	g_eyepositionCommand[2] = atof(token);
}

void Cmd_Flags(char *token)
{
	GetToken(false, token);
	g_flagsCommand = atoi(token);
}

void Cmd_Modelname(char *token)
{
	GetToken(false, token);
	strcpyn(g_modelnameCommand, token);
}

void Cmd_Body_OptionStudio(char *token)
{
	if (!GetToken(false, token))
		return;

	g_submodel[g_submodelscount] = (s_model_t *)std::calloc(1, sizeof(s_model_t));
	g_bodypart[g_bodygroupcount].pmodel[g_bodypart[g_bodygroupcount].nummodels] = g_submodel[g_submodelscount];

	strcpyn(g_submodel[g_submodelscount]->name, token);

	g_flagfliptriangles = 1;

	g_scaleBodyAndSequenceOption = g_scaleCommand;

	while (TokenAvailable())
	{
		GetToken(false, token);
		if (stricmp("reverse", token) == 0)
		{
			g_flagfliptriangles = 0;
		}
		else if (stricmp("scale", token) == 0)
		{
			GetToken(false, token);
			g_scaleBodyAndSequenceOption = atof(token);
		}
	}

	ParseSMD(g_submodel[g_submodelscount]);

	g_bodypart[g_bodygroupcount].nummodels++;
	g_submodelscount++;

	g_scaleBodyAndSequenceOption = g_scaleCommand;
}

int Cmd_Body_OptionBlank()
{
	g_submodel[g_submodelscount] = (s_model_t *)(1, sizeof(s_model_t));
	g_bodypart[g_bodygroupcount].pmodel[g_bodypart[g_bodygroupcount].nummodels] = g_submodel[g_submodelscount];

	strcpyn(g_submodel[g_submodelscount]->name, "blank");

	g_bodypart[g_bodygroupcount].nummodels++;
	g_submodelscount++;
	return 0;
}

void Cmd_Bodygroup(char *token)
{
	if (!GetToken(false, token))
		return;

	if (g_bodygroupcount == 0)
	{
		g_bodypart[g_bodygroupcount].base = 1;
	}
	else
	{
		g_bodypart[g_bodygroupcount].base = g_bodypart[g_bodygroupcount - 1].base * g_bodypart[g_bodygroupcount - 1].nummodels;
	}
	strcpyn(g_bodypart[g_bodygroupcount].name, token);

	while (true)
	{
		int is_started = 0;
		GetToken(true, token);
		if (g_endofscript)
			return;

		if (token[0] == '{')
		{
			is_started = 1;
		}
		else if (token[0] == '}')
		{
			break;
		}
		else if (stricmp("studio", token) == 0)
		{
			Cmd_Body_OptionStudio(token);
		}
		else if (stricmp("blank", token) == 0)
		{
			Cmd_Body_OptionBlank();
		}
	}

	g_bodygroupcount++;
	return;
}

void Cmd_Body(char *token)
{
	if (!GetToken(false, token))
		return;

	if (g_bodygroupcount == 0)
	{
		g_bodypart[g_bodygroupcount].base = 1;
	}
	else
	{
		g_bodypart[g_bodygroupcount].base = g_bodypart[g_bodygroupcount - 1].base * g_bodypart[g_bodygroupcount - 1].nummodels;
	}
	strcpyn(g_bodypart[g_bodygroupcount].name, token);

	Cmd_Body_OptionStudio(token);

	g_bodygroupcount++;
}

void Grab_OptionAnimation(s_animation_t *panim)
{
	vec3_t pos;
	vec3_t rot;
	char cmd[1024];
	int index;
	int t = -99999999;
	float cz, sz;
	int start = 99999;
	int end = 0;

	for (index = 0; index < panim->numbones; index++)
	{
		panim->pos[index] = (vec3_t *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(vec3_t));
		panim->rot[index] = (vec3_t *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(vec3_t));
	}

	cz = cos(g_rotateCommand);
	sz = sin(g_rotateCommand);

	while (fgets(g_currentline, sizeof(g_currentline), g_inputFile) != NULL)
	{
		g_linecount++;
		if (sscanf(g_currentline, "%d %f %f %f %f %f %f", &index, &pos[0], &pos[1], &pos[2], &rot[0], &rot[1], &rot[2]) == 7)
		{
			if (t >= panim->startframe && t <= panim->endframe)
			{
				if (panim->node[index].parent == -1)
				{
					AdjustVertexToQcOrigin(pos);
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
		else if (sscanf(g_currentline, "%s %d", cmd, &index))
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
				Error("Error(%d) : %s", g_linecount, g_currentline);
			}
		}
		else
		{
			Error("Error(%d) : %s", g_linecount, g_currentline);
		}
	}
	Error("unexpected EOF: %s\n", panim->name);
}

void Shift_OptionAnimation(s_animation_t *panim)
{
	int size;

	size = (panim->endframe - panim->startframe + 1) * sizeof(vec3_t);
	// shift
	for (int j = 0; j < panim->numbones; j++)
	{
		vec3_t *ppos;
		vec3_t *prot;

		ppos = (vec3_t *)std::calloc(1, size);
		prot = (vec3_t *)std::calloc(1, size);

		std::memcpy(ppos, &panim->pos[j][panim->startframe], size);
		std::memcpy(prot, &panim->rot[j][panim->startframe], size);

		free(panim->pos[j]);
		free(panim->rot[j]);

		panim->pos[j] = ppos;
		panim->rot[j] = prot;
	}
}

void Cmd_Sequence_OptionAnimation(char *name, s_animation_t *panim)
{
	int time1;
	char cmd[1024];
	int option;

	strcpyn(panim->name, name);

	sprintf(g_inputfilename, "%s/%s.smd", g_cdCommand, panim->name);
	time1 = FileTime(g_inputfilename);
	if (time1 == -1)
		Error("%s doesn't exist", g_inputfilename);

	printf("grabbing %s\n", g_inputfilename);

	if ((g_inputFile = fopen(g_inputfilename, "r")) == 0)
	{
		fprintf(stderr, "reader: could not open file '%s'\n", g_inputfilename);
		Error(0);
	}
	g_linecount = 0;

	while (fgets(g_currentline, sizeof(g_currentline), g_inputFile) != NULL)
	{
		g_linecount++;
		sscanf(g_currentline, "%s %d", cmd, &option);
		if (std::strcmp(cmd, "version") == 0)
		{
			if (option != 1)
			{
				Error("bad version\n");
			}
		}
		else if (std::strcmp(cmd, "nodes") == 0)
		{
			panim->numbones = Grab_SMDNodes(panim->node);
		}
		else if (std::strcmp(cmd, "skeleton") == 0)
		{
			Grab_OptionAnimation(panim);
			Shift_OptionAnimation(panim);
		}
		else
		{
			printf("unknown studio command : %s\n", cmd);
			while (fgets(g_currentline, sizeof(g_currentline), g_inputFile) != NULL)
			{
				g_linecount++;
				if (strncmp(g_currentline, "end", 3) == 0)
					break;
			}
		}
	}
	fclose(g_inputFile);
}

int Cmd_Sequence_OptionDeform(s_sequence_t *psequence) // delete this
{
	return 0;
}

int Cmd_Sequence_OptionEvent( char* token, s_sequence_t *psequence)
{
	int event;

	if (psequence->numevents + 1 >= MAXSTUDIOEVENTS)
	{
		printf("too many events\n");
		exit(0);
	}

	GetToken(false, token);
	event = atoi(token);
	psequence->event[psequence->numevents].event = event;

	GetToken(false, token);
	psequence->event[psequence->numevents].frame = atoi(token);

	psequence->numevents++;

	// option token
	if (TokenAvailable())
	{
		GetToken(false, token);
		if (token[0] == '}') // opps, hit the end
			return 1;
		// found an option
		std::strcpy(psequence->event[psequence->numevents - 1].options, token);
	}

	return 0;
}

int Cmd_Sequence_OptionFps(char *token,s_sequence_t *psequence)
{
	GetToken(false, token);

	psequence->fps = atof(token);

	return 0;
}

int Cmd_Sequence_OptionAddPivot(char *token, s_sequence_t *psequence)
{
	if (psequence->numpivots + 1 >= MAXSTUDIOPIVOTS)
	{
		printf("too many pivot points\n");
		exit(0);
	}

	GetToken(false, token);
	psequence->pivot[psequence->numpivots].index = atoi(token);

	GetToken(false, token);
	psequence->pivot[psequence->numpivots].start = atoi(token);

	GetToken(false, token);
	psequence->pivot[psequence->numpivots].end = atoi(token);

	psequence->numpivots++;

	return 0;
}

void Cmd_Origin(char *token)
{
	GetToken(false, token);
	g_originCommand[0] = atof(token);

	GetToken(false, token);
	g_originCommand[1] = atof(token);

	GetToken(false, token);
	g_originCommand[2] = atof(token);

	if (TokenAvailable())
	{
		GetToken(false, token);
		g_originCommandRotation = (atof(token) + 90) * (Q_PI / 180.0);
	}
}

void Cmd_Sequence_OptionOrigin(char *token)
{
	GetToken(false, token);
	g_sequenceOrigin[0] = atof(token);

	GetToken(false, token);
	g_sequenceOrigin[1] = atof(token);

	GetToken(false, token);
	g_sequenceOrigin[2] = atof(token);
}

void Cmd_Sequence_OptionRotate(char *token)
{
	GetToken(false, token);
	g_rotateCommand = (atof(token) + 90) * (Q_PI / 180.0);
}

void Cmd_ScaleUp(char *token)
{
	GetToken(false, token);
	g_scaleCommand = g_scaleBodyAndSequenceOption = atof(token);
}

void Cmd_Rotate(char *token) // XDM
{
	if (!GetToken(false,token))
		return;
	g_rotateCommand = (atof(token) + 90) * (Q_PI / 180.0);
}

void Cmd_Sequence_OptionScaleUp(char *token)
{
	GetToken(false, token);
	g_scaleBodyAndSequenceOption = atof(token);
}

int Cmd_SequenceGroup(char *token)
{
	GetToken(false, token);
	strcpyn(g_sequencegroupCommand[g_sequencegroupcount].label, token);
	g_sequencegroupcount++;

	return 0;
}

int Cmd_Sequence_OptionAction(char *szActivity)
{
	for (int i = 0; activity_map[i].name; i++)
	{
		if (stricmp(szActivity, activity_map[i].name) == 0)
			return activity_map[i].type;
	}
	// match ACT_#
	if (strnicmp(szActivity, "ACT_", 4) == 0)
	{
		return atoi(&szActivity[4]);
	}
	return 0;
}

int Cmd_Sequence(char *token)
{
	int depth = 0;
	char smdfilename[MAXSTUDIOBLENDS][1024];
	int i;
	int numblends = 0;
	int start = 0;
	int end = MAXSTUDIOANIMATIONS - 1;

	if (!GetToken(false, token))
		return 0;

	strcpyn(g_sequenceCommand[g_sequencecount].name, token);

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
			if (!GetToken(true, token))
			{
				break;
			}
		}
		else
		{
			if (!TokenAvailable())
			{
				break;
			}
			else
			{
				GetToken(false, token);
			}
		}

		if (g_endofscript)
		{
			if (depth != 0)
			{
				printf("missing }\n");
				exit(1);
			}
			return 1;
		}
		if (stricmp("{", token) == 0)
		{
			depth++;
		}
		else if (stricmp("}", token) == 0)
		{
			depth--;
		}
		else if (stricmp("deform", token) == 0)
		{
			Cmd_Sequence_OptionDeform(&g_sequenceCommand[g_sequencecount]);
		}
		else if (stricmp("event", token) == 0)
		{
			depth -= Cmd_Sequence_OptionEvent(token, &g_sequenceCommand[g_sequencecount]);
		}
		else if (stricmp("pivot", token) == 0)
		{
			Cmd_Sequence_OptionAddPivot(token, &g_sequenceCommand[g_sequencecount]);
		}
		else if (stricmp("fps", token) == 0)
		{
			Cmd_Sequence_OptionFps(token, &g_sequenceCommand[g_sequencecount]);
		}
		else if (stricmp("origin", token) == 0)
		{
			Cmd_Sequence_OptionOrigin(token);
		}
		else if (stricmp("rotate", token) == 0)
		{
			Cmd_Sequence_OptionRotate(token);
		}
		else if (stricmp("scale", token) == 0)
		{
			Cmd_Sequence_OptionScaleUp(token);
		}
		else if (strnicmp("loop", token, 4) == 0)
		{
			g_sequenceCommand[g_sequencecount].flags |= STUDIO_LOOPING;
		}
		else if (strnicmp("frame", token, 5) == 0)
		{
			GetToken(false, token);
			start = atoi(token);
			GetToken(false, token);
			end = atoi(token);
		}
		else if (strnicmp("blend", token, 5) == 0)
		{
			GetToken(false, token);
			g_sequenceCommand[g_sequencecount].blendtype[0] = lookupControl(token);
			GetToken(false, token);
			g_sequenceCommand[g_sequencecount].blendstart[0] = atof(token);
			GetToken(false, token);
			g_sequenceCommand[g_sequencecount].blendend[0] = atof(token);
		}
		else if (strnicmp("node", token, 4) == 0)
		{
			GetToken(false, token);
			g_sequenceCommand[g_sequencecount].entrynode = g_sequenceCommand[g_sequencecount].exitnode = atoi(token);
		}
		else if (strnicmp("transition", token, 4) == 0)
		{
			GetToken(false, token);
			g_sequenceCommand[g_sequencecount].entrynode = atoi(token);
			GetToken(false, token);
			g_sequenceCommand[g_sequencecount].exitnode = atoi(token);
		}
		else if (strnicmp("rtransition", token, 4) == 0)
		{
			GetToken(false, token);
			g_sequenceCommand[g_sequencecount].entrynode = atoi(token);
			GetToken(false, token);
			g_sequenceCommand[g_sequencecount].exitnode = atoi(token);
			g_sequenceCommand[g_sequencecount].nodeflags |= 1;
		}
		else if (lookupControl(token) != -1) // motion flags [motion extraction]
		{
			g_sequenceCommand[g_sequencecount].motiontype |= lookupControl(token);
		}
		else if (stricmp("animation", token) == 0)
		{
			GetToken(false, token);
			strcpyn(smdfilename[numblends++], token);
		}
		else if ((i = Cmd_Sequence_OptionAction(token)) != 0)
		{
			g_sequenceCommand[g_sequencecount].activity = i;
			GetToken(false, token);
			g_sequenceCommand[g_sequencecount].actweight = atoi(token);
		}
		else
		{
			strcpyn(smdfilename[numblends++], token);
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
		g_animationSequenceOption[g_animationcount] = (s_animation_t *)malloc(sizeof(s_animation_t));
		g_sequenceCommand[g_sequencecount].panim[i] = g_animationSequenceOption[g_animationcount];
		g_sequenceCommand[g_sequencecount].panim[i]->startframe = start;
		g_sequenceCommand[g_sequencecount].panim[i]->endframe = end;
		g_sequenceCommand[g_sequencecount].panim[i]->flags = 0;
		Cmd_Sequence_OptionAnimation(smdfilename[i], g_animationSequenceOption[g_animationcount]);
		g_animationcount++;
	}
	g_sequenceCommand[g_sequencecount].numblends = numblends;

	g_sequencecount++;

	return 0;
}

int Cmd_Controller(char *token)
{
	if (GetToken(false, token))
	{
		if (!std::strcmp("mouth", token))
		{
			g_bonecontrollerCommand[g_bonecontrollerscount].index = 4;
		}
		else
		{
			g_bonecontrollerCommand[g_bonecontrollerscount].index = atoi(token);
		}
		if (GetToken(false, token))
		{
			strcpyn(g_bonecontrollerCommand[g_bonecontrollerscount].name, token);
			GetToken(false, token);
			if ((g_bonecontrollerCommand[g_bonecontrollerscount].type = lookupControl(token)) == -1)
			{
				printf("unknown bonecontroller type '%s'\n", token);
				return 0;
			}
			GetToken(false, token);
			g_bonecontrollerCommand[g_bonecontrollerscount].start = atof(token);
			GetToken(false, token);
			g_bonecontrollerCommand[g_bonecontrollerscount].end = atof(token);

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

void Cmd_BBox(char *token)
{
	GetToken(false, token);
	g_bboxCommand[0][0] = atof(token);

	GetToken(false, token);
	g_bboxCommand[0][1] = atof(token);

	GetToken(false, token);
	g_bboxCommand[0][2] = atof(token);

	GetToken(false, token);
	g_bboxCommand[1][0] = atof(token);

	GetToken(false, token);
	g_bboxCommand[1][1] = atof(token);

	GetToken(false, token);
	g_bboxCommand[1][2] = atof(token);
}

void Cmd_CBox(char *token)
{
	GetToken(false, token);
	g_cboxCommand[0][0] = atof(token);

	GetToken(false, token);
	g_cboxCommand[0][1] = atof(token);

	GetToken(false, token);
	g_cboxCommand[0][2] = atof(token);

	GetToken(false, token);
	g_cboxCommand[1][0] = atof(token);

	GetToken(false, token);
	g_cboxCommand[1][1] = atof(token);

	GetToken(false, token);
	g_cboxCommand[1][2] = atof(token);
}

void Cmd_Mirror(char *token)
{
	GetToken(false, token);
	strcpyn(g_mirrorboneCommand[g_mirroredcount++], token);
}

void Cmd_Gamma(char *token)
{
	GetToken(false, token);
	g_gammaCommand = atof(token);
}

int Cmd_TextureGroup(char *token)
{
	int i;
	int depth = 0;
	int index = 0;
	int group = 0;

	if (g_texturescount == 0)
		Error("texturegroups must follow model loading\n");

	if (!GetToken(false, token))
		return 0;

	if (g_skinrefcount == 0)
		g_skinrefcount = g_texturescount;

	while (true)
	{
		if (!GetToken(true, token))
		{
			break;
		}

		if (g_endofscript)
		{
			if (depth != 0)
			{
				Error("missing }\n");
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
			i = FindTextureIndex(token);
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

int Cmd_Hitgroup(char *token)
{
	GetToken(false, token);
	g_hitgroupCommand[g_hitgroupscount].group = atoi(token);
	GetToken(false, token);
	strcpyn(g_hitgroupCommand[g_hitgroupscount].name, token);
	g_hitgroupscount++;

	return 0;
}

int Cmd_Hitbox(char *token)
{
	GetToken(false, token);
	g_hitboxCommand[g_hitboxescount].group = atoi(token);
	GetToken(false, token);
	strcpyn(g_hitboxCommand[g_hitboxescount].name, token);
	GetToken(false, token);
	g_hitboxCommand[g_hitboxescount].bmin[0] = atof(token);
	GetToken(false, token);
	g_hitboxCommand[g_hitboxescount].bmin[1] = atof(token);
	GetToken(false, token);
	g_hitboxCommand[g_hitboxescount].bmin[2] = atof(token);
	GetToken(false, token);
	g_hitboxCommand[g_hitboxescount].bmax[0] = atof(token);
	GetToken(false, token);
	g_hitboxCommand[g_hitboxescount].bmax[1] = atof(token);
	GetToken(false, token);
	g_hitboxCommand[g_hitboxescount].bmax[2] = atof(token);

	g_hitboxescount++;

	return 0;
}

int Cmd_Attachment(char *token)
{
	// index
	GetToken(false, token);
	g_attachmentCommand[g_attachmentscount].index = atoi(token);

	// bone name
	GetToken(false, token);
	strcpyn(g_attachmentCommand[g_attachmentscount].bonename, token);

	// position
	GetToken(false, token);
	g_attachmentCommand[g_attachmentscount].org[0] = atof(token);
	GetToken(false, token);
	g_attachmentCommand[g_attachmentscount].org[1] = atof(token);
	GetToken(false, token);
	g_attachmentCommand[g_attachmentscount].org[2] = atof(token);

	if (TokenAvailable())
		GetToken(false, token);

	if (TokenAvailable())
		GetToken(false, token);

	g_attachmentscount++;
	return 0;
}

void Cmd_Renamebone(char *token)
{
	// from
	GetToken(false, token);
	std::strcpy(g_renameboneCommand[g_renamebonecount].from, token);

	// to
	GetToken(false, token);
	std::strcpy(g_renameboneCommand[g_renamebonecount].to, token);

	g_renamebonecount++;
}

void Cmd_TexRenderMode(char *token)
{
	char tex_name[256];
	GetToken(false, token);
	std::strcpy(tex_name, token);

	GetToken(false, token);
	if (!std::strcmp(token, "additive"))
	{
		g_texture[FindTextureIndex(tex_name)].flags |= STUDIO_NF_ADDITIVE;
	}
	else if (!std::strcmp(token, "masked"))
	{
		g_texture[FindTextureIndex(tex_name)].flags |= STUDIO_NF_MASKED;
	}
	else if (!std::strcmp(token, "fullbright"))
	{
		g_texture[FindTextureIndex(tex_name)].flags |= STUDIO_NF_FULLBRIGHT;
	}
	else if (!std::strcmp(token, "flatshade"))
	{
		g_texture[FindTextureIndex(tex_name)].flags |= STUDIO_NF_FLATSHADE;
	}
	else
		printf("Texture '%s' has unknown render mode '%s'!\n", tex_name, token);
}

void ParseQCscript()
{
	char token[MAXTOKEN];
	bool iscdalreadyset = false;
	while (true)
	{
		// Look for a line starting with a $ command
		while (true)
		{
			GetToken(true, token);
			if (g_endofscript)
				return;

			if (token[0] == '$')
				break;

			// Skip the rest of the line
			while (TokenAvailable())
				GetToken(false, token);
		}

		// Process recognized commands
		if (!std::strcmp(token, "$modelname"))
		{
			Cmd_Modelname(token);
		}
		else if (!std::strcmp(token, "$cd"))
		{
			if (iscdalreadyset)
				Error("Two $cd in one model");
			iscdalreadyset = true;
			GetToken(false, token);

			std::strcpy(g_cdPartialPath, token);
			std::strcpy(g_cdCommand, ExpandPath(token));
		}
		else if (!std::strcmp(token, "$cdtexture"))
		{
			while (TokenAvailable())
			{
				GetToken(false, token);
				std::strcpy(g_cdtextureCommand[g_cdtexturepathcount], ExpandPath(token));
				g_cdtexturepathcount++;
			}
		}
		else if (!std::strcmp(token, "$scale"))
		{
			Cmd_ScaleUp(token);
		}
		else if (!stricmp(token, "$rotate")) // XDM
		{
			Cmd_Rotate(token);
		}
		else if (!std::strcmp(token, "$controller"))
		{
			Cmd_Controller(token);
		}
		else if (!std::strcmp(token, "$body"))
		{
			Cmd_Body(token);
		}
		else if (!std::strcmp(token, "$bodygroup"))
		{
			Cmd_Bodygroup(token);
		}
		else if (!std::strcmp(token, "$sequence"))
		{
			Cmd_Sequence(token);
		}
		else if (!std::strcmp(token, "$sequencegroup"))
		{
			Cmd_SequenceGroup(token);
		}
		else if (!std::strcmp(token, "$eyeposition"))
		{
			Cmd_Eyeposition(token);
		}
		else if (!std::strcmp(token, "$origin"))
		{
			Cmd_Origin(token);
		}
		else if (!std::strcmp(token, "$bbox"))
		{
			Cmd_BBox(token);
		}
		else if (!std::strcmp(token, "$cbox"))
		{
			Cmd_CBox(token);
		}
		else if (!std::strcmp(token, "$mirrorbone"))
		{
			Cmd_Mirror(token);
		}
		else if (!std::strcmp(token, "$gamma"))
		{
			Cmd_Gamma(token);
		}
		else if (!std::strcmp(token, "$flags"))
		{
			Cmd_Flags(token);
		}
		else if (!std::strcmp(token, "$texturegroup"))
		{
			Cmd_TextureGroup(token);
		}
		else if (!std::strcmp(token, "$hgroup"))
		{
			Cmd_Hitgroup(token);
		}
		else if (!std::strcmp(token, "$hbox"))
		{
			Cmd_Hitbox(token);
		}
		else if (!std::strcmp(token, "$attachment"))
		{
			Cmd_Attachment(token);
		}
		else if (!std::strcmp(token, "$renamebone"))
		{
			Cmd_Renamebone(token);
		}
		else if (!std::strcmp(token, "$texrendermode"))
		{
			Cmd_TexRenderMode(token);
		}
		else
		{
			Error("bad command %s\n", token);
		}
	}
}

int main(int argc, char **argv)
{
	int i;
	char path[1024];

	g_scaleCommand = 1.0;
	g_originCommandRotation = Q_PI / 2;

	g_numtextureteplacements = 0;
	g_flagreversedtriangles = 0;
	g_flagbadnormals = 0;
	g_flagfliptriangles = 1;
	g_flagkeepallbones = false;

	g_flagnormalblendangle = cos(2.0 * (Q_PI / 180.0));

	g_gammaCommand = 1.8;

	if (argc == 1)
		Error("usage: studiomdl <flags>\n [-t texture]\n -r(tag reversed)\n -n(tag bad normals)\n -f(flip all triangles)\n [-a normal_blend_angle]\n -h(dump hboxes)\n -i(ignore warnings) \n b(keep all unused bones)\n file.qc");

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
				g_flagnormalblendangle = cos(atof(argv[i]) * (Q_PI / 180.0));
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
	LoadScriptFile(path);
	// parse it
	std::strcpy(g_modelnameCommand, argv[i]);
	ParseQCscript();
	SetSkinValues();
	SimplifyModel();
	WriteFile();

	return 0;
}
