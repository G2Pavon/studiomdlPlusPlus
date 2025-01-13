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

char inputFilename[1024];
FILE *inputFile;
char currentLine[1024];
int lineCount;
bool isCdSet;
char defaultTextures[16][256];
char sourceTexture[16][256];
int numTextureReplacements;
s_bonefixup_t boneFixup[MAXSTUDIOSRCBONES];

// studiomdl.exe args -----------
int flagReversedTriangles;
int flagBadNormals;
int flagFlipTriangles;
float flagNormalBlendAngle;
int flagDumpHitboxes;
int flagIgnoreWarnings;
bool flagKeepAllBones;

// QC Command variables -----------------
char cdPartialPath[256];							 // $cd
char cdCommand[256];								 // $cd
int cdtexturePathCount;								 // $cdtexture <paths> (max paths = 18, idk why)
char cdtextureCommand[16][256];						 // $cdtexture
float scaleCommand;									 // $scale
float scaleBodyAndSequenceOption;					 // $body studio <value> // also for $sequence
vec3_t originCommand;								 // $origin
float originCommandRotation;						 // $origin <X> <Y> <Z> <rotation>
float rotateCommand;								 // $rotate and $sequence <sequence name> <SMD path> {[rotate <zrotation>]} only z axis
vec3_t sequenceOrigin;								 // $sequence <sequence name> <SMD path>  {[origin <X> <Y> <Z>]}
float gammaCommand;									 // $$gamma
s_renamebone_t renameboneCommand[MAXSTUDIOSRCBONES]; // $renamebone
int renameboneCount;
s_hitgroup_t hitgroupCommand[MAXSTUDIOSRCBONES]; // $hgroup
int hitgroupsCount;
char mirrorboneCommand[MAXSTUDIOSRCBONES][64]; // $mirrorbone
int mirroredCount;
s_animation_t *animationSequenceOption[MAXSTUDIOSEQUENCES * MAXSTUDIOBLENDS]; // $sequence, each sequence can have 16 blends
int animationCount;
int texturegroupCommand[32][32][32]; // $texturegroup
int texturegroupCount;				 // unnecessary? since engine doesn't support multiple texturegroups
int texturegrouplayers[32];
int texturegroupreps[32];
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
	for (i = 0; i < sequenceCount; i++)
	{
		if (sequence[i].numframes > 1)
		{
			// assume 0 for now.
			int typeMotion;
			vec3_t *ptrPos;
			vec3_t motion = {0, 0, 0};
			typeMotion = sequence[i].motiontype;
			ptrPos = sequence[i].panim[0]->pos[0];

			k = sequence[i].numframes - 1;

			if (typeMotion & STUDIO_LX)
				motion[0] = ptrPos[k][0] - ptrPos[0][0];
			if (typeMotion & STUDIO_LY)
				motion[1] = ptrPos[k][1] - ptrPos[0][1];
			if (typeMotion & STUDIO_LZ)
				motion[2] = ptrPos[k][2] - ptrPos[0][2];

			for (j = 0; j < sequence[i].numframes; j++)
			{
				vec3_t adjustedPosition;
				for (k = 0; k < sequence[i].panim[0]->numbones; k++)
				{
					if (sequence[i].panim[0]->node[k].parent == -1)
					{
						ptrPos = sequence[i].panim[0]->pos[k];

						adjustedPosition = motion * (j * 1.0 / (sequence[i].numframes - 1));
						for (blendIndex = 0; blendIndex < sequence[i].numblends; blendIndex++)
						{
							sequence[i].panim[blendIndex]->pos[k][j] = sequence[i].panim[blendIndex]->pos[k][j] - adjustedPosition;
						}
					}
				}
			}

			sequence[i].linearmovement = motion;
		}
		else
		{ //
			sequence[i].linearmovement = sequence[i].linearmovement - sequence[i].linearmovement;
		}
	}

	// extract unused motion
	for (i = 0; i < sequenceCount; i++)
	{
		int typeUnusedMotion;
		typeUnusedMotion = sequence[i].motiontype;
		for (k = 0; k < sequence[i].panim[0]->numbones; k++)
		{
			if (sequence[i].panim[0]->node[k].parent == -1)
			{
				for (blendIndex = 0; blendIndex < sequence[i].numblends; blendIndex++)
				{
					float motion[6];
					motion[0] = sequence[i].panim[blendIndex]->pos[k][0][0];
					motion[1] = sequence[i].panim[blendIndex]->pos[k][0][1];
					motion[2] = sequence[i].panim[blendIndex]->pos[k][0][2];
					motion[3] = sequence[i].panim[blendIndex]->rot[k][0][0];
					motion[4] = sequence[i].panim[blendIndex]->rot[k][0][1];
					motion[5] = sequence[i].panim[blendIndex]->rot[k][0][2];

					for (j = 0; j < sequence[i].numframes; j++)
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
							sequence[i].panim[blendIndex]->rot[k][j][0] = motion[3];
						if (typeUnusedMotion & STUDIO_YR)
							sequence[i].panim[blendIndex]->rot[k][j][1] = motion[4];
						if (typeUnusedMotion & STUDIO_ZR)
							sequence[i].panim[blendIndex]->rot[k][j][2] = motion[5];
					}
				}
			}
		}
	}

	// extract auto motion
	for (i = 0; i < sequenceCount; i++)
	{
		// assume 0 for now.
		int typeAutoMotion;
		vec3_t *ptrAutoPos;
		vec3_t *ptrAutoRot;
		vec3_t motion = {0, 0, 0};
		vec3_t angles = {0, 0, 0};

		typeAutoMotion = sequence[i].motiontype;
		for (j = 0; j < sequence[i].numframes; j++)
		{
			ptrAutoPos = sequence[i].panim[0]->pos[0];
			ptrAutoRot = sequence[i].panim[0]->rot[0];

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

			sequence[i].automovepos[j] = motion;
			sequence[i].automoveangle[j] = angles;
		}
	}
}

void OptimizeAnimations()
{
	int startFrame, endFrame;
	int typeMotion;
	int iError = 0;

	// optimize animations
	for (int i = 0; i < sequenceCount; i++)
	{
		sequence[i].numframes = sequence[i].panim[0]->endframe - sequence[i].panim[0]->startframe + 1;

		// force looping animations to be looping
		if (sequence[i].flags & STUDIO_LOOPING)
		{
			for (int j = 0; j < sequence[i].panim[0]->numbones; j++)
			{
				for (int blends = 0; blends < sequence[i].numblends; blends++)
				{
					vec3_t *ppos = sequence[i].panim[blends]->pos[j];
					vec3_t *prot = sequence[i].panim[blends]->rot[j];

					startFrame = 0;						  // sequence[i].panim[q]->startframe;
					endFrame = sequence[i].numframes - 1; // sequence[i].panim[q]->endframe;

					typeMotion = sequence[i].motiontype;
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

		for (int j = 0; j < sequence[i].numevents; j++)
		{
			if (sequence[i].event[j].frame < sequence[i].panim[0]->startframe)
			{
				printf("sequence %s has event (%d) before first frame (%d)\n", sequence[i].name, sequence[i].event[j].frame, sequence[i].panim[0]->startframe);
				sequence[i].event[j].frame = sequence[i].panim[0]->startframe;
				iError++;
			}
			if (sequence[i].event[j].frame > sequence[i].panim[0]->endframe)
			{
				printf("sequence %s has event (%d) after last frame (%d)\n", sequence[i].name, sequence[i].event[j].frame, sequence[i].panim[0]->endframe);
				sequence[i].event[j].frame = sequence[i].panim[0]->endframe;
				iError++;
			}
		}

		sequence[i].frameoffset = sequence[i].panim[0]->startframe;
	}
}

int findNode(char *name)
{
	for (int k = 0; k < bonesCount; k++)
	{
		if (std::strcmp(bonetable[k].name, name) == 0)
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
	for (i = 0; i < sequenceCount; i++)
	{
		if (sequence[i].entrynode != sequence[i].exitnode)
		{
			xnode[sequence[i].entrynode - 1][sequence[i].exitnode - 1] = sequence[i].exitnode;
			if (sequence[i].nodeflags)
			{
				xnode[sequence[i].exitnode - 1][sequence[i].entrynode - 1] = sequence[i].entrynode;
			}
		}
		if (sequence[i].entrynode > numxnodes)
			numxnodes = sequence[i].entrynode;
	}

	// Add multi-stage transitions
	while (true)
	{
		iHit = 0;
		for (i = 1; i <= numxnodes; i++)
		{
			for (j = 1; j <= numxnodes; j++)
			{
				// If I can't go there directly
				if (i != j && xnode[i - 1][j - 1] == 0)
				{
					for (int k = 1; k < numxnodes; k++)
					{
						// But I found someone who knows how that I can get to
						if (xnode[k - 1][j - 1] > 0 && xnode[i - 1][k - 1] > 0)
						{
							// Then go to them
							xnode[i - 1][j - 1] = -xnode[i - 1][k - 1];
							iHit = 1;
							break;
						}
					}
				}
			}
		}

		// Reset previous pass so the links can be used in the next pass
		for (i = 1; i <= numxnodes; i++)
		{
			for (j = 1; j <= numxnodes; j++)
			{
				xnode[i - 1][j - 1] = abs(xnode[i - 1][j - 1]);
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
	for (i = 0; i < modelsCount; i++)
	{
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			model[i]->boneref[k] = flagKeepAllBones;
		}
		for (j = 0; j < model[i]->numverts; j++)
		{
			model[i]->boneref[model[i]->vert[j].bone] = 1;
		}
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			// tag parent bones as used if child has been used
			if (model[i]->boneref[k])
			{
				n = model[i]->node[k].parent;
				while (n != -1 && !model[i]->boneref[n])
				{
					model[i]->boneref[n] = 1;
					n = model[i]->node[n].parent;
				}
			}
		}
	}

	// rename model bones if needed
	for (i = 0; i < modelsCount; i++)
	{
		for (j = 0; j < model[i]->numbones; j++)
		{
			for (k = 0; k < renameboneCount; k++)
			{
				if (!std::strcmp(model[i]->node[j].name, renameboneCommand[k].from))
				{
					std::strcpy(model[i]->node[j].name, renameboneCommand[k].to);
					break;
				}
			}
		}
	}

	// union of all used bones
	bonesCount = 0;
	for (i = 0; i < modelsCount; i++)
	{
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			model[i]->boneimap[k] = -1;
		}
		for (j = 0; j < model[i]->numbones; j++)
		{
			if (model[i]->boneref[j])
			{
				k = findNode(model[i]->node[j].name);
				if (k == -1)
				{
					// create new bone
					k = bonesCount;
					strcpyn(bonetable[k].name, model[i]->node[j].name);
					if ((n = model[i]->node[j].parent) != -1)
						bonetable[k].parent = findNode(model[i]->node[n].name);
					else
						bonetable[k].parent = -1;
					bonetable[k].bonecontroller = 0;
					bonetable[k].flags = 0;
					// set defaults
					defaultpos[k] = (vec3_t *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(vec3_t));
					defaultrot[k] = (vec3_t *)std::calloc(MAXSTUDIOANIMATIONS, sizeof(vec3_t));
					for (n = 0; n < MAXSTUDIOANIMATIONS; n++)
					{
						defaultpos[k][n] = model[i]->skeleton[j].pos;
						defaultrot[k][n] = model[i]->skeleton[j].rot;
					}
					bonetable[k].pos = model[i]->skeleton[j].pos;
					bonetable[k].rot = model[i]->skeleton[j].rot;
					bonesCount++;
				}
				else
				{
					// double check parent assignments
					n = model[i]->node[j].parent;
					if (n != -1)
						n = findNode(model[i]->node[n].name);
					m = bonetable[k].parent;

					if (n != m)
					{
						printf("illegal parent bone replacement in model \"%s\"\n\t\"%s\" has \"%s\", previously was \"%s\"\n",
							   model[i]->name,
							   model[i]->node[j].name,
							   (n != -1) ? bonetable[n].name : "ROOT",
							   (m != -1) ? bonetable[m].name : "ROOT");
						iError++;
					}
				}
				model[i]->bonemap[j] = k;
				model[i]->boneimap[k] = j;
			}
		}
	}

	if (iError && !(flagIgnoreWarnings))
	{
		exit(1);
	}

	if (bonesCount >= MAXSTUDIOBONES)
	{
		Error("Too many bones used in model, used %d, max %d\n", bonesCount, MAXSTUDIOBONES);
	}

	// rename sequence bones if needed
	for (i = 0; i < sequenceCount; i++)
	{
		for (j = 0; j < sequence[i].panim[0]->numbones; j++)
		{
			for (k = 0; k < renameboneCount; k++)
			{
				if (!std::strcmp(sequence[i].panim[0]->node[j].name, renameboneCommand[k].from))
				{
					std::strcpy(sequence[i].panim[0]->node[j].name, renameboneCommand[k].to);
					break;
				}
			}
		}
	}

	// map each sequences bone list to master list
	for (i = 0; i < sequenceCount; i++)
	{
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			sequence[i].panim[0]->boneimap[k] = -1;
		}
		for (j = 0; j < sequence[i].panim[0]->numbones; j++)
		{
			k = findNode(sequence[i].panim[0]->node[j].name);

			if (k == -1)
			{
				sequence[i].panim[0]->bonemap[j] = -1;
			}
			else
			{
				char *szAnim = "ROOT";
				char *szNode = "ROOT";

				// whoa, check parent connections!
				if (sequence[i].panim[0]->node[j].parent != -1)
					szAnim = sequence[i].panim[0]->node[sequence[i].panim[0]->node[j].parent].name;

				if (bonetable[k].parent != -1)
					szNode = bonetable[bonetable[k].parent].name;

				if (std::strcmp(szAnim, szNode))
				{
					printf("illegal parent bone replacement in sequence \"%s\"\n\t\"%s\" has \"%s\", reference has \"%s\"\n",
						   sequence[i].name,
						   sequence[i].panim[0]->node[j].name,
						   szAnim,
						   szNode);
					iError++;
				}
				sequence[i].panim[0]->bonemap[j] = k;
				sequence[i].panim[0]->boneimap[k] = j;
			}
		}
	}
	if (iError && !(flagIgnoreWarnings))
	{
		exit(1);
	}

	// link bonecontrollers
	for (i = 0; i < bonecontrollersCount; i++)
	{
		for (j = 0; j < bonesCount; j++)
		{
			if (stricmp(bonecontroller[i].name, bonetable[j].name) == 0)
				break;
		}
		if (j >= bonesCount)
		{
			Error("unknown bonecontroller link '%s'\n", bonecontroller[i].name);
		}
		bonecontroller[i].bone = j;
	}

	// link attachments
	for (i = 0; i < attachmentsCount; i++)
	{
		for (j = 0; j < bonesCount; j++)
		{
			if (stricmp(attachment[i].bonename, bonetable[j].name) == 0)
				break;
		}
		if (j >= bonesCount)
		{
			Error("unknown attachment link '%s'\n", attachment[i].bonename);
		}
		attachment[i].bone = j;
	}

	// relink model
	for (i = 0; i < modelsCount; i++)
	{
		for (j = 0; j < model[i]->numverts; j++)
		{
			model[i]->vert[j].bone = model[i]->bonemap[model[i]->vert[j].bone];
		}
		for (j = 0; j < model[i]->numnorms; j++)
		{
			model[i]->normal[j].bone = model[i]->bonemap[model[i]->normal[j].bone];
		}
	}

	// set hitgroups
	for (k = 0; k < bonesCount; k++)
	{
		bonetable[k].group = -9999;
	}
	for (j = 0; j < hitgroupsCount; j++)
	{
		for (k = 0; k < bonesCount; k++)
		{
			if (std::strcmp(bonetable[k].name, hitgroupCommand[j].name) == 0)
			{
				bonetable[k].group = hitgroupCommand[j].group;
				break;
			}
		}
		if (k >= bonesCount)
			Error("cannot find bone %s for hitgroup %d\n", hitgroupCommand[j].name, hitgroupCommand[j].group);
	}
	for (k = 0; k < bonesCount; k++)
	{
		if (bonetable[k].group == -9999)
		{
			if (bonetable[k].parent != -1)
				bonetable[k].group = bonetable[bonetable[k].parent].group;
			else
				bonetable[k].group = 0;
		}
	}

	if (hitboxesCount == 0)
	{
		// find intersection box volume for each bone
		for (k = 0; k < bonesCount; k++)
		{
			for (j = 0; j < 3; j++)
			{
				bonetable[k].bmin[j] = 0.0;
				bonetable[k].bmax[j] = 0.0;
			}
		}
		// try all the connect vertices
		for (i = 0; i < modelsCount; i++)
		{
			vec3_t p;
			for (j = 0; j < model[i]->numverts; j++)
			{
				p = model[i]->vert[j].org;
				k = model[i]->vert[j].bone;

				if (p[0] < bonetable[k].bmin[0])
					bonetable[k].bmin[0] = p[0];
				if (p[1] < bonetable[k].bmin[1])
					bonetable[k].bmin[1] = p[1];
				if (p[2] < bonetable[k].bmin[2])
					bonetable[k].bmin[2] = p[2];
				if (p[0] > bonetable[k].bmax[0])
					bonetable[k].bmax[0] = p[0];
				if (p[1] > bonetable[k].bmax[1])
					bonetable[k].bmax[1] = p[1];
				if (p[2] > bonetable[k].bmax[2])
					bonetable[k].bmax[2] = p[2];
			}
		}
		// add in all your children as well
		for (k = 0; k < bonesCount; k++)
		{
			if ((j = bonetable[k].parent) != -1)
			{
				if (bonetable[k].pos[0] < bonetable[j].bmin[0])
					bonetable[j].bmin[0] = bonetable[k].pos[0];
				if (bonetable[k].pos[1] < bonetable[j].bmin[1])
					bonetable[j].bmin[1] = bonetable[k].pos[1];
				if (bonetable[k].pos[2] < bonetable[j].bmin[2])
					bonetable[j].bmin[2] = bonetable[k].pos[2];
				if (bonetable[k].pos[0] > bonetable[j].bmax[0])
					bonetable[j].bmax[0] = bonetable[k].pos[0];
				if (bonetable[k].pos[1] > bonetable[j].bmax[1])
					bonetable[j].bmax[1] = bonetable[k].pos[1];
				if (bonetable[k].pos[2] > bonetable[j].bmax[2])
					bonetable[j].bmax[2] = bonetable[k].pos[2];
			}
		}

		for (k = 0; k < bonesCount; k++)
		{
			if (bonetable[k].bmin[0] < bonetable[k].bmax[0] - 1 && bonetable[k].bmin[1] < bonetable[k].bmax[1] - 1 && bonetable[k].bmin[2] < bonetable[k].bmax[2] - 1)
			{
				hitbox[hitboxesCount].bone = k;
				hitbox[hitboxesCount].group = bonetable[k].group;
				hitbox[hitboxesCount].bmin = bonetable[k].bmin;
				hitbox[hitboxesCount].bmax = bonetable[k].bmax;

				if (flagDumpHitboxes)
				{
					printf("$hbox %d \"%s\" %.2f %.2f %.2f  %.2f %.2f %.2f\n",
						   hitbox[hitboxesCount].group,
						   bonetable[hitbox[hitboxesCount].bone].name,
						   hitbox[hitboxesCount].bmin[0], hitbox[hitboxesCount].bmin[1], hitbox[hitboxesCount].bmin[2],
						   hitbox[hitboxesCount].bmax[0], hitbox[hitboxesCount].bmax[1], hitbox[hitboxesCount].bmax[2]);
				}
				hitboxesCount++;
			}
		}
	}
	else
	{
		for (j = 0; j < hitboxesCount; j++)
		{
			for (k = 0; k < bonesCount; k++)
			{
				if (std::strcmp(bonetable[k].name, hitbox[j].name) == 0)
				{
					hitbox[j].bone = k;
					break;
				}
			}
			if (k >= bonesCount)
				Error("cannot find bone %s for bbox\n", hitbox[j].name);
		}
	}

	// relink animations
	for (i = 0; i < sequenceCount; i++)
	{
		vec3_t *origpos[MAXSTUDIOSRCBONES] = {0};
		vec3_t *origrot[MAXSTUDIOSRCBONES] = {0};

		for (q = 0; q < sequence[i].numblends; q++)
		{
			// save pointers to original animations
			for (j = 0; j < sequence[i].panim[q]->numbones; j++)
			{
				origpos[j] = sequence[i].panim[q]->pos[j];
				origrot[j] = sequence[i].panim[q]->rot[j];
			}

			for (j = 0; j < bonesCount; j++)
			{
				if ((k = sequence[i].panim[0]->boneimap[j]) >= 0)
				{
					// link to original animations
					sequence[i].panim[q]->pos[j] = origpos[k];
					sequence[i].panim[q]->rot[j] = origrot[k];
				}
				else
				{
					// link to dummy animations
					sequence[i].panim[q]->pos[j] = defaultpos[j];
					sequence[i].panim[q]->rot[j] = defaultrot[j];
				}
			}
		}
	}

	// find scales for all bones
	for (j = 0; j < bonesCount; j++)
	{
		for (k = 0; k < 6; k++)
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

			for (i = 0; i < sequenceCount; i++)
			{
				for (q = 0; q < sequence[i].numblends; q++)
				{
					for (n = 0; n < sequence[i].numframes; n++)
					{
						float v;
						switch (k)
						{
						case 0:
						case 1:
						case 2:
							v = (sequence[i].panim[q]->pos[j][n][k] - bonetable[j].pos[k]);
							break;
						case 3:
						case 4:
						case 5:
							v = (sequence[i].panim[q]->rot[j][n][k - 3] - bonetable[j].rot[k - 3]);
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
				bonetable[j].posscale[k] = scale;
				break;
			case 3:
			case 4:
			case 5:
				bonetable[j].rotscale[k - 3] = scale;
				break;
			}
		}
	}

	// find bounding box for each sequence
	for (i = 0; i < sequenceCount; i++)
	{
		vec3_t bmin, bmax;

		// find intersection box volume for each bone
		for (j = 0; j < 3; j++)
		{
			bmin[j] = 9999.0;
			bmax[j] = -9999.0;
		}

		for (q = 0; q < sequence[i].numblends; q++)
		{
			for (n = 0; n < sequence[i].numframes; n++)
			{
				float bonetransform[MAXSTUDIOBONES][3][4]; // bone transformation matrix
				float bonematrix[3][4];					   // local transformation matrix
				vec3_t pos;

				for (j = 0; j < bonesCount; j++)
				{
					vec3_t angle;

					// convert to degrees
					angle[0] = sequence[i].panim[q]->rot[j][n][0] * (180.0 / Q_PI);
					angle[1] = sequence[i].panim[q]->rot[j][n][1] * (180.0 / Q_PI);
					angle[2] = sequence[i].panim[q]->rot[j][n][2] * (180.0 / Q_PI);

					AngleMatrix(angle, bonematrix);

					bonematrix[0][3] = sequence[i].panim[q]->pos[j][n][0];
					bonematrix[1][3] = sequence[i].panim[q]->pos[j][n][1];
					bonematrix[2][3] = sequence[i].panim[q]->pos[j][n][2];

					if (bonetable[j].parent == -1)
					{
						MatrixCopy(bonematrix, bonetransform[j]);
					}
					else
					{
						R_ConcatTransforms(bonetransform[bonetable[j].parent], bonematrix, bonetransform[j]);
					}
				}

				for (k = 0; k < modelsCount; k++)
				{
					for (j = 0; j < model[k]->numverts; j++)
					{
						VectorTransform(model[k]->vert[j].org, bonetransform[model[k]->vert[j].bone], pos);

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

		sequence[i].bmin = bmin;
		sequence[i].bmax = bmax;
	}

	// reduce animations
	{
		int total = 0;
		int changes = 0;
		int p;

		for (i = 0; i < sequenceCount; i++)
		{
			for (q = 0; q < sequence[i].numblends; q++)
			{
				for (j = 0; j < bonesCount; j++)
				{
					for (k = 0; k < 6; k++)
					{
						mstudioanimvalue_t *pcount, *pvalue;
						float v;
						short value[MAXSTUDIOANIMATIONS];
						mstudioanimvalue_t data[MAXSTUDIOANIMATIONS];

						for (n = 0; n < sequence[i].numframes; n++)
						{
							switch (k)
							{
							case 0:
							case 1:
							case 2:
								value[n] = (sequence[i].panim[q]->pos[j][n][k] - bonetable[j].pos[k]) / bonetable[j].posscale[k];
								break;
							case 3:
							case 4:
							case 5:
								v = (sequence[i].panim[q]->rot[j][n][k - 3] - bonetable[j].rot[k - 3]);
								if (v >= Q_PI)
									v -= Q_PI * 2;
								if (v < -Q_PI)
									v += Q_PI * 2;

								value[n] = v / bonetable[j].rotscale[k - 3];
								break;
							}
						}
						if (n == 0)
							Error("no animation frames: \"%s\"\n", sequence[i].name);

						sequence[i].panim[q]->numanim[j][k] = 0;

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

						sequence[i].panim[q]->numanim[j][k] = pvalue - data;
						if (sequence[i].panim[q]->numanim[j][k] == 2 && value[0] == 0)
						{
							sequence[i].panim[q]->numanim[j][k] = 0;
						}
						else
						{
							sequence[i].panim[q]->anim[j][k] = (mstudioanimvalue_t *)std::calloc(pvalue - data, sizeof(mstudioanimvalue_t));
							std::memcpy(sequence[i].panim[q]->anim[j][k], data, (pvalue - data) * sizeof(mstudioanimvalue_t));
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
	for (i = 0; i < texturesCount; i++)
	{
		if (stricmp(texture[i].name, texturename) == 0)
		{
			return i;
		}
	}

	strcpyn(texture[i].name, texturename);

	// XDM: allow such names as "tex_chrome_bright" - chrome and full brightness effects
	if (stristr(texturename, "chrome") != NULL)
		texture[i].flags = STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME;
	else if (stristr(texturename, "bright") != NULL)
		texture[i].flags = STUDIO_NF_FLATSHADE | STUDIO_NF_FULLBRIGHT;
	else
		texture[i].flags = 0;

	texturesCount++;
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
		if (pmodel->normal[i].org.dot(pnormal->org) > flagNormalBlendAngle && pmodel->normal[i].bone == pnormal->bone && pmodel->normal[i].skinref == pnormal->skinref)
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
	org[0] = (org[0] - sequenceOrigin[0]);
	org[1] = (org[1] - sequenceOrigin[1]);
	org[2] = (org[2] - sequenceOrigin[2]);
}

void ScaleVertexByQcScale(vec3_t org)
{
	org[0] = org[0] * scaleBodyAndSequenceOption;
	org[1] = org[1] * scaleBodyAndSequenceOption;
	org[2] = org[2] * scaleBodyAndSequenceOption;
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

	if (result = LoadBMP(filename, &ptexture->ppicture, (byte **)&ptexture->ppal, &ptexture->srcwidth, &ptexture->srcheight))
	{
		Error("error %d reading BMP image \"%s\"\n", result, filename);
	}
}

void ResizeTexture(s_texture_t *ptexture)
{
	int i, t;
	byte *pdest;
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
	pdest = (byte *)malloc(ptexture->size);
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
	if (gammaCommand != 1.8)
	// gamma correct the monster textures to a gamma of 1.8
	{
		float g;
		byte *psrc = (byte *)ptexture->ppal;
		g = gammaCommand / 1.8;
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

	sprintf(textureFilePath, "%s/%s", cdPartialPath, ptexture->name);
	ExpandPathAndArchive(textureFilePath);

	if (cdtexturePathCount)
	{
		int time1 = -1;
		for (int i = 0; i < cdtexturePathCount; i++)
		{
			sprintf(textureFilePath, "%s/%s", cdtextureCommand[i], ptexture->name);
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
		sprintf(textureFilePath, "%s/%s", cdCommand, ptexture->name);
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

	for (i = 0; i < texturesCount; i++)
	{
		Grab_Skin(&texture[i]);

		texture[i].max_s = -9999999;
		texture[i].min_s = 9999999;
		texture[i].max_t = -9999999;
		texture[i].min_t = 9999999;
	}

	for (i = 0; i < modelsCount; i++)
	{
		for (j = 0; j < model[i]->nummesh; j++)
		{
			TextureCoordRanges(model[i]->pmesh[j], &texture[model[i]->pmesh[j]->skinref]);
		}
	}

	for (i = 0; i < texturesCount; i++)
	{
		if (texture[i].max_s < texture[i].min_s)
		{
			// must be a replacement texture
			if (texture[i].flags & STUDIO_NF_CHROME)
			{
				texture[i].max_s = 63;
				texture[i].min_s = 0;
				texture[i].max_t = 63;
				texture[i].min_t = 0;
			}
			else
			{
				texture[i].max_s = texture[texture[i].parent].max_s;
				texture[i].min_s = texture[texture[i].parent].min_s;
				texture[i].max_t = texture[texture[i].parent].max_t;
				texture[i].min_t = texture[texture[i].parent].min_t;
			}
		}

		ResizeTexture(&texture[i]);
	}

	for (i = 0; i < modelsCount; i++)
	{
		for (j = 0; j < model[i]->nummesh; j++)
		{
			ResetTextureCoordRanges(model[i]->pmesh[j], &texture[model[i]->pmesh[j]->skinref]);
		}
	}

	// build texture groups
	for (i = 0; i < MAXSTUDIOSKINS; i++)
	{
		for (j = 0; j < MAXSTUDIOSKINS; j++)
		{
			skinref[i][j] = j;
		}
	}
	for (i = 0; i < texturegrouplayers[0]; i++)
	{
		for (j = 0; j < texturegroupreps[0]; j++)
		{
			skinref[i][texturegroupCommand[0][0][j]] = texturegroupCommand[0][i][j];
		}
	}
	if (i != 0)
	{
		skinfamiliesCount = i;
	}
	else
	{
		skinfamiliesCount = 1;
		skinrefCount = texturesCount;
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
			AngleMatrix(boneAngle, boneFixup[i].m);
			AngleIMatrix(boneAngle, boneFixup[i].im);
			boneFixup[i].worldorg = pmodel->skeleton[i].pos;
		}
		else
		{
			vec3_t truePos;
			float rotationMatrix[3][4];
			// calc compound rotational matrices
			// FIXME : Hey, it's orthogical so inv(A) == transpose(A)
			AngleMatrix(boneAngle, rotationMatrix);
			R_ConcatTransforms(boneFixup[parent].m, rotationMatrix, boneFixup[i].m);
			AngleIMatrix(boneAngle, rotationMatrix);
			R_ConcatTransforms(rotationMatrix, boneFixup[parent].im, boneFixup[i].im);

			// calc true world coord.
			VectorTransform(pmodel->skeleton[i].pos, boneFixup[parent].m, truePos);
			boneFixup[i].worldorg = truePos + boneFixup[parent].worldorg;
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
		if (fgets(currentLine, sizeof(currentLine), inputFile) != NULL)
		{
			s_mesh_t *pmesh;
			char triangleMaterial[64];
			s_trianglevert_t *ptriangleVert;
			int parentBone;

			vec3_t triangleVertices[3];
			vec3_t triangleNormals[3];

			lineCount++;

			// check for end
			if (std::strcmp("end\n", currentLine) == 0)
				return;

			// strip off trailing smag
			std::strcpy(triangleMaterial, currentLine);
			for (i = strlen(triangleMaterial) - 1; i >= 0 && !isgraph(triangleMaterial[i]); i--)
				;
			triangleMaterial[i + 1] = '\0';

			// funky texture overrides
			for (i = 0; i < numTextureReplacements; i++)
			{
				if (sourceTexture[i][0] == '\0')
				{
					std::strcpy(triangleMaterial, defaultTextures[i]);
					break;
				}
				if (stricmp(triangleMaterial, sourceTexture[i]) == 0)
				{
					std::strcpy(triangleMaterial, defaultTextures[i]);
					break;
				}
			}

			if (triangleMaterial[0] == '\0')
			{
				// weird model problem, skip them
				fgets(currentLine, sizeof(currentLine), inputFile);
				fgets(currentLine, sizeof(currentLine), inputFile);
				fgets(currentLine, sizeof(currentLine), inputFile);
				lineCount += 3;
				continue;
			}

			pmesh = FindMeshByTexture(pmodel, triangleMaterial);

			for (int j = 0; j < 3; j++)
			{
				if (flagFlipTriangles)
					// quake wants them in the reverse order
					ptriangleVert = FindMeshTriangleByIndex(pmesh, pmesh->numtris) + 2 - j;
				else
					ptriangleVert = FindMeshTriangleByIndex(pmesh, pmesh->numtris) + j;

				if (fgets(currentLine, sizeof(currentLine), inputFile) != NULL)
				{
					s_vertex_t triangleVertex;
					s_normal_t triangleNormal;

					lineCount++;
					if (sscanf(currentLine, "%d %f %f %f %f %f %f %f %f",
							   &parentBone,
							   &triangleVertex.org[0], &triangleVertex.org[1], &triangleVertex.org[2],
							   &triangleNormal.org[0], &triangleNormal.org[1], &triangleNormal.org[2],
							   &ptriangleVert->u, &ptriangleVert->v) == 9)
					{
						if (parentBone < 0 || parentBone >= pmodel->numbones)
						{
							fprintf(stderr, "bogus bone index\n");
							fprintf(stderr, "%d %s :\n%s", lineCount, inputFilename, currentLine);
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
						tmp = triangleVertex.org - boneFixup[triangleVertex.bone].worldorg;
						VectorTransform(tmp, boneFixup[triangleVertex.bone].im, triangleVertex.org);

						// move normal to object space.
						tmp = triangleNormal.org;
						VectorTransform(tmp, boneFixup[triangleVertex.bone].im, triangleNormal.org);
						triangleNormal.org.normalize();

						ptriangleVert->normindex = FindVertexNormalIndex(pmodel, &triangleNormal);
						ptriangleVert->vertindex = FindVertexIndex(pmodel, &triangleVertex);

						// tag bone as being used
						// pmodel->bone[bone].ref = 1;
					}
					else
					{
						Error("%s: error on line %d: %s", inputFilename, lineCount, currentLine);
					}
				}
			}

			if (flagReversedTriangles || flagBadNormals)
			{
				// check triangle direction

				if (triangleNormals[0].dot(triangleNormals[1]) < 0.0 || triangleNormals[1].dot(triangleNormals[2]) < 0.0 || triangleNormals[2].dot(triangleNormals[0]) < 0.0)
				{
					badNormalsCount++;

					if (flagBadNormals)
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
						if (flagReversedTriangles)
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

	while (fgets(currentLine, sizeof(currentLine), inputFile) != NULL)
	{
		lineCount++;
		if (sscanf(currentLine, "%d %f %f %f %f %f %f", &node, &posX, &posY, &posZ, &rotX, &rotY, &rotZ) == 7)
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
		else if (sscanf(currentLine, "%s %d", cmd, &node)) // Delete this
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

	while (fgets(currentLine, sizeof(currentLine), inputFile) != NULL)
	{
		lineCount++;
		if (sscanf(currentLine, "%d \"%[^\"]\" %d", &index, boneName, &parent) == 3)
		{

			strcpyn(pnodes[index].name, boneName);
			pnodes[index].parent = parent;
			numBones = index;
			// check for mirrored bones;
			for (int i = 0; i < mirroredCount; i++)
			{
				if (std::strcmp(boneName, mirrorboneCommand[i]) == 0)
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
	Error("Unexpected EOF at line %d\n", lineCount);
	return 0;
}

void ParseSMD(s_model_t *pmodel)
{
	int time1;
	char cmd[1024];
	int option;

	sprintf(inputFilename, "%s/%s.smd", cdCommand, pmodel->name);
	time1 = FileTime(inputFilename);
	if (time1 == -1)
		Error("%s doesn't exist", inputFilename);

	printf("grabbing %s\n", inputFilename);

	if ((inputFile = fopen(inputFilename, "r")) == 0)
	{
		fprintf(stderr, "reader: could not open file '%s'\n", inputFilename);
	}
	lineCount = 0;

	while (fgets(currentLine, sizeof(currentLine), inputFile) != NULL)
	{
		lineCount++;
		sscanf(currentLine, "%s %d", cmd, &option);
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
	fclose(inputFile);
}

void Cmd_Eyeposition()
{
	// rotate points into frame of reference so model points down the positive x
	// axis
	GetToken(false);
	eyeposition[1] = atof(token);

	GetToken(false);
	eyeposition[0] = -atof(token);

	GetToken(false);
	eyeposition[2] = atof(token);
}

void Cmd_Flags()
{
	GetToken(false);
	gflags = atoi(token);
}

void Cmd_Modelname()
{
	GetToken(false);
	strcpyn(outname, token);
}

void Cmd_Body_OptionStudio()
{
	if (!GetToken(false))
		return;

	model[modelsCount] = (s_model_t *)std::calloc(1, sizeof(s_model_t));
	bodypart[bodygroupCount].pmodel[bodypart[bodygroupCount].nummodels] = model[modelsCount];

	strcpyn(model[modelsCount]->name, token);

	flagFlipTriangles = 1;

	scaleBodyAndSequenceOption = scaleCommand;

	while (TokenAvailable())
	{
		GetToken(false);
		if (stricmp("reverse", token) == 0)
		{
			flagFlipTriangles = 0;
		}
		else if (stricmp("scale", token) == 0)
		{
			GetToken(false);
			scaleBodyAndSequenceOption = atof(token);
		}
	}

	ParseSMD(model[modelsCount]);

	bodypart[bodygroupCount].nummodels++;
	modelsCount++;

	scaleBodyAndSequenceOption = scaleCommand;
}

int Cmd_Body_OptionBlank()
{
	model[modelsCount] = (s_model_t *)(1, sizeof(s_model_t));
	bodypart[bodygroupCount].pmodel[bodypart[bodygroupCount].nummodels] = model[modelsCount];

	strcpyn(model[modelsCount]->name, "blank");

	bodypart[bodygroupCount].nummodels++;
	modelsCount++;
	return 0;
}

void Cmd_Bodygroup()
{
	if (!GetToken(false))
		return;

	if (bodygroupCount == 0)
	{
		bodypart[bodygroupCount].base = 1;
	}
	else
	{
		bodypart[bodygroupCount].base = bodypart[bodygroupCount - 1].base * bodypart[bodygroupCount - 1].nummodels;
	}
	strcpyn(bodypart[bodygroupCount].name, token);

	while (true)
	{
		int is_started = 0;
		GetToken(true);
		if (endofscript)
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
			Cmd_Body_OptionStudio();
		}
		else if (stricmp("blank", token) == 0)
		{
			Cmd_Body_OptionBlank();
		}
	}

	bodygroupCount++;
	return;
}

void Cmd_Body()
{
	if (!GetToken(false))
		return;

	if (bodygroupCount == 0)
	{
		bodypart[bodygroupCount].base = 1;
	}
	else
	{
		bodypart[bodygroupCount].base = bodypart[bodygroupCount - 1].base * bodypart[bodygroupCount - 1].nummodels;
	}
	strcpyn(bodypart[bodygroupCount].name, token);

	Cmd_Body_OptionStudio();

	bodygroupCount++;
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

	cz = cos(rotateCommand);
	sz = sin(rotateCommand);

	while (fgets(currentLine, sizeof(currentLine), inputFile) != NULL)
	{
		lineCount++;
		if (sscanf(currentLine, "%d %f %f %f %f %f %f", &index, &pos[0], &pos[1], &pos[2], &rot[0], &rot[1], &rot[2]) == 7)
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
					rot[2] += rotateCommand;
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
		else if (sscanf(currentLine, "%s %d", cmd, &index))
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
				Error("Error(%d) : %s", lineCount, currentLine);
			}
		}
		else
		{
			Error("Error(%d) : %s", lineCount, currentLine);
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

	sprintf(inputFilename, "%s/%s.smd", cdCommand, panim->name);
	time1 = FileTime(inputFilename);
	if (time1 == -1)
		Error("%s doesn't exist", inputFilename);

	printf("grabbing %s\n", inputFilename);

	if ((inputFile = fopen(inputFilename, "r")) == 0)
	{
		fprintf(stderr, "reader: could not open file '%s'\n", inputFilename);
		Error(0);
	}
	lineCount = 0;

	while (fgets(currentLine, sizeof(currentLine), inputFile) != NULL)
	{
		lineCount++;
		sscanf(currentLine, "%s %d", cmd, &option);
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
			while (fgets(currentLine, sizeof(currentLine), inputFile) != NULL)
			{
				lineCount++;
				if (strncmp(currentLine, "end", 3) == 0)
					break;
			}
		}
	}
	fclose(inputFile);
}

int Cmd_Sequence_OptionDeform(s_sequence_t *psequence) // delete this
{
	return 0;
}

int Cmd_Sequence_OptionEvent(s_sequence_t *psequence)
{
	int event;

	if (psequence->numevents + 1 >= MAXSTUDIOEVENTS)
	{
		printf("too many events\n");
		exit(0);
	}

	GetToken(false);
	event = atoi(token);
	psequence->event[psequence->numevents].event = event;

	GetToken(false);
	psequence->event[psequence->numevents].frame = atoi(token);

	psequence->numevents++;

	// option token
	if (TokenAvailable())
	{
		GetToken(false);
		if (token[0] == '}') // opps, hit the end
			return 1;
		// found an option
		std::strcpy(psequence->event[psequence->numevents - 1].options, token);
	}

	return 0;
}

int Cmd_Sequence_OptionFps(s_sequence_t *psequence)
{
	GetToken(false);

	psequence->fps = atof(token);

	return 0;
}

int Cmd_Sequence_OptionAddPivot(s_sequence_t *psequence)
{
	if (psequence->numpivots + 1 >= MAXSTUDIOPIVOTS)
	{
		printf("too many pivot points\n");
		exit(0);
	}

	GetToken(false);
	psequence->pivot[psequence->numpivots].index = atoi(token);

	GetToken(false);
	psequence->pivot[psequence->numpivots].start = atoi(token);

	GetToken(false);
	psequence->pivot[psequence->numpivots].end = atoi(token);

	psequence->numpivots++;

	return 0;
}

void Cmd_Origin()
{
	GetToken(false);
	originCommand[0] = atof(token);

	GetToken(false);
	originCommand[1] = atof(token);

	GetToken(false);
	originCommand[2] = atof(token);

	if (TokenAvailable())
	{
		GetToken(false);
		originCommandRotation = (atof(token) + 90) * (Q_PI / 180.0);
	}
}

void Cmd_Sequence_OptionOrigin()
{
	GetToken(false);
	sequenceOrigin[0] = atof(token);

	GetToken(false);
	sequenceOrigin[1] = atof(token);

	GetToken(false);
	sequenceOrigin[2] = atof(token);
}

void Cmd_Sequence_OptionRotate()
{
	GetToken(false);
	rotateCommand = (atof(token) + 90) * (Q_PI / 180.0);
}

void Cmd_ScaleUp()
{

	GetToken(false);
	scaleCommand = scaleBodyAndSequenceOption = atof(token);
}

void Cmd_Rotate() // XDM
{
	if (!GetToken(false))
		return;
	rotateCommand = (atof(token) + 90) * (Q_PI / 180.0);
}

void Cmd_Sequence_OptionScaleUp()
{

	GetToken(false);
	scaleBodyAndSequenceOption = atof(token);
}

int Cmd_SequenceGroup()
{
	GetToken(false);
	strcpyn(sequencegroup[sequencegroupCount].label, token);
	sequencegroupCount++;

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

int Cmd_Sequence()
{
	int depth = 0;
	char smdfilename[MAXSTUDIOBLENDS][1024];
	int i;
	int numblends = 0;
	int start = 0;
	int end = MAXSTUDIOANIMATIONS - 1;

	if (!GetToken(false))
		return 0;

	strcpyn(sequence[sequenceCount].name, token);

	sequenceOrigin = originCommand;
	scaleBodyAndSequenceOption = scaleCommand;

	rotateCommand = originCommandRotation;
	sequence[sequenceCount].fps = 30.0;
	sequence[sequenceCount].seqgroup = sequencegroupCount - 1;
	sequence[sequenceCount].blendstart[0] = 0.0;
	sequence[sequenceCount].blendend[0] = 1.0;

	while (true)
	{
		if (depth > 0)
		{
			if (!GetToken(true))
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
				GetToken(false);
			}
		}

		if (endofscript)
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
			Cmd_Sequence_OptionDeform(&sequence[sequenceCount]);
		}
		else if (stricmp("event", token) == 0)
		{
			depth -= Cmd_Sequence_OptionEvent(&sequence[sequenceCount]);
		}
		else if (stricmp("pivot", token) == 0)
		{
			Cmd_Sequence_OptionAddPivot(&sequence[sequenceCount]);
		}
		else if (stricmp("fps", token) == 0)
		{
			Cmd_Sequence_OptionFps(&sequence[sequenceCount]);
		}
		else if (stricmp("origin", token) == 0)
		{
			Cmd_Sequence_OptionOrigin();
		}
		else if (stricmp("rotate", token) == 0)
		{
			Cmd_Sequence_OptionRotate();
		}
		else if (stricmp("scale", token) == 0)
		{
			Cmd_Sequence_OptionScaleUp();
		}
		else if (strnicmp("loop", token, 4) == 0)
		{
			sequence[sequenceCount].flags |= STUDIO_LOOPING;
		}
		else if (strnicmp("frame", token, 5) == 0)
		{
			GetToken(false);
			start = atoi(token);
			GetToken(false);
			end = atoi(token);
		}
		else if (strnicmp("blend", token, 5) == 0)
		{
			GetToken(false);
			sequence[sequenceCount].blendtype[0] = lookupControl(token);
			GetToken(false);
			sequence[sequenceCount].blendstart[0] = atof(token);
			GetToken(false);
			sequence[sequenceCount].blendend[0] = atof(token);
		}
		else if (strnicmp("node", token, 4) == 0)
		{
			GetToken(false);
			sequence[sequenceCount].entrynode = sequence[sequenceCount].exitnode = atoi(token);
		}
		else if (strnicmp("transition", token, 4) == 0)
		{
			GetToken(false);
			sequence[sequenceCount].entrynode = atoi(token);
			GetToken(false);
			sequence[sequenceCount].exitnode = atoi(token);
		}
		else if (strnicmp("rtransition", token, 4) == 0)
		{
			GetToken(false);
			sequence[sequenceCount].entrynode = atoi(token);
			GetToken(false);
			sequence[sequenceCount].exitnode = atoi(token);
			sequence[sequenceCount].nodeflags |= 1;
		}
		else if (lookupControl(token) != -1) // motion flags [motion extraction]
		{
			sequence[sequenceCount].motiontype |= lookupControl(token);
		}
		else if (stricmp("animation", token) == 0)
		{
			GetToken(false);
			strcpyn(smdfilename[numblends++], token);
		}
		else if ((i = Cmd_Sequence_OptionAction(token)) != 0)
		{
			sequence[sequenceCount].activity = i;
			GetToken(false);
			sequence[sequenceCount].actweight = atoi(token);
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
		animationSequenceOption[animationCount] = (s_animation_t *)malloc(sizeof(s_animation_t));
		sequence[sequenceCount].panim[i] = animationSequenceOption[animationCount];
		sequence[sequenceCount].panim[i]->startframe = start;
		sequence[sequenceCount].panim[i]->endframe = end;
		sequence[sequenceCount].panim[i]->flags = 0;
		Cmd_Sequence_OptionAnimation(smdfilename[i], animationSequenceOption[animationCount]);
		animationCount++;
	}
	sequence[sequenceCount].numblends = numblends;

	sequenceCount++;

	return 0;
}

int Cmd_Controller()
{
	if (GetToken(false))
	{
		if (!std::strcmp("mouth", token))
		{
			bonecontroller[bonecontrollersCount].index = 4;
		}
		else
		{
			bonecontroller[bonecontrollersCount].index = atoi(token);
		}
		if (GetToken(false))
		{
			strcpyn(bonecontroller[bonecontrollersCount].name, token);
			GetToken(false);
			if ((bonecontroller[bonecontrollersCount].type = lookupControl(token)) == -1)
			{
				printf("unknown bonecontroller type '%s'\n", token);
				return 0;
			}
			GetToken(false);
			bonecontroller[bonecontrollersCount].start = atof(token);
			GetToken(false);
			bonecontroller[bonecontrollersCount].end = atof(token);

			if (bonecontroller[bonecontrollersCount].type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
			{
				if ((static_cast<int>(bonecontroller[bonecontrollersCount].start + 360) % 360) == (static_cast<int>(bonecontroller[bonecontrollersCount].end + 360) % 360))
				{
					bonecontroller[bonecontrollersCount].type |= STUDIO_RLOOP;
				}
			}
			bonecontrollersCount++;
		}
	}
	return 1;
}

void Cmd_BBox()
{
	GetToken(false);
	bbox[0][0] = atof(token);

	GetToken(false);
	bbox[0][1] = atof(token);

	GetToken(false);
	bbox[0][2] = atof(token);

	GetToken(false);
	bbox[1][0] = atof(token);

	GetToken(false);
	bbox[1][1] = atof(token);

	GetToken(false);
	bbox[1][2] = atof(token);
}

void Cmd_CBox()
{
	GetToken(false);
	cbox[0][0] = atof(token);

	GetToken(false);
	cbox[0][1] = atof(token);

	GetToken(false);
	cbox[0][2] = atof(token);

	GetToken(false);
	cbox[1][0] = atof(token);

	GetToken(false);
	cbox[1][1] = atof(token);

	GetToken(false);
	cbox[1][2] = atof(token);
}

void Cmd_Mirror()
{
	GetToken(false);
	strcpyn(mirrorboneCommand[mirroredCount++], token);
}

void Cmd_Gamma()
{
	GetToken(false);
	gammaCommand = atof(token);
}

int Cmd_TextureGroup()
{
	int i;
	int depth = 0;
	int index = 0;
	int group = 0;

	if (texturesCount == 0)
		Error("texturegroups must follow model loading\n");

	if (!GetToken(false))
		return 0;

	if (skinrefCount == 0)
		skinrefCount = texturesCount;

	while (true)
	{
		if (!GetToken(true))
		{
			break;
		}

		if (endofscript)
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
			texturegroupCommand[texturegroupCount][group][index] = i;
			if (group != 0)
				texture[i].parent = texturegroupCommand[texturegroupCount][0][index];
			index++;
			texturegroupreps[texturegroupCount] = index;
			texturegrouplayers[texturegroupCount] = group + 1;
		}
	}

	texturegroupCount++;

	return 0;
}

int Cmd_Hitgroup()
{
	GetToken(false);
	hitgroupCommand[hitgroupsCount].group = atoi(token);
	GetToken(false);
	strcpyn(hitgroupCommand[hitgroupsCount].name, token);
	hitgroupsCount++;

	return 0;
}

int Cmd_Hitbox()
{
	GetToken(false);
	hitbox[hitboxesCount].group = atoi(token);
	GetToken(false);
	strcpyn(hitbox[hitboxesCount].name, token);
	GetToken(false);
	hitbox[hitboxesCount].bmin[0] = atof(token);
	GetToken(false);
	hitbox[hitboxesCount].bmin[1] = atof(token);
	GetToken(false);
	hitbox[hitboxesCount].bmin[2] = atof(token);
	GetToken(false);
	hitbox[hitboxesCount].bmax[0] = atof(token);
	GetToken(false);
	hitbox[hitboxesCount].bmax[1] = atof(token);
	GetToken(false);
	hitbox[hitboxesCount].bmax[2] = atof(token);

	hitboxesCount++;

	return 0;
}

int Cmd_Attachment()
{
	// index
	GetToken(false);
	attachment[attachmentsCount].index = atoi(token);

	// bone name
	GetToken(false);
	strcpyn(attachment[attachmentsCount].bonename, token);

	// position
	GetToken(false);
	attachment[attachmentsCount].org[0] = atof(token);
	GetToken(false);
	attachment[attachmentsCount].org[1] = atof(token);
	GetToken(false);
	attachment[attachmentsCount].org[2] = atof(token);

	if (TokenAvailable())
		GetToken(false);

	if (TokenAvailable())
		GetToken(false);

	attachmentsCount++;
	return 0;
}

void Cmd_Renamebone()
{
	// from
	GetToken(false);
	std::strcpy(renameboneCommand[renameboneCount].from, token);

	// to
	GetToken(false);
	std::strcpy(renameboneCommand[renameboneCount].to, token);

	renameboneCount++;
}

void Cmd_TexRenderMode()
{
	char tex_name[256];
	GetToken(false);
	std::strcpy(tex_name, token);

	GetToken(false);
	if (!std::strcmp(token, "additive"))
	{
		texture[FindTextureIndex(tex_name)].flags |= STUDIO_NF_ADDITIVE;
	}
	else if (!std::strcmp(token, "masked"))
	{
		texture[FindTextureIndex(tex_name)].flags |= STUDIO_NF_TRANSPARENT;
	}
	else if (!std::strcmp(token, "fullbright"))
	{
		texture[FindTextureIndex(tex_name)].flags |= STUDIO_NF_FULLBRIGHT;
	}
	else if (!std::strcmp(token, "flatshade"))
	{
		texture[FindTextureIndex(tex_name)].flags |= STUDIO_NF_FLATSHADE;
	}
	else
		printf("Texture '%s' has unknown render mode '%s'!\n", tex_name, token);
}

void ParseQCscript()
{
	while (true)
	{
		// Look for a line starting with a $ command
		while (true)
		{
			GetToken(true);
			if (endofscript)
				return;

			if (token[0] == '$')
				break;

			// Skip the rest of the line
			while (TokenAvailable())
				GetToken(false);
		}

		// Process recognized commands
		if (!std::strcmp(token, "$modelname"))
		{
			Cmd_Modelname();
		}
		else if (!std::strcmp(token, "$cd"))
		{
			if (isCdSet)
				Error("Two $cd in one model");
			isCdSet = true;
			GetToken(false);

			std::strcpy(cdPartialPath, token);
			std::strcpy(cdCommand, ExpandPath(token));
		}
		else if (!std::strcmp(token, "$cdtexture"))
		{
			while (TokenAvailable())
			{
				GetToken(false);
				std::strcpy(cdtextureCommand[cdtexturePathCount], ExpandPath(token));
				cdtexturePathCount++;
			}
		}
		else if (!std::strcmp(token, "$scale"))
		{
			Cmd_ScaleUp();
		}
		else if (!stricmp(token, "$rotate")) // XDM
		{
			Cmd_Rotate();
		}
		else if (!std::strcmp(token, "$controller"))
		{
			Cmd_Controller();
		}
		else if (!std::strcmp(token, "$body"))
		{
			Cmd_Body();
		}
		else if (!std::strcmp(token, "$bodygroup"))
		{
			Cmd_Bodygroup();
		}
		else if (!std::strcmp(token, "$sequence"))
		{
			Cmd_Sequence();
		}
		else if (!std::strcmp(token, "$sequencegroup"))
		{
			Cmd_SequenceGroup();
		}
		else if (!std::strcmp(token, "$eyeposition"))
		{
			Cmd_Eyeposition();
		}
		else if (!std::strcmp(token, "$origin"))
		{
			Cmd_Origin();
		}
		else if (!std::strcmp(token, "$bbox"))
		{
			Cmd_BBox();
		}
		else if (!std::strcmp(token, "$cbox"))
		{
			Cmd_CBox();
		}
		else if (!std::strcmp(token, "$mirrorbone"))
		{
			Cmd_Mirror();
		}
		else if (!std::strcmp(token, "$gamma"))
		{
			Cmd_Gamma();
		}
		else if (!std::strcmp(token, "$flags"))
		{
			Cmd_Flags();
		}
		else if (!std::strcmp(token, "$texturegroup"))
		{
			Cmd_TextureGroup();
		}
		else if (!std::strcmp(token, "$hgroup"))
		{
			Cmd_Hitgroup();
		}
		else if (!std::strcmp(token, "$hbox"))
		{
			Cmd_Hitbox();
		}
		else if (!std::strcmp(token, "$attachment"))
		{
			Cmd_Attachment();
		}
		else if (!std::strcmp(token, "$renamebone"))
		{
			Cmd_Renamebone();
		}
		else if (!std::strcmp(token, "$texrendermode"))
		{
			Cmd_TexRenderMode();
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

	scaleCommand = 1.0;
	originCommandRotation = Q_PI / 2;

	numTextureReplacements = 0;
	flagReversedTriangles = 0;
	flagBadNormals = 0;
	flagFlipTriangles = 1;
	flagKeepAllBones = false;

	flagNormalBlendAngle = cos(2.0 * (Q_PI / 180.0));

	gammaCommand = 1.8;

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
				std::strcpy(defaultTextures[numTextureReplacements], argv[i]);
				if (i < argc - 2 && argv[i + 1][0] != '-')
				{
					i++;
					std::strcpy(sourceTexture[numTextureReplacements], argv[i]);
					printf("Replaceing %s with %s\n", sourceTexture[numTextureReplacements], defaultTextures[numTextureReplacements]);
				}
				printf("Using default texture: %s\n", defaultTextures);
				numTextureReplacements++;
				break;
			case 'r':
				flagReversedTriangles = 1;
				break;
			case 'n':
				flagBadNormals = 1;
				break;
			case 'f':
				flagFlipTriangles = 0;
				break;
			case 'a':
				i++;
				flagNormalBlendAngle = cos(atof(argv[i]) * (Q_PI / 180.0));
				break;
			case 'h':
				flagDumpHitboxes = 1;
				break;
			case 'i':
				flagIgnoreWarnings = 1;
				break;
			case 'b':
				flagKeepAllBones = true;
				break;
			}
		}
	}

	std::strcpy(sequencegroup[sequencegroupCount].label, "default");
	sequencegroupCount = 1;
	// load the script
	std::strcpy(path, argv[i]);
	LoadScriptFile(path);
	// parse it
	std::strcpy(outname, argv[i]);
	ParseQCscript();
	SetSkinValues();
	SimplifyModel();
	WriteFile();

	return 0;
}
