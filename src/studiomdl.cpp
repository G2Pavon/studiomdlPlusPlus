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

char filename[1024];
FILE *input;
char line[1024];
int linecount;
bool cdset;
char defaulttexture[16][256];
char sourcetexture[16][256];
int numrep;
s_bonefixup_t bonefixup[MAXSTUDIOSRCBONES];

// studiomdl.exe args -----------
int tag_reversed;
int tag_normals;
int flip_triangles;
float normal_blend;
int dump_hboxes;
int ignore_warnings;
bool keep_all_bones;

// QC Command variables -----------------
char cdpartial[256];						   // $cd
char cddir[256];							   // $cd
int cdtextureset;							   // $cdtexture
char cdtexture[16][256];					   // $cdtexture
float default_scale;						   // $scale
float scale_up;								   // $body studio <scale_up>
vec3_t origin;								   // $origin
float defaultzrotation;						   // $origin <X> <Y> <Z> <defultzrotation>
float zrotation;							   // $sequence <sequence name> <SMD path> {[rotate <zrotation>]}
vec3_t sequenceOrigin;						   // $sequence <sequence name> <SMD path>  {[origin <X> <Y> <Z>]}
float texgamma;								   // $$gamma
s_renamebone_t renamedbone[MAXSTUDIOSRCBONES]; // $renamebone
int renameboneCount;
s_hitgroup_t hitgroup[MAXSTUDIOSRCBONES]; // $hgroup
int hitgroupsCount;
char mirrored[MAXSTUDIOSRCBONES][64]; // $mirrorbone
int mirroredCount;
s_animation_t *panimation[MAXSTUDIOSEQUENCES * MAXSTUDIOBLENDS]; // $sequence, each sequence can have 16 blends
int animationCount;
int texturegroup[32][32][32]; // $texturegroup
int texturegroupCount;		  // unnecessary? since engine doesn't support multiple texturegroups
int texturegrouplayers[32];
int texturegroupreps[32];
// ---------------------------------------

void clip_rotations(vec3_t rot)
{
	int j;
	// clip everything to : -Q_PI <= x < Q_PI

	for (j = 0; j < 3; j++)
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
	int q;

	// extract linear motion
	for (i = 0; i < sequenceCount; i++)
	{
		if (sequence[i].numframes > 1)
		{
			// assume 0 for now.
			int type;
			vec3_t *ppos;
			vec3_t motion = {0, 0, 0};
			type = sequence[i].motiontype;
			ppos = sequence[i].panim[0]->pos[0];

			k = sequence[i].numframes - 1;

			if (type & STUDIO_LX)
				motion[0] = ppos[k][0] - ppos[0][0];
			if (type & STUDIO_LY)
				motion[1] = ppos[k][1] - ppos[0][1];
			if (type & STUDIO_LZ)
				motion[2] = ppos[k][2] - ppos[0][2];

			for (j = 0; j < sequence[i].numframes; j++)
			{
				vec3_t adj;
				for (k = 0; k < sequence[i].panim[0]->numbones; k++)
				{
					if (sequence[i].panim[0]->node[k].parent == -1)
					{
						ppos = sequence[i].panim[0]->pos[k];

						VectorScale(motion, j * 1.0 / (sequence[i].numframes - 1), adj);
						for (q = 0; q < sequence[i].numblends; q++)
						{
							VectorSubstract(sequence[i].panim[q]->pos[k][j], adj, sequence[i].panim[q]->pos[k][j]);
						}
					}
				}
			}

			VectorCopy(motion, sequence[i].linearmovement);
		}
		else
		{
			VectorSubstract(sequence[i].linearmovement, sequence[i].linearmovement, sequence[i].linearmovement);
		}
	}

	// extract unused motion
	for (i = 0; i < sequenceCount; i++)
	{
		int type;
		type = sequence[i].motiontype;
		for (k = 0; k < sequence[i].panim[0]->numbones; k++)
		{
			if (sequence[i].panim[0]->node[k].parent == -1)
			{
				for (q = 0; q < sequence[i].numblends; q++)
				{
					float motion[6];
					motion[0] = sequence[i].panim[q]->pos[k][0][0];
					motion[1] = sequence[i].panim[q]->pos[k][0][1];
					motion[2] = sequence[i].panim[q]->pos[k][0][2];
					motion[3] = sequence[i].panim[q]->rot[k][0][0];
					motion[4] = sequence[i].panim[q]->rot[k][0][1];
					motion[5] = sequence[i].panim[q]->rot[k][0][2];

					for (j = 0; j < sequence[i].numframes; j++)
					{
						/*
						if (type & STUDIO_X)
							sequence[i].panim[q]->pos[k][j][0] = motion[0];
						if (type & STUDIO_Y)
							sequence[i].panim[q]->pos[k][j][1] = motion[1];
						if (type & STUDIO_Z)
							sequence[i].panim[q]->pos[k][j][2] = motion[2];
						*/
						if (type & STUDIO_XR)
							sequence[i].panim[q]->rot[k][j][0] = motion[3];
						if (type & STUDIO_YR)
							sequence[i].panim[q]->rot[k][j][1] = motion[4];
						if (type & STUDIO_ZR)
							sequence[i].panim[q]->rot[k][j][2] = motion[5];
					}
				}
			}
		}
	}

	// extract auto motion
	for (i = 0; i < sequenceCount; i++)
	{
		// assume 0 for now.
		int type;
		vec3_t *ppos;
		vec3_t *prot;
		vec3_t motion = {0, 0, 0};
		vec3_t angles = {0, 0, 0};

		type = sequence[i].motiontype;
		for (j = 0; j < sequence[i].numframes; j++)
		{
			ppos = sequence[i].panim[0]->pos[0];
			prot = sequence[i].panim[0]->rot[0];

			if (type & STUDIO_AX)
				motion[0] = ppos[j][0] - ppos[0][0];
			if (type & STUDIO_AY)
				motion[1] = ppos[j][1] - ppos[0][1];
			if (type & STUDIO_AZ)
				motion[2] = ppos[j][2] - ppos[0][2];
			if (type & STUDIO_AXR)
				angles[0] = prot[j][0] - prot[0][0];
			if (type & STUDIO_AYR)
				angles[1] = prot[j][1] - prot[0][1];
			if (type & STUDIO_AZR)
				angles[2] = prot[j][2] - prot[0][2];

			VectorCopy(motion, sequence[i].automovepos[j]);
			VectorCopy(angles, sequence[i].automoveangle[j]);
		}
	}
}

void OptimizeAnimations()
{
	int i, j;
	int n, m;
	int type;
	int q;
	int iError = 0;

	// optimize animations
	for (i = 0; i < sequenceCount; i++)
	{
		sequence[i].numframes = sequence[i].panim[0]->endframe - sequence[i].panim[0]->startframe + 1;

		// force looping animations to be looping
		if (sequence[i].flags & STUDIO_LOOPING)
		{
			for (j = 0; j < sequence[i].panim[0]->numbones; j++)
			{
				for (q = 0; q < sequence[i].numblends; q++)
				{
					vec3_t *ppos = sequence[i].panim[q]->pos[j];
					vec3_t *prot = sequence[i].panim[q]->rot[j];

					n = 0;						   // sequence[i].panim[q]->startframe;
					m = sequence[i].numframes - 1; // sequence[i].panim[q]->endframe;

					type = sequence[i].motiontype;
					if (!(type & STUDIO_LX))
						ppos[m][0] = ppos[n][0];
					if (!(type & STUDIO_LY))
						ppos[m][1] = ppos[n][1];
					if (!(type & STUDIO_LZ))
						ppos[m][2] = ppos[n][2];

					prot[m][0] = prot[n][0];
					prot[m][1] = prot[n][1];
					prot[m][2] = prot[n][2];
				}
			}
		}

		for (j = 0; j < sequence[i].numevents; j++)
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
	int k;

	for (k = 0; k < bonesCount; k++)
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
	int i, j, k;
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
					for (k = 1; k < numxnodes; k++)
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
			model[i]->boneref[k] = keep_all_bones;
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
				if (!std::strcmp(model[i]->node[j].name, renamedbone[k].from))
				{
					std::strcpy(model[i]->node[j].name, renamedbone[k].to);
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
						VectorCopy(model[i]->skeleton[j].pos, defaultpos[k][n]);
						VectorCopy(model[i]->skeleton[j].rot, defaultrot[k][n]);
					}
					VectorCopy(model[i]->skeleton[j].pos, bonetable[k].pos);
					VectorCopy(model[i]->skeleton[j].rot, bonetable[k].rot);
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

	if (iError && !(ignore_warnings))
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
				if (!std::strcmp(sequence[i].panim[0]->node[j].name, renamedbone[k].from))
				{
					std::strcpy(sequence[i].panim[0]->node[j].name, renamedbone[k].to);
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
	if (iError && !(ignore_warnings))
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
			if (std::strcmp(bonetable[k].name, hitgroup[j].name) == 0)
			{
				bonetable[k].group = hitgroup[j].group;
				break;
			}
		}
		if (k >= bonesCount)
			Error("cannot find bone %s for hitgroup %d\n", hitgroup[j].name, hitgroup[j].group);
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
				VectorCopy(model[i]->vert[j].org, p);
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
				VectorCopy(bonetable[k].bmin, hitbox[hitboxesCount].bmin);
				VectorCopy(bonetable[k].bmax, hitbox[hitboxesCount].bmax);

				if (dump_hboxes)
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

		VectorCopy(bmin, sequence[i].bmin);
		VectorCopy(bmax, sequence[i].bmax);
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
		if (DotProduct(pmodel->normal[i].org, pnormal->org) > normal_blend && pmodel->normal[i].bone == pnormal->bone && pmodel->normal[i].skinref == pnormal->skinref)
		{
			return i;
		}
	}
	if (i >= MAXSTUDIOVERTS)
	{
		Error("too many normals in model: \"%s\"\n", pmodel->name);
	}
	VectorCopy(pnormal->org, pmodel->normal[i].org);
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
		if (VectorCompare(pmodel->vert[i].org, pv->org) && pmodel->vert[i].bone == pv->bone)
		{
			return i;
		}
	}
	if (i >= MAXSTUDIOVERTS)
	{
		Error("too many vertices in model: \"%s\"\n", pmodel->name);
	}
	VectorCopy(pv->org, pmodel->vert[i].org);
	pmodel->vert[i].bone = pv->bone;
	pmodel->numverts = i + 1;
	return i;
}

void AdjustVertexToQcOrigin(float *org)
{
	org[0] = (org[0] - sequenceOrigin[0]);
	org[1] = (org[1] - sequenceOrigin[1]);
	org[2] = (org[2] - sequenceOrigin[2]);
}

void ScaleVertexByQcScale(float *org)
{
	org[0] = org[0] * scale_up;
	org[1] = org[1] * scale_up;
	org[2] = org[2] * scale_up;
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
	int i, j;

	// adjust top, left edge
	for (i = 0; i < pmesh->numtris; i++)
	{
		for (j = 0; j < 3; j++)
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
	int i, j, s, t;
	byte *pdest;
	int srcadjwidth;

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
	srcadjwidth = (ptexture->srcwidth + 3) & ~3;

	// Move the picture data to the model area, replicating missing data, deleting unused data.
	for (i = 0, t = ptexture->srcheight - ptexture->skinheight - ptexture->skintop + 10 * ptexture->srcheight; i < ptexture->skinheight; i++, t++)
	{
		while (t >= ptexture->srcheight)
			t -= ptexture->srcheight;
		while (t < 0)
			t += ptexture->srcheight;
		for (j = 0, s = ptexture->skinleft + 10 * ptexture->srcwidth; j < ptexture->skinwidth; j++, s++)
		{
			while (s >= ptexture->srcwidth)
				s -= ptexture->srcwidth;
			*(pdest++) = *(ptexture->ppicture + s + t * srcadjwidth);
		}
	}

	// TODO: process the texture and flag it if fullbright or transparent are used.
	// TODO: only save as many palette entries as are actually used.
	if (texgamma != 1.8)
	// gamma correct the monster textures to a gamma of 1.8
	{
		float g;
		byte *psrc = (byte *)ptexture->ppal;
		g = texgamma / 1.8;
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
	char file1[1024];

	sprintf(file1, "%s/%s", cdpartial, ptexture->name);
	ExpandPathAndArchive(file1);

	if (cdtextureset)
	{
		int i;
		int time1 = -1;
		for (i = 0; i < cdtextureset; i++)
		{
			sprintf(file1, "%s/%s", cdtexture[i], ptexture->name);
			time1 = FileTime(file1);
			if (time1 != -1)
				break;
		}
		if (time1 == -1)
			Error("%s not found", file1);
	}
	else
	{
		sprintf(file1, "%s/%s", cddir, ptexture->name);
	}

	if (stricmp(".bmp", &file1[strlen(file1) - 4]) == 0)
	{
		Grab_BMP(file1, ptexture);
	}
	else
	{
		Error("unknown graphics type: \"%s\"\n", file1);
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
			skinref[i][texturegroup[0][0][j]] = texturegroup[0][i][j];
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
	int i;
	float angle[3];

	for (i = 0; i < pmodel->numbones; i++)
	{
		int parent;

		// convert to degrees
		angle[0] = pmodel->skeleton[i].rot[0] * (180.0 / Q_PI);
		angle[1] = pmodel->skeleton[i].rot[1] * (180.0 / Q_PI);
		angle[2] = pmodel->skeleton[i].rot[2] * (180.0 / Q_PI);

		parent = pmodel->node[i].parent;
		if (parent == -1)
		{
			// scale the done pos.
			// calc rotational matrices
			AngleMatrix(angle, bonefixup[i].m);
			AngleIMatrix(angle, bonefixup[i].im);
			VectorCopy(pmodel->skeleton[i].pos, bonefixup[i].worldorg);
		}
		else
		{
			vec3_t p;
			float m[3][4];
			// calc compound rotational matrices
			// FIXME : Hey, it's orthogical so inv(A) == transpose(A)
			AngleMatrix(angle, m);
			R_ConcatTransforms(bonefixup[parent].m, m, bonefixup[i].m);
			AngleIMatrix(angle, m);
			R_ConcatTransforms(m, bonefixup[parent].im, bonefixup[i].im);

			// calc true world coord.
			VectorTransform(pmodel->skeleton[i].pos, bonefixup[parent].m, p);
			VectorAdd(p, bonefixup[parent].worldorg, bonefixup[i].worldorg);
		}
	}
}

void Grab_SMDTriangles(s_model_t *pmodel)
{
	int i, j;
	int tcount = 0;
	int ncount = 0;
	vec3_t vmin, vmax;

	vmin[0] = vmin[1] = vmin[2] = 99999;
	vmax[0] = vmax[1] = vmax[2] = -99999;

	Build_Reference(pmodel);

	// load the base triangles
	while (true)
	{
		if (fgets(line, sizeof(line), input) != NULL)
		{
			s_mesh_t *pmesh;
			char texturename[64];
			s_trianglevert_t *ptriv;
			int bone;

			vec3_t vert[3];
			vec3_t norm[3];

			linecount++;

			// check for end
			if (std::strcmp("end\n", line) == 0)
				return;

			// strip off trailing smag
			std::strcpy(texturename, line);
			for (i = strlen(texturename) - 1; i >= 0 && !isgraph(texturename[i]); i--)
				;
			texturename[i + 1] = '\0';

			// funky texture overrides
			for (i = 0; i < numrep; i++)
			{
				if (sourcetexture[i][0] == '\0')
				{
					std::strcpy(texturename, defaulttexture[i]);
					break;
				}
				if (stricmp(texturename, sourcetexture[i]) == 0)
				{
					std::strcpy(texturename, defaulttexture[i]);
					break;
				}
			}

			if (texturename[0] == '\0')
			{
				// weird model problem, skip them
				fgets(line, sizeof(line), input);
				fgets(line, sizeof(line), input);
				fgets(line, sizeof(line), input);
				linecount += 3;
				continue;
			}

			pmesh = FindMeshByTexture(pmodel, texturename);

			for (j = 0; j < 3; j++)
			{
				if (flip_triangles)
					// quake wants them in the reverse order
					ptriv = FindMeshTriangleByIndex(pmesh, pmesh->numtris) + 2 - j;
				else
					ptriv = FindMeshTriangleByIndex(pmesh, pmesh->numtris) + j;

				if (fgets(line, sizeof(line), input) != NULL)
				{
					s_vertex_t p;
					s_normal_t normal;

					linecount++;
					if (sscanf(line, "%d %f %f %f %f %f %f %f %f",
							   &bone,
							   &p.org[0], &p.org[1], &p.org[2],
							   &normal.org[0], &normal.org[1], &normal.org[2],
							   &ptriv->u, &ptriv->v) == 9)
					{
						if (bone < 0 || bone >= pmodel->numbones)
						{
							fprintf(stderr, "bogus bone index\n");
							fprintf(stderr, "%d %s :\n%s", linecount, filename, line);
							exit(1);
						}

						VectorCopy(p.org, vert[j]);
						VectorCopy(normal.org, norm[j]);

						p.bone = bone;
						normal.bone = bone;
						normal.skinref = pmesh->skinref;

						if (p.org[2] < vmin[2])
							vmin[2] = p.org[2];

						AdjustVertexToQcOrigin(p.org);
						ScaleVertexByQcScale(p.org);

						// move vertex position to object space.
						vec3_t tmp;
						VectorSubstract(p.org, bonefixup[p.bone].worldorg, tmp);
						VectorTransform(tmp, bonefixup[p.bone].im, p.org);

						// move normal to object space.
						VectorCopy(normal.org, tmp);
						VectorTransform(tmp, bonefixup[p.bone].im, normal.org);
						VectorNormalize(normal.org);

						ptriv->normindex = FindVertexNormalIndex(pmodel, &normal);
						ptriv->vertindex = FindVertexIndex(pmodel, &p);

						// tag bone as being used
						// pmodel->bone[bone].ref = 1;
					}
					else
					{
						Error("%s: error on line %d: %s", filename, linecount, line);
					}
				}
			}

			if (tag_reversed || tag_normals)
			{
				// check triangle direction

				if (DotProduct(norm[0], norm[1]) < 0.0 || DotProduct(norm[1], norm[2]) < 0.0 || DotProduct(norm[2], norm[0]) < 0.0)
				{

					ncount++;

					if (tag_normals)
					{
						// steal the triangle and make it white
						s_trianglevert_t *ptriv2;
						pmesh = FindMeshByTexture(pmodel, "..\\white.bmp");
						ptriv2 = FindMeshTriangleByIndex(pmesh, pmesh->numtris);

						ptriv2[0] = ptriv[0];
						ptriv2[1] = ptriv[1];
						ptriv2[2] = ptriv[2];
					}
				}
				else
				{
					vec3_t a1, a2, sn;
					float x, y, z;

					VectorSubstract(vert[1], vert[0], a1);
					VectorSubstract(vert[2], vert[0], a2);
					CrossProduct(a1, a2, sn);
					VectorNormalize(sn);

					x = DotProduct(sn, norm[0]);
					y = DotProduct(sn, norm[1]);
					z = DotProduct(sn, norm[2]);
					if (x < 0.0 || y < 0.0 || z < 0.0)
					{
						if (tag_reversed)
						{
							// steal the triangle and make it white
							s_trianglevert_t *ptriv2;

							printf("triangle reversed (%f %f %f)\n",
								   DotProduct(norm[0], norm[1]),
								   DotProduct(norm[1], norm[2]),
								   DotProduct(norm[2], norm[0]));

							pmesh = FindMeshByTexture(pmodel, "..\\white.bmp");
							ptriv2 = FindMeshTriangleByIndex(pmesh, pmesh->numtris);

							ptriv2[0] = ptriv[0];
							ptriv2[1] = ptriv[1];
							ptriv2[2] = ptriv[2];
						}
					}
				}
			}

			pmodel->trimesh[tcount] = pmesh;
			pmodel->trimap[tcount] = pmesh->numtris++;
			tcount++;
		}
		else
		{
			break;
		}
	}

	if (ncount)
		printf("%d triangles with misdirected normals\n", ncount);

	if (vmin[2] != 0.0)
	{
		printf("lowest vector at %f\n", vmin[2]);
	}
}

void Grab_SMDSkeleton(s_node_t *pnodes, s_bone_t *pbones)
{
	float x, y, z, xr, yr, zr;
	char cmd[1024];
	int index;

	while (fgets(line, sizeof(line), input) != NULL)
	{
		linecount++;
		if (sscanf(line, "%d %f %f %f %f %f %f", &index, &x, &y, &z, &xr, &yr, &zr) == 7)
		{
			pbones[index].pos[0] = x;
			pbones[index].pos[1] = y;
			pbones[index].pos[2] = z;

			ScaleVertexByQcScale(pbones[index].pos);

			if (pnodes[index].mirrored)
				VectorScale(pbones[index].pos, -1.0, pbones[index].pos);

			pbones[index].rot[0] = xr;
			pbones[index].rot[1] = yr;
			pbones[index].rot[2] = zr;

			clip_rotations(pbones[index].rot);
		}
		else if (sscanf(line, "%s %d", cmd, &index))
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
	char name[1024];
	int parent;
	int numbones = 0;
	int i;

	while (fgets(line, sizeof(line), input) != NULL)
	{
		linecount++;
		if (sscanf(line, "%d \"%[^\"]\" %d", &index, name, &parent) == 3)
		{

			strcpyn(pnodes[index].name, name);
			pnodes[index].parent = parent;
			numbones = index;
			// check for mirrored bones;
			for (i = 0; i < mirroredCount; i++)
			{
				if (std::strcmp(name, mirrored[i]) == 0)
					pnodes[index].mirrored = 1;
			}
			if ((!pnodes[index].mirrored) && parent != -1)
			{
				pnodes[index].mirrored = pnodes[pnodes[index].parent].mirrored;
			}
		}
		else
		{
			return numbones + 1;
		}
	}
	Error("Unexpected EOF at line %d\n", linecount);
	return 0;
}

void ParseSMD(s_model_t *pmodel)
{
	int time1;
	char cmd[1024];
	int option;

	sprintf(filename, "%s/%s.smd", cddir, pmodel->name);
	time1 = FileTime(filename);
	if (time1 == -1)
		Error("%s doesn't exist", filename);

	printf("grabbing %s\n", filename);

	if ((input = fopen(filename, "r")) == 0)
	{
		fprintf(stderr, "reader: could not open file '%s'\n", filename);
	}
	linecount = 0;

	while (fgets(line, sizeof(line), input) != NULL)
	{
		linecount++;
		sscanf(line, "%s %d", cmd, &option);
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
	fclose(input);
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

	flip_triangles = 1;

	scale_up = default_scale;

	while (TokenAvailable())
	{
		GetToken(false);
		if (stricmp("reverse", token) == 0)
		{
			flip_triangles = 0;
		}
		else if (stricmp("scale", token) == 0)
		{
			GetToken(false);
			scale_up = atof(token);
		}
	}

	ParseSMD(model[modelsCount]);

	bodypart[bodygroupCount].nummodels++;
	modelsCount++;

	scale_up = default_scale;
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

	cz = cos(zrotation);
	sz = sin(zrotation);

	while (fgets(line, sizeof(line), input) != NULL)
	{
		linecount++;
		if (sscanf(line, "%d %f %f %f %f %f %f", &index, &pos[0], &pos[1], &pos[2], &rot[0], &rot[1], &rot[2]) == 7)
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
					rot[2] += zrotation;
				}
				else
				{
					VectorCopy(pos, panim->pos[index][t]);
				}
				if (t > end)
					end = t;
				if (t < start)
					start = t;

				if (panim->node[index].mirrored)
					VectorScale(panim->pos[index][t], -1.0, panim->pos[index][t]);

				ScaleVertexByQcScale(panim->pos[index][t]);

				clip_rotations(rot);

				VectorCopy(rot, panim->rot[index][t]);
			}
		}
		else if (sscanf(line, "%s %d", cmd, &index))
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
				Error("Error(%d) : %s", linecount, line);
			}
		}
		else
		{
			Error("Error(%d) : %s", linecount, line);
		}
	}
	Error("unexpected EOF: %s\n", panim->name);
}

void Shift_OptionAnimation(s_animation_t *panim)
{
	int j;

	int size;

	size = (panim->endframe - panim->startframe + 1) * sizeof(vec3_t);
	// shift
	for (j = 0; j < panim->numbones; j++)
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

	sprintf(filename, "%s/%s.smd", cddir, panim->name);
	time1 = FileTime(filename);
	if (time1 == -1)
		Error("%s doesn't exist", filename);

	printf("grabbing %s\n", filename);

	if ((input = fopen(filename, "r")) == 0)
	{
		fprintf(stderr, "reader: could not open file '%s'\n", filename);
		Error(0);
	}
	linecount = 0;

	while (fgets(line, sizeof(line), input) != NULL)
	{
		linecount++;
		sscanf(line, "%s %d", cmd, &option);
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
			while (fgets(line, sizeof(line), input) != NULL)
			{
				linecount++;
				if (strncmp(line, "end", 3) == 0)
					break;
			}
		}
	}
	fclose(input);
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
	origin[0] = atof(token);

	GetToken(false);
	origin[1] = atof(token);

	GetToken(false);
	origin[2] = atof(token);

	if (TokenAvailable())
	{
		GetToken(false);
		defaultzrotation = (atof(token) + 90) * (Q_PI / 180.0);
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
	zrotation = (atof(token) + 90) * (Q_PI / 180.0);
}

void Cmd_ScaleUp()
{

	GetToken(false);
	default_scale = scale_up = atof(token);
}

void Cmd_Rotate() // XDM
{
	if (!GetToken(false))
		return;
	zrotation = (atof(token) + 90) * (Q_PI / 180.0);
}

void Cmd_Sequence_OptionScaleUp()
{

	GetToken(false);
	scale_up = atof(token);
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
	int i;

	for (i = 0; activity_map[i].name; i++)
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

	VectorCopy(origin, sequenceOrigin);
	scale_up = default_scale;

	zrotation = defaultzrotation;
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
		panimation[animationCount] = (s_animation_t *)malloc(sizeof(s_animation_t));
		sequence[sequenceCount].panim[i] = panimation[animationCount];
		sequence[sequenceCount].panim[i]->startframe = start;
		sequence[sequenceCount].panim[i]->endframe = end;
		sequence[sequenceCount].panim[i]->flags = 0;
		Cmd_Sequence_OptionAnimation(smdfilename[i], panimation[animationCount]);
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
	strcpyn(mirrored[mirroredCount++], token);
}

void Cmd_Gamma()
{
	GetToken(false);
	texgamma = atof(token);
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
			texturegroup[texturegroupCount][group][index] = i;
			if (group != 0)
				texture[i].parent = texturegroup[texturegroupCount][0][index];
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
	hitgroup[hitgroupsCount].group = atoi(token);
	GetToken(false);
	strcpyn(hitgroup[hitgroupsCount].name, token);
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
	std::strcpy(renamedbone[renameboneCount].from, token);

	// to
	GetToken(false);
	std::strcpy(renamedbone[renameboneCount].to, token);

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
			if (cdset)
				Error("Two $cd in one model");
			cdset = true;
			GetToken(false);
			std::strcpy(cdpartial, token);
			std::strcpy(cddir, ExpandPath(token));
		}
		else if (!std::strcmp(token, "$cdtexture"))
		{
			while (TokenAvailable())
			{
				GetToken(false);
				std::strcpy(cdtexture[cdtextureset], ExpandPath(token));
				cdtextureset++;
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

	default_scale = 1.0;
	defaultzrotation = Q_PI / 2;

	numrep = 0;
	tag_reversed = 0;
	tag_normals = 0;
	flip_triangles = 1;
	keep_all_bones = false;

	normal_blend = cos(2.0 * (Q_PI / 180.0));

	texgamma = 1.8;

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
				std::strcpy(defaulttexture[numrep], argv[i]);
				if (i < argc - 2 && argv[i + 1][0] != '-')
				{
					i++;
					std::strcpy(sourcetexture[numrep], argv[i]);
					printf("Replaceing %s with %s\n", sourcetexture[numrep], defaulttexture[numrep]);
				}
				printf("Using default texture: %s\n", defaulttexture);
				numrep++;
				break;
			case 'r':
				tag_reversed = 1;
				break;
			case 'n':
				tag_normals = 1;
				break;
			case 'f':
				flip_triangles = 0;
				break;
			case 'a':
				i++;
				normal_blend = cos(atof(argv[i]) * (Q_PI / 180.0));
				break;
			case 'h':
				dump_hboxes = 1;
				break;
			case 'i':
				ignore_warnings = 1;
				break;
			case 'b':
				keep_all_bones = true;
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
