#include <cstring>
#include <cstdlib>

#include "writemdl.hpp"
#include "mathlib.hpp"
#include "studio.hpp"
#include "studiomdl.hpp"
#include "qc.hpp"
#include "tristrip.hpp"

int g_totalframes = 0;
float g_totalseconds = 0;
constexpr int FILEBUFFER = 16 * 1024 * 1024;
extern int g_numcommandnodes;

std::uint8_t *g_pData;
std::uint8_t *g_pStart;
StudioHeader *g_phdr;
StudioSequenceGroupHeader *g_pseqhdr;

void write_bone_info(QC &qc_cmd)
{
	int i, j;
	StudioBone *pbone;
	StudioBoneController *pbonecontroller;
	StudioAttachment *pattachment;
	StudioHitbox *pbbox;

	// save bone info
	pbone = (StudioBone *)g_pData;
	g_phdr->numbones = g_bonescount;
	g_phdr->boneindex = static_cast<int>(g_pData - g_pStart);

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
	g_pData += g_bonescount * sizeof(StudioBone);
	g_pData = (std::uint8_t *)ALIGN(g_pData);

	// map bonecontroller to bones
	for (i = 0; i < g_bonescount; i++)
	{
		for (j = 0; j < DEGREESOFFREEDOM; j++)
		{
			pbone[i].bonecontroller[j] = -1;
		}
	}

	for (i = 0; i < qc_cmd.bonecontrollerscount; i++)
	{
		j = qc_cmd.bonecontroller[i].bone;
		switch (qc_cmd.bonecontroller[i].type & STUDIO_TYPES)
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
	pbonecontroller = (StudioBoneController *)g_pData;
	g_phdr->numbonecontrollers = qc_cmd.bonecontrollerscount;
	g_phdr->bonecontrollerindex = static_cast<int>(g_pData - g_pStart);

	for (i = 0; i < qc_cmd.bonecontrollerscount; i++)
	{
		pbonecontroller[i].bone = qc_cmd.bonecontroller[i].bone;
		pbonecontroller[i].index = qc_cmd.bonecontroller[i].index;
		pbonecontroller[i].type = qc_cmd.bonecontroller[i].type;
		pbonecontroller[i].start = qc_cmd.bonecontroller[i].start;
		pbonecontroller[i].end = qc_cmd.bonecontroller[i].end;
	}
	g_pData += qc_cmd.bonecontrollerscount * sizeof(StudioBoneController);
	g_pData = (std::uint8_t *)ALIGN(g_pData);

	// save attachment info
	pattachment = (StudioAttachment *)g_pData;
	g_phdr->numattachments = qc_cmd.attachmentscount;
	g_phdr->attachmentindex = static_cast<int>(g_pData - g_pStart);

	for (i = 0; i < qc_cmd.attachmentscount; i++)
	{
		pattachment[i].bone = qc_cmd.attachment[i].bone;
		pattachment[i].org = qc_cmd.attachment[i].org;
	}
	g_pData += qc_cmd.attachmentscount * sizeof(StudioAttachment);
	g_pData = (std::uint8_t *)ALIGN(g_pData);

	// save bbox info
	pbbox = (StudioHitbox *)g_pData;
	g_phdr->numhitboxes = qc_cmd.hitboxescount;
	g_phdr->hitboxindex = static_cast<int>(g_pData - g_pStart);

	for (i = 0; i < qc_cmd.hitboxescount; i++)
	{
		pbbox[i].bone = qc_cmd.hitbox[i].bone;
		pbbox[i].group = qc_cmd.hitbox[i].group;
		pbbox[i].bbmin = qc_cmd.hitbox[i].bmin;
		pbbox[i].bbmax = qc_cmd.hitbox[i].bmax;
	}
	g_pData += qc_cmd.hitboxescount * sizeof(StudioHitbox);
	g_pData = (std::uint8_t *)ALIGN(g_pData);
}
void write_sequence_info(QC &qc_cmd)
{
	int i, j;

	StudioSequenceGroup *pseqgroup;
	StudioSequenceDescription *pseqdesc;
	std::uint8_t *ptransition;

	// save sequence info
	pseqdesc = (StudioSequenceDescription *)g_pData;
	g_phdr->numseq = qc_cmd.sequencecount;
	g_phdr->seqindex = static_cast<int>(g_pData - g_pStart);
	g_pData += qc_cmd.sequencecount * sizeof(StudioSequenceDescription);

	for (i = 0; i < qc_cmd.sequencecount; i++, pseqdesc++)
	{
		std::strcpy(pseqdesc->label, qc_cmd.sequence[i].name);
		pseqdesc->numframes = qc_cmd.sequence[i].numframes;
		pseqdesc->fps = qc_cmd.sequence[i].fps;
		pseqdesc->flags = qc_cmd.sequence[i].flags;

		pseqdesc->numblends = qc_cmd.sequence[i].numblends;

		pseqdesc->blendtype[0] = static_cast<int>(qc_cmd.sequence[i].blendtype[0]);
		pseqdesc->blendtype[1] = static_cast<int>(qc_cmd.sequence[i].blendtype[1]);
		pseqdesc->blendstart[0] = qc_cmd.sequence[i].blendstart[0];
		pseqdesc->blendend[0] = qc_cmd.sequence[i].blendend[0];
		pseqdesc->blendstart[1] = qc_cmd.sequence[i].blendstart[1];
		pseqdesc->blendend[1] = qc_cmd.sequence[i].blendend[1];

		pseqdesc->motiontype = qc_cmd.sequence[i].motiontype;
		pseqdesc->motionbone = 0; // sequence[i].motionbone;
		pseqdesc->linearmovement = qc_cmd.sequence[i].linearmovement;

		pseqdesc->seqgroup = qc_cmd.sequence[i].seqgroup;

		pseqdesc->animindex = qc_cmd.sequence[i].animindex;

		pseqdesc->activity = qc_cmd.sequence[i].activity;
		pseqdesc->actweight = qc_cmd.sequence[i].actweight;

		pseqdesc->bbmin = qc_cmd.sequence[i].bmin;
		pseqdesc->bbmax = qc_cmd.sequence[i].bmax;

		pseqdesc->entrynode = qc_cmd.sequence[i].entrynode;
		pseqdesc->exitnode = qc_cmd.sequence[i].exitnode;
		pseqdesc->nodeflags = qc_cmd.sequence[i].nodeflags;

		g_totalframes += qc_cmd.sequence[i].numframes;
		g_totalseconds += static_cast<float>(qc_cmd.sequence[i].numframes) / qc_cmd.sequence[i].fps;

		// save events
		{
			StudioAnimationEvent *pevent = (StudioAnimationEvent *)g_pData; // Declare inside the block
			pseqdesc->numevents = qc_cmd.sequence[i].numevents;
			pseqdesc->eventindex = static_cast<int>(g_pData - g_pStart);
			g_pData += pseqdesc->numevents * sizeof(StudioAnimationEvent);
			for (j = 0; j < qc_cmd.sequence[i].numevents; j++)
			{
				pevent[j].frame = qc_cmd.sequence[i].event[j].frame - qc_cmd.sequence[i].frameoffset;
				pevent[j].event = qc_cmd.sequence[i].event[j].event;
				memcpy(pevent[j].options, qc_cmd.sequence[i].event[j].options, sizeof(pevent[j].options));
			}
			g_pData = (std::uint8_t *)ALIGN(g_pData);
		}

		// save pivots
		{
			StudioPivot *ppivot = (StudioPivot *)g_pData; // Declare inside the block
			pseqdesc->numpivots = qc_cmd.sequence[i].numpivots;
			pseqdesc->pivotindex = static_cast<int>(g_pData - g_pStart);
			g_pData += pseqdesc->numpivots * sizeof(StudioPivot);
			for (j = 0; j < qc_cmd.sequence[i].numpivots; j++)
			{
				ppivot[j].org = qc_cmd.sequence[i].pivot[j].org;
				ppivot[j].start = qc_cmd.sequence[i].pivot[j].start - qc_cmd.sequence[i].frameoffset;
				ppivot[j].end = qc_cmd.sequence[i].pivot[j].end - qc_cmd.sequence[i].frameoffset;
			}
			g_pData = (std::uint8_t *)ALIGN(g_pData);
		}
	}

	// save sequence group info
	pseqgroup = (StudioSequenceGroup *)g_pData;
	g_phdr->numseqgroups = qc_cmd.sequencegroupcount;
	g_phdr->seqgroupindex = static_cast<int>(g_pData - g_pStart);
	g_pData += g_phdr->numseqgroups * sizeof(StudioSequenceGroup);
	g_pData = (std::uint8_t *)ALIGN(g_pData);

	for (i = 0; i < qc_cmd.sequencegroupcount; i++)
	{
		std::strcpy(pseqgroup[i].label, qc_cmd.sequencegroup[i].label);
		std::strcpy(pseqgroup[i].name, qc_cmd.sequencegroup[i].name);
	}

	// save transition graph
	ptransition = (std::uint8_t *)g_pData;
	g_phdr->numtransitions = g_numxnodes;
	g_phdr->transitionindex = static_cast<int>(g_pData - g_pStart);
	g_pData += g_numxnodes * g_numxnodes * sizeof(std::uint8_t);
	g_pData = (std::uint8_t *)ALIGN(g_pData);
	for (i = 0; i < g_numxnodes; i++)
	{
		for (j = 0; j < g_numxnodes; j++)
		{
			*ptransition++ = static_cast<std::uint8_t>(g_xnode[i][j]);
		}
	}
}

std::uint8_t *write_animations(QC &qc_cmd, std::uint8_t *pData, const std::uint8_t *pStart, int group)
{
	StudioAnimationFrameOffset *panim;
	StudioAnimationValue *panimvalue;

	// hack for seqgroup 0
	// pseqgroup->data = (pData - pStart);

	for (int i = 0; i < qc_cmd.sequencecount; i++)
	{
		if (qc_cmd.sequence[i].seqgroup == group)
		{
			// save animations
			panim = (StudioAnimationFrameOffset *)pData;
			qc_cmd.sequence[i].animindex = static_cast<int>(pData - pStart);
			pData += qc_cmd.sequence[i].numblends * g_bonescount * sizeof(StudioAnimationFrameOffset);
			pData = (std::uint8_t *)ALIGN(pData);

			panimvalue = (StudioAnimationValue *)pData;
			for (int blends = 0; blends < qc_cmd.sequence[i].numblends; blends++)
			{
				// save animation value info
				for (int j = 0; j < g_bonescount; j++)
				{
					for (int k = 0; k < DEGREESOFFREEDOM; k++)
					{
						if (qc_cmd.sequence[i].panim[blends]->numanim[j][k] == 0)
						{
							panim->offset[k] = 0;
						}
						else
						{
							panim->offset[k] = static_cast<std::uint16_t>((std::uint8_t *)panimvalue - (std::uint8_t *)panim);
							for (int n = 0; n < qc_cmd.sequence[i].panim[blends]->numanim[j][k]; n++)
							{
								panimvalue->value = qc_cmd.sequence[i].panim[blends]->anim[j][k][n].value;
								panimvalue++;
							}
						}
					}
					if (((std::uint8_t *)panimvalue - (std::uint8_t *)panim) > 65535)
						error("sequence \"%s\" is greate than 64K\n", qc_cmd.sequence[i].name);
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

void write_textures()
{
	int i;
	StudioTexture *ptexture;
	short *pref;

	// save bone info
	ptexture = (StudioTexture *)g_pData;
	g_phdr->numtextures = g_texturescount;
	g_phdr->textureindex = static_cast<int>(g_pData - g_pStart);
	g_pData += g_texturescount * sizeof(StudioTexture);
	g_pData = (std::uint8_t *)ALIGN(g_pData);

	g_phdr->skinindex = static_cast<int>(g_pData - g_pStart);
	g_phdr->numskinref = g_skinrefcount;
	g_phdr->numskinfamilies = g_skinfamiliescount;
	pref = (short *)g_pData;

	for (i = 0; i < g_phdr->numskinfamilies; i++)
	{
		for (int j = 0; j < g_phdr->numskinref; j++)
		{
			*pref = static_cast<short>(g_skinref[i][j]);
			pref++;
		}
	}
	g_pData = (std::uint8_t *)pref;
	g_pData = (std::uint8_t *)ALIGN(g_pData);

	g_phdr->texturedataindex = static_cast<int>(g_pData - g_pStart); // must be the end of the file!

	for (i = 0; i < g_texturescount; i++)
	{
		std::strcpy(ptexture[i].name, g_texture[i].name);
		ptexture[i].flags = g_texture[i].flags;
		ptexture[i].width = g_texture[i].skinwidth;
		ptexture[i].height = g_texture[i].skinheight;
		ptexture[i].index = static_cast<int>(g_pData - g_pStart);
		memcpy(g_pData, g_texture[i].pdata, g_texture[i].size);
		g_pData += g_texture[i].size;
	}
	g_pData = (std::uint8_t *)ALIGN(g_pData);
}

void write_model(QC &qc_cmd)
{
	int i, j, k;

	StudioBodyPart *pbodypart;
	StudioModel *pmodel;
	// vec3_t			*bbox;
	std::uint8_t *pbone;
	TriangleVert *psrctri;
	std::intptr_t cur;

	pbodypart = (StudioBodyPart *)g_pData;
	g_phdr->numbodyparts = qc_cmd.bodygroupcount;
	g_phdr->bodypartindex = static_cast<int>(g_pData - g_pStart);
	g_pData += qc_cmd.bodygroupcount * sizeof(StudioBodyPart);

	pmodel = (StudioModel *)g_pData;
	g_pData += qc_cmd.submodelscount * sizeof(StudioModel);

	for (i = 0, j = 0; i < qc_cmd.bodygroupcount; i++)
	{
		std::strcpy(pbodypart[i].name, qc_cmd.bodypart[i].name);
		pbodypart[i].nummodels = qc_cmd.bodypart[i].nummodels;
		pbodypart[i].base = qc_cmd.bodypart[i].base;
		pbodypart[i].modelindex = static_cast<int>((std::uint8_t *)&pmodel[j] - g_pStart);
		j += qc_cmd.bodypart[i].nummodels;
	}
	g_pData = (std::uint8_t *)ALIGN(g_pData);

	cur = reinterpret_cast<std::intptr_t>(g_pData);
	for (i = 0; i < qc_cmd.submodelscount; i++)
	{
		int normmap[MAXSTUDIOVERTS];
		int normimap[MAXSTUDIOVERTS];
		int n = 0;

		std::strcpy(pmodel[i].name, qc_cmd.submodel[i]->name);

		// save bbox info

		// remap normals to be sorted by skin reference
		for (j = 0; j < qc_cmd.submodel[i]->nummesh; j++)
		{
			for (k = 0; k < qc_cmd.submodel[i]->numnorms; k++)
			{
				if (qc_cmd.submodel[i]->normal[k].skinref == qc_cmd.submodel[i]->pmesh[j]->skinref)
				{
					normmap[k] = n;
					normimap[n] = k;
					n++;
					qc_cmd.submodel[i]->pmesh[j]->numnorms++;
				}
			}
		}

		// save vertice bones
		pbone = g_pData;
		pmodel[i].numverts = qc_cmd.submodel[i]->numverts;
		pmodel[i].vertinfoindex = static_cast<int>(g_pData - g_pStart);
		for (j = 0; j < pmodel[i].numverts; j++)
		{
			*pbone++ = static_cast<std::uint8_t>(qc_cmd.submodel[i]->vert[j].bone);
		}
		pbone = (std::uint8_t *)ALIGN(pbone);

		// save normal bones
		pmodel[i].numnorms = qc_cmd.submodel[i]->numnorms;
		pmodel[i].norminfoindex = static_cast<int>((std::uint8_t *)pbone - g_pStart);
		for (j = 0; j < pmodel[i].numnorms; j++)
		{
			*pbone++ = static_cast<std::uint8_t>(qc_cmd.submodel[i]->normal[normimap[j]].bone);
		}
		pbone = (std::uint8_t *)ALIGN(pbone);

		g_pData = pbone;

		// save group info
		{
			Vector3 *pvert = (Vector3 *)g_pData;
			g_pData += qc_cmd.submodel[i]->numverts * sizeof(Vector3);
			pmodel[i].vertindex = static_cast<int>((std::uint8_t *)pvert - g_pStart);
			g_pData = (std::uint8_t *)ALIGN(g_pData);

			Vector3 *pnorm = (Vector3 *)g_pData;
			g_pData += qc_cmd.submodel[i]->numnorms * sizeof(Vector3);
			pmodel[i].normindex = static_cast<int>((std::uint8_t *)pnorm - g_pStart);
			g_pData = (std::uint8_t *)ALIGN(g_pData);

			for (j = 0; j < qc_cmd.submodel[i]->numverts; j++)
			{
				pvert[j] = qc_cmd.submodel[i]->vert[j].org;
			}

			for (j = 0; j < qc_cmd.submodel[i]->numnorms; j++)
			{
				pnorm[j] = qc_cmd.submodel[i]->normal[normimap[j]].org;
			}
			printf("vertices  %6d bytes (%d vertices, %d normals)\n", g_pData - cur, qc_cmd.submodel[i]->numverts, qc_cmd.submodel[i]->numnorms);
			cur = reinterpret_cast<std::intptr_t>(g_pData);
		}
		// save mesh info
		{

			StudioMesh *pmesh;
			pmesh = (StudioMesh *)g_pData;
			pmodel[i].nummesh = qc_cmd.submodel[i]->nummesh;
			pmodel[i].meshindex = static_cast<int>(g_pData - g_pStart);
			g_pData += pmodel[i].nummesh * sizeof(StudioMesh);
			g_pData = (std::uint8_t *)ALIGN(g_pData);

			int total_tris = 0;
			int total_strips = 0;
			for (j = 0; j < qc_cmd.submodel[i]->nummesh; j++)
			{
				int numCmdBytes;
				std::uint8_t *pCmdSrc;

				pmesh[j].numtris = qc_cmd.submodel[i]->pmesh[j]->numtris;
				pmesh[j].skinref = qc_cmd.submodel[i]->pmesh[j]->skinref;
				pmesh[j].numnorms = qc_cmd.submodel[i]->pmesh[j]->numnorms;

				psrctri = (TriangleVert *)(qc_cmd.submodel[i]->pmesh[j]->triangle);
				for (k = 0; k < pmesh[j].numtris * 3; k++)
				{
					psrctri->normindex = normmap[psrctri->normindex];
					psrctri++;
				}

				numCmdBytes = build_tris(qc_cmd.submodel[i]->pmesh[j]->triangle, qc_cmd.submodel[i]->pmesh[j], &pCmdSrc);

				pmesh[j].triindex = static_cast<int>(g_pData - g_pStart);
				memcpy(g_pData, pCmdSrc, numCmdBytes);
				g_pData += numCmdBytes;
				g_pData = (std::uint8_t *)ALIGN(g_pData);
				total_tris += pmesh[j].numtris;
				total_strips += g_numcommandnodes;
			}
			printf("mesh      %6d bytes (%d tris, %d strips)\n", g_pData - cur, total_tris, total_strips);
			cur = reinterpret_cast<std::intptr_t>(g_pData);
		}
	}
}

void write_file(QC &qc_cmd)
{
	FILE *modelouthandle;
	int total = 0;

	g_pStart = (std::uint8_t *)std::calloc(1, FILEBUFFER);

	strip_extension(qc_cmd.modelname);

	for (int i = 1; i < qc_cmd.sequencegroupcount; i++)
	{
		// write the non-default sequence group data to separate files
		char groupname[128], localname[128];

		sprintf(groupname, "%s%02d.mdl", qc_cmd.modelname, i);

		printf("writing %s:\n", groupname);
		modelouthandle = safe_open_write(groupname);

		g_pseqhdr = (StudioSequenceGroupHeader *)g_pStart;
		g_pseqhdr->id = IDSTUDIOSEQHEADER;
		g_pseqhdr->version = STUDIO_VERSION;

		g_pData = g_pStart + sizeof(StudioSequenceGroupHeader);

		g_pData = write_animations(qc_cmd, g_pData, g_pStart, i);

		extract_filebase(groupname, localname);
		sprintf(qc_cmd.sequencegroup[i].name, "models\\%s.mdl", localname);
		std::strcpy(g_pseqhdr->name, qc_cmd.sequencegroup[i].name);
		g_pseqhdr->length = static_cast<int>(g_pData - g_pStart);

		printf("total     %6d\n", g_pseqhdr->length);

		safe_write(modelouthandle, g_pStart, g_pseqhdr->length);

		fclose(modelouthandle);
		std::memset(g_pStart, 0, g_pseqhdr->length);
	}
	//
	// write the model output file
	//
	strcat(qc_cmd.modelname, ".mdl");

	printf("---------------------\n");
	printf("writing %s:\n", qc_cmd.modelname);
	modelouthandle = safe_open_write(qc_cmd.modelname);

	g_phdr = (StudioHeader *)g_pStart;

	g_phdr->ident = IDSTUDIOHEADER;
	g_phdr->version = STUDIO_VERSION;
	std::strcpy(g_phdr->name, qc_cmd.modelname);
	g_phdr->eyeposition = qc_cmd.eyeposition;
	g_phdr->min = qc_cmd.bbox[0];
	g_phdr->max = qc_cmd.bbox[1];
	g_phdr->bbmin = qc_cmd.cbox[0];
	g_phdr->bbmax = qc_cmd.cbox[1];

	g_phdr->flags = qc_cmd.flags;

	g_pData = (std::uint8_t *)g_phdr + sizeof(StudioHeader);

	write_bone_info(qc_cmd);
	printf("bones     %6d bytes (%d)\n", g_pData - g_pStart - total, g_bonescount);
	total = static_cast<int>(g_pData - g_pStart);

	g_pData = write_animations(qc_cmd, g_pData, g_pStart, 0);

	write_sequence_info(qc_cmd);
	printf("sequences %6d bytes (%d frames) [%d:%02d]\n", g_pData - g_pStart - total, g_totalframes, static_cast<int>(g_totalseconds) / 60, static_cast<int>(g_totalseconds) % 60);
	total = static_cast<int>(g_pData - g_pStart);

	write_model(qc_cmd);
	printf("models    %6d bytes\n", g_pData - g_pStart - total);
	total = static_cast<int>(g_pData - g_pStart);

	write_textures();
	printf("textures  %6d bytes\n", g_pData - g_pStart - total);

	g_phdr->length = static_cast<int>(g_pData - g_pStart);

	printf("total     %6d\n", g_phdr->length);

	safe_write(modelouthandle, g_pStart, g_phdr->length);

	fclose(modelouthandle);
}
