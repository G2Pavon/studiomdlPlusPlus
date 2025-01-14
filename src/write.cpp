#include <cstring>
#include <cstdlib>

#include "write.h"
#include "mathlib.h"
#include "studio.h"
#include "studiomdl.h"

int g_totalframes = 0;
float g_totalseconds = 0;
constexpr int FILEBUFFER = 16 * 1024 * 1024;
extern int g_numcommandnodes;

std::uint8_t *g_pData;
std::uint8_t *g_pStart;
studiohdr_t *g_phdr;
studioseqhdr_t *g_pseqhdr;

void WriteBoneInfo()
{
	int i, j;
	mstudiobone_t *pbone;
	mstudiobonecontroller_t *pbonecontroller;
	mstudioattachment_t *pattachment;
	mstudiobbox_t *pbbox;

	// save bone info
	pbone = (mstudiobone_t *)g_pData;
	g_phdr->numbones = g_bonescount;
	g_phdr->boneindex = (g_pData - g_pStart);

	for (i = 0; i < g_bonescount; i++)
	{
		std::strcpy(pbone[i].name, g_bonetable[i].name);
		pbone[i].parent = g_bonetable[i].parent;
		pbone[i].flags = g_bonetable[i].flags;
		pbone[i].value[0] = g_bonetable[i].pos[0];
		pbone[i].value[1] = g_bonetable[i].pos[1];
		pbone[i].value[2] = g_bonetable[i].pos[2];
		pbone[i].value[3] = g_bonetable[i].rot[0];
		pbone[i].value[4] = g_bonetable[i].rot[1];
		pbone[i].value[5] = g_bonetable[i].rot[2];
		pbone[i].scale[0] = g_bonetable[i].posscale[0];
		pbone[i].scale[1] = g_bonetable[i].posscale[1];
		pbone[i].scale[2] = g_bonetable[i].posscale[2];
		pbone[i].scale[3] = g_bonetable[i].rotscale[0];
		pbone[i].scale[4] = g_bonetable[i].rotscale[1];
		pbone[i].scale[5] = g_bonetable[i].rotscale[2];
	}
	g_pData += g_bonescount * sizeof(mstudiobone_t);
	g_pData = (std::uint8_t *)ALIGN(g_pData);

	// map bonecontroller to bones
	for (i = 0; i < g_bonescount; i++)
	{
		for (j = 0; j < DEGREESOFFREEDOM; j++)
		{
			pbone[i].bonecontroller[j] = -1;
		}
	}

	for (i = 0; i < g_bonecontrollerscount; i++)
	{
		j = g_bonecontrollerCommand[i].bone;
		switch (g_bonecontrollerCommand[i].type & STUDIO_TYPES)
		{
		case STUDIO_X:
			pbone[j].bonecontroller[0] = i;
			break;
		case STUDIO_Y:
			pbone[j].bonecontroller[1] = i;
			break;
		case STUDIO_Z:
			pbone[j].bonecontroller[2] = i;
			break;
		case STUDIO_XR:
			pbone[j].bonecontroller[3] = i;
			break;
		case STUDIO_YR:
			pbone[j].bonecontroller[4] = i;
			break;
		case STUDIO_ZR:
			pbone[j].bonecontroller[5] = i;
			break;
		default:
			printf("unknown bonecontroller type\n");
			exit(1);
		}
	}

	// save bonecontroller info
	pbonecontroller = (mstudiobonecontroller_t *)g_pData;
	g_phdr->numbonecontrollers = g_bonecontrollerscount;
	g_phdr->bonecontrollerindex = (g_pData - g_pStart);

	for (i = 0; i < g_bonecontrollerscount; i++)
	{
		pbonecontroller[i].bone = g_bonecontrollerCommand[i].bone;
		pbonecontroller[i].index = g_bonecontrollerCommand[i].index;
		pbonecontroller[i].type = g_bonecontrollerCommand[i].type;
		pbonecontroller[i].start = g_bonecontrollerCommand[i].start;
		pbonecontroller[i].end = g_bonecontrollerCommand[i].end;
	}
	g_pData += g_bonecontrollerscount * sizeof(mstudiobonecontroller_t);
	g_pData = (std::uint8_t *)ALIGN(g_pData);

	// save attachment info
	pattachment = (mstudioattachment_t *)g_pData;
	g_phdr->numattachments = g_attachmentscount;
	g_phdr->attachmentindex = (g_pData - g_pStart);

	for (i = 0; i < g_attachmentscount; i++)
	{
		pattachment[i].bone = g_attachmentCommand[i].bone;
		pattachment[i].org = g_attachmentCommand[i].org;
	}
	g_pData += g_attachmentscount * sizeof(mstudioattachment_t);
	g_pData = (std::uint8_t *)ALIGN(g_pData);

	// save bbox info
	pbbox = (mstudiobbox_t *)g_pData;
	g_phdr->numhitboxes = g_hitboxescount;
	g_phdr->hitboxindex = (g_pData - g_pStart);

	for (i = 0; i < g_hitboxescount; i++)
	{
		pbbox[i].bone = g_hitboxCommand[i].bone;
		pbbox[i].group = g_hitboxCommand[i].group;
		pbbox[i].bbmin = g_hitboxCommand[i].bmin;
		pbbox[i].bbmax = g_hitboxCommand[i].bmax;
	}
	g_pData += g_hitboxescount * sizeof(mstudiobbox_t);
	g_pData = (std::uint8_t *)ALIGN(g_pData);
}
void WriteSequenceInfo()
{
	int i, j;

	mstudioseqgroup_t *pseqgroup;
	mstudioseqdesc_t *pseqdesc;
	std::uint8_t *ptransition;

	// save sequence info
	pseqdesc = (mstudioseqdesc_t *)g_pData;
	g_phdr->numseq = g_sequencecount;
	g_phdr->seqindex = (g_pData - g_pStart);
	g_pData += g_sequencecount * sizeof(mstudioseqdesc_t);

	for (i = 0; i < g_sequencecount; i++, pseqdesc++)
	{
		std::strcpy(pseqdesc->label, g_sequenceCommand[i].name);
		pseqdesc->numframes = g_sequenceCommand[i].numframes;
		pseqdesc->fps = g_sequenceCommand[i].fps;
		pseqdesc->flags = g_sequenceCommand[i].flags;

		pseqdesc->numblends = g_sequenceCommand[i].numblends;

		pseqdesc->blendtype[0] = g_sequenceCommand[i].blendtype[0];
		pseqdesc->blendtype[1] = g_sequenceCommand[i].blendtype[1];
		pseqdesc->blendstart[0] = g_sequenceCommand[i].blendstart[0];
		pseqdesc->blendend[0] = g_sequenceCommand[i].blendend[0];
		pseqdesc->blendstart[1] = g_sequenceCommand[i].blendstart[1];
		pseqdesc->blendend[1] = g_sequenceCommand[i].blendend[1];

		pseqdesc->motiontype = g_sequenceCommand[i].motiontype;
		pseqdesc->motionbone = 0; // sequence[i].motionbone;
		pseqdesc->linearmovement = g_sequenceCommand[i].linearmovement;

		pseqdesc->seqgroup = g_sequenceCommand[i].seqgroup;

		pseqdesc->animindex = g_sequenceCommand[i].animindex;

		pseqdesc->activity = g_sequenceCommand[i].activity;
		pseqdesc->actweight = g_sequenceCommand[i].actweight;

		pseqdesc->bbmin = g_sequenceCommand[i].bmin;
		pseqdesc->bbmax = g_sequenceCommand[i].bmax;

		pseqdesc->entrynode = g_sequenceCommand[i].entrynode;
		pseqdesc->exitnode = g_sequenceCommand[i].exitnode;
		pseqdesc->nodeflags = g_sequenceCommand[i].nodeflags;

		g_totalframes += g_sequenceCommand[i].numframes;
		g_totalseconds += g_sequenceCommand[i].numframes / g_sequenceCommand[i].fps;

		// save events
		{
			mstudioevent_t *pevent = (mstudioevent_t *)g_pData; // Declare inside the block
			pseqdesc->numevents = g_sequenceCommand[i].numevents;
			pseqdesc->eventindex = (g_pData - g_pStart);
			g_pData += pseqdesc->numevents * sizeof(mstudioevent_t);
			for (j = 0; j < g_sequenceCommand[i].numevents; j++)
			{
				pevent[j].frame = g_sequenceCommand[i].event[j].frame - g_sequenceCommand[i].frameoffset;
				pevent[j].event = g_sequenceCommand[i].event[j].event;
				memcpy(pevent[j].options, g_sequenceCommand[i].event[j].options, sizeof(pevent[j].options));
			}
			g_pData = (std::uint8_t *)ALIGN(g_pData);
		}

		// save pivots
		{
			mstudiopivot_t *ppivot = (mstudiopivot_t *)g_pData; // Declare inside the block
			pseqdesc->numpivots = g_sequenceCommand[i].numpivots;
			pseqdesc->pivotindex = (g_pData - g_pStart);
			g_pData += pseqdesc->numpivots * sizeof(mstudiopivot_t);
			for (j = 0; j < g_sequenceCommand[i].numpivots; j++)
			{
				ppivot[j].org = g_sequenceCommand[i].pivot[j].org;
				ppivot[j].start = g_sequenceCommand[i].pivot[j].start - g_sequenceCommand[i].frameoffset;
				ppivot[j].end = g_sequenceCommand[i].pivot[j].end - g_sequenceCommand[i].frameoffset;
			}
			g_pData = (std::uint8_t *)ALIGN(g_pData);
		}
	}

	// save sequence group info
	pseqgroup = (mstudioseqgroup_t *)g_pData;
	g_phdr->numseqgroups = g_sequencegroupcount;
	g_phdr->seqgroupindex = (g_pData - g_pStart);
	g_pData += g_phdr->numseqgroups * sizeof(mstudioseqgroup_t);
	g_pData = (std::uint8_t *)ALIGN(g_pData);

	for (i = 0; i < g_sequencegroupcount; i++)
	{
		std::strcpy(pseqgroup[i].label, g_sequencegroupCommand[i].label);
		std::strcpy(pseqgroup[i].name, g_sequencegroupCommand[i].name);
	}

	// save transition graph
	ptransition = (std::uint8_t *)g_pData;
	g_phdr->numtransitions = g_numxnodes;
	g_phdr->transitionindex = (g_pData - g_pStart);
	g_pData += g_numxnodes * g_numxnodes * sizeof(std::uint8_t);
	g_pData = (std::uint8_t *)ALIGN(g_pData);
	for (i = 0; i < g_numxnodes; i++)
	{
		for (j = 0; j < g_numxnodes; j++)
		{
			*ptransition++ = g_xnode[i][j];
		}
	}
}

std::uint8_t *WriteAnimations(std::uint8_t *pData, const std::uint8_t *pStart, int group)
{
	mstudioanim_t *panim;
	mstudioanimvalue_t *panimvalue;

	// hack for seqgroup 0
	// pseqgroup->data = (pData - pStart);

	for (int i = 0; i < g_sequencecount; i++)
	{
		if (g_sequenceCommand[i].seqgroup == group)
		{
			// save animations
			panim = (mstudioanim_t *)pData;
			g_sequenceCommand[i].animindex = (pData - pStart);
			pData += g_sequenceCommand[i].numblends * g_bonescount * sizeof(mstudioanim_t);
			pData = (std::uint8_t *)ALIGN(pData);

			panimvalue = (mstudioanimvalue_t *)pData;
			for (int blends = 0; blends < g_sequenceCommand[i].numblends; blends++)
			{
				// save animation value info
				for (int j = 0; j < g_bonescount; j++)
				{
					for (int k = 0; k < DEGREESOFFREEDOM; k++)
					{
						if (g_sequenceCommand[i].panim[blends]->numanim[j][k] == 0)
						{
							panim->offset[k] = 0;
						}
						else
						{
							panim->offset[k] = ((std::uint8_t *)panimvalue - (std::uint8_t *)panim);
							for (int n = 0; n < g_sequenceCommand[i].panim[blends]->numanim[j][k]; n++)
							{
								panimvalue->value = g_sequenceCommand[i].panim[blends]->anim[j][k][n].value;
								panimvalue++;
							}
						}
					}
					if (((std::uint8_t *)panimvalue - (std::uint8_t *)panim) > 65535)
						Error("sequence \"%s\" is greate than 64K\n", g_sequenceCommand[i].name);
					panim++;
				}
			}

			// printf("raw bone data %d : %s\n", (std::uint8_t *)panimvalue - pData, sequence[i].name);
			pData = (std::uint8_t *)panimvalue;
			pData = (std::uint8_t *)ALIGN(pData);
		}
	}
	return pData;
}

void WriteTextures()
{
	int i;
	mstudiotexture_t *ptexture;
	short *pref;

	// save bone info
	ptexture = (mstudiotexture_t *)g_pData;
	g_phdr->numtextures = g_texturescount;
	g_phdr->textureindex = (g_pData - g_pStart);
	g_pData += g_texturescount * sizeof(mstudiotexture_t);
	g_pData = (std::uint8_t *)ALIGN(g_pData);

	g_phdr->skinindex = (g_pData - g_pStart);
	g_phdr->numskinref = g_skinrefcount;
	g_phdr->numskinfamilies = g_skinfamiliescount;
	pref = (short *)g_pData;

	for (i = 0; i < g_phdr->numskinfamilies; i++)
	{
		for (int j = 0; j < g_phdr->numskinref; j++)
		{
			*pref = g_skinref[i][j];
			pref++;
		}
	}
	g_pData = (std::uint8_t *)pref;
	g_pData = (std::uint8_t *)ALIGN(g_pData);

	g_phdr->texturedataindex = (g_pData - g_pStart); // must be the end of the file!

	for (i = 0; i < g_texturescount; i++)
	{
		std::strcpy(ptexture[i].name, g_texture[i].name);
		ptexture[i].flags = g_texture[i].flags;
		ptexture[i].width = g_texture[i].skinwidth;
		ptexture[i].height = g_texture[i].skinheight;
		ptexture[i].index = (g_pData - g_pStart);
		memcpy(g_pData, g_texture[i].pdata, g_texture[i].size);
		g_pData += g_texture[i].size;
	}
	g_pData = (std::uint8_t *)ALIGN(g_pData);
}

void WriteModel()
{
	int i, j, k;

	mstudiobodyparts_t *pbodypart;
	mstudiomodel_t *pmodel;
	// vec3_t			*bbox;
	std::uint8_t *pbone;
	s_trianglevert_t *psrctri;
	int cur;

	pbodypart = (mstudiobodyparts_t *)g_pData;
	g_phdr->numbodyparts = g_bodygroupcount;
	g_phdr->bodypartindex = (g_pData - g_pStart);
	g_pData += g_bodygroupcount * sizeof(mstudiobodyparts_t);

	pmodel = (mstudiomodel_t *)g_pData;
	g_pData += g_submodelscount * sizeof(mstudiomodel_t);

	for (i = 0, j = 0; i < g_bodygroupcount; i++)
	{
		std::strcpy(pbodypart[i].name, g_bodypart[i].name);
		pbodypart[i].nummodels = g_bodypart[i].nummodels;
		pbodypart[i].base = g_bodypart[i].base;
		pbodypart[i].modelindex = ((std::uint8_t *)&pmodel[j]) - g_pStart;
		j += g_bodypart[i].nummodels;
	}
	g_pData = (std::uint8_t *)ALIGN(g_pData);

	cur = reinterpret_cast<intptr_t>(g_pData);
	for (i = 0; i < g_submodelscount; i++)
	{
		int normmap[MAXSTUDIOVERTS];
		int normimap[MAXSTUDIOVERTS];
		int n = 0;

		std::strcpy(pmodel[i].name, g_submodel[i]->name);

		// save bbox info

		// remap normals to be sorted by skin reference
		for (j = 0; j < g_submodel[i]->nummesh; j++)
		{
			for (k = 0; k < g_submodel[i]->numnorms; k++)
			{
				if (g_submodel[i]->normal[k].skinref == g_submodel[i]->pmesh[j]->skinref)
				{
					normmap[k] = n;
					normimap[n] = k;
					n++;
					g_submodel[i]->pmesh[j]->numnorms++;
				}
			}
		}

		// save vertice bones
		pbone = g_pData;
		pmodel[i].numverts = g_submodel[i]->numverts;
		pmodel[i].vertinfoindex = (g_pData - g_pStart);
		for (j = 0; j < pmodel[i].numverts; j++)
		{
			*pbone++ = g_submodel[i]->vert[j].bone;
		}
		pbone = (std::uint8_t *)ALIGN(pbone);

		// save normal bones
		pmodel[i].numnorms = g_submodel[i]->numnorms;
		pmodel[i].norminfoindex = ((std::uint8_t *)pbone - g_pStart);
		for (j = 0; j < pmodel[i].numnorms; j++)
		{
			*pbone++ = g_submodel[i]->normal[normimap[j]].bone;
		}
		pbone = (std::uint8_t *)ALIGN(pbone);

		g_pData = pbone;

		// save group info
		{
			vec3_t *pvert = (vec3_t *)g_pData;
			g_pData += g_submodel[i]->numverts * sizeof(vec3_t);
			pmodel[i].vertindex = ((std::uint8_t *)pvert - g_pStart);
			g_pData = (std::uint8_t *)ALIGN(g_pData);

			vec3_t *pnorm = (vec3_t *)g_pData;
			g_pData += g_submodel[i]->numnorms * sizeof(vec3_t);
			pmodel[i].normindex = ((std::uint8_t *)pnorm - g_pStart);
			g_pData = (std::uint8_t *)ALIGN(g_pData);

			for (j = 0; j < g_submodel[i]->numverts; j++)
			{
				pvert[j] = g_submodel[i]->vert[j].org;
			}

			for (j = 0; j < g_submodel[i]->numnorms; j++)
			{
				pnorm[j] = g_submodel[i]->normal[normimap[j]].org;
			}
			printf("vertices  %6d bytes (%d vertices, %d normals)\n", g_pData - cur, g_submodel[i]->numverts, g_submodel[i]->numnorms);
			cur = reinterpret_cast<intptr_t>(g_pData);
		}
		// save mesh info
		{

			mstudiomesh_t *pmesh;
			pmesh = (mstudiomesh_t *)g_pData;
			pmodel[i].nummesh = g_submodel[i]->nummesh;
			pmodel[i].meshindex = (g_pData - g_pStart);
			g_pData += pmodel[i].nummesh * sizeof(mstudiomesh_t);
			g_pData = (std::uint8_t *)ALIGN(g_pData);

			int total_tris = 0;
			int total_strips = 0;
			for (j = 0; j < g_submodel[i]->nummesh; j++)
			{
				int numCmdBytes;
				std::uint8_t *pCmdSrc;

				pmesh[j].numtris = g_submodel[i]->pmesh[j]->numtris;
				pmesh[j].skinref = g_submodel[i]->pmesh[j]->skinref;
				pmesh[j].numnorms = g_submodel[i]->pmesh[j]->numnorms;

				psrctri = (s_trianglevert_t *)(g_submodel[i]->pmesh[j]->triangle);
				for (k = 0; k < pmesh[j].numtris * 3; k++)
				{
					psrctri->normindex = normmap[psrctri->normindex];
					psrctri++;
				}

				numCmdBytes = BuildTris(g_submodel[i]->pmesh[j]->triangle, g_submodel[i]->pmesh[j], &pCmdSrc);

				pmesh[j].triindex = (g_pData - g_pStart);
				memcpy(g_pData, pCmdSrc, numCmdBytes);
				g_pData += numCmdBytes;
				g_pData = (std::uint8_t *)ALIGN(g_pData);
				total_tris += pmesh[j].numtris;
				total_strips += g_numcommandnodes;
			}
			printf("mesh      %6d bytes (%d tris, %d strips)\n", g_pData - cur, total_tris, total_strips);
			cur = reinterpret_cast<intptr_t>(g_pData);
		}
	}
}

void WriteFile(void)
{
	FILE *modelouthandle;
	int total = 0;

	g_pStart = (std::uint8_t *)std::calloc(1, FILEBUFFER);

	StripExtension(g_modelnameCommand);

	for (int i = 1; i < g_sequencegroupcount; i++)
	{
		// write the non-default sequence group data to separate files
		char groupname[128], localname[128];

		sprintf(groupname, "%s%02d.mdl", g_modelnameCommand, i);

		printf("writing %s:\n", groupname);
		modelouthandle = SafeOpenWrite(groupname);

		g_pseqhdr = (studioseqhdr_t *)g_pStart;
		g_pseqhdr->id = IDSTUDIOSEQHEADER;
		g_pseqhdr->version = STUDIO_VERSION;

		g_pData = g_pStart + sizeof(studioseqhdr_t);

		g_pData = WriteAnimations(g_pData, g_pStart, i);

		ExtractFileBase(groupname, localname);
		sprintf(g_sequencegroupCommand[i].name, "models\\%s.mdl", localname);
		std::strcpy(g_pseqhdr->name, g_sequencegroupCommand[i].name);
		g_pseqhdr->length = g_pData - g_pStart;

		printf("total     %6d\n", g_pseqhdr->length);

		SafeWrite(modelouthandle, g_pStart, g_pseqhdr->length);

		fclose(modelouthandle);
		std::memset(g_pStart, 0, g_pseqhdr->length);
	}
	//
	// write the model output file
	//
	strcat(g_modelnameCommand, ".mdl");

	printf("---------------------\n");
	printf("writing %s:\n", g_modelnameCommand);
	modelouthandle = SafeOpenWrite(g_modelnameCommand);

	g_phdr = (studiohdr_t *)g_pStart;

	g_phdr->ident = IDSTUDIOHEADER;
	g_phdr->version = STUDIO_VERSION;
	std::strcpy(g_phdr->name, g_modelnameCommand);
	g_phdr->eyeposition = g_eyepositionCommand;
	g_phdr->min = g_bboxCommand[0];
	g_phdr->max = g_bboxCommand[1];
	g_phdr->bbmin = g_cboxCommand[0];
	g_phdr->bbmax = g_cboxCommand[1];

	g_phdr->flags = g_flagsCommand;

	g_pData = (std::uint8_t *)g_phdr + sizeof(studiohdr_t);

	WriteBoneInfo();
	printf("bones     %6d bytes (%d)\n", g_pData - g_pStart - total, g_bonescount);
	total = g_pData - g_pStart;

	g_pData = WriteAnimations(g_pData, g_pStart, 0);

	WriteSequenceInfo();
	printf("sequences %6d bytes (%d frames) [%d:%02d]\n", g_pData - g_pStart - total, g_totalframes, static_cast<int>(g_totalseconds) / 60, static_cast<int>(g_totalseconds) % 60);
	total = g_pData - g_pStart;

	WriteModel();
	printf("models    %6d bytes\n", g_pData - g_pStart - total);
	total = g_pData - g_pStart;

	WriteTextures();
	printf("textures  %6d bytes\n", g_pData - g_pStart - total);

	g_phdr->length = g_pData - g_pStart;

	printf("total     %6d\n", g_phdr->length);

	SafeWrite(modelouthandle, g_pStart, g_phdr->length);

	fclose(modelouthandle);
}
