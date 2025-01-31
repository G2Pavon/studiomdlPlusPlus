#include <cstring>
#include <cstdlib>

#include "writemdl.hpp"
#include "format/mdl.hpp"
#include "studiomdl.hpp"
#include "format/qc.hpp"
#include "utils/mathlib.hpp"
#include "utils/stripification.hpp"

int g_totalframes = 0;
float g_totalseconds = 0;
constexpr int FILEBUFFER = 16 * 1024 * 1024;
extern int g_numcommandnodes;

std::uint8_t *g_currentposition;
std::uint8_t *g_bufferstart;
StudioHeader *g_studioheader;
StudioSequenceGroupHeader *g_studiosequenceheader;

void write_bone_info(QC &qc_cmd)
{
	int i, j;
	StudioBone *pbone;
	StudioBoneController *pbonecontroller;
	StudioAttachment *pattachment;
	StudioHitbox *pbbox;

	// save bone info
	pbone = (StudioBone *)g_currentposition;
	g_studioheader->numbones = g_bonescount;
	g_studioheader->boneindex = static_cast<int>(g_currentposition - g_bufferstart);

	for (i = 0; i < g_bonescount; i++)
	{
		std::strcpy(pbone[i].name, g_bonetable[i].name.c_str());
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
	g_currentposition += g_bonescount * sizeof(StudioBone);
	g_currentposition = (std::uint8_t *)ALIGN(g_currentposition);

	// map bonecontroller to bones
	for (i = 0; i < g_bonescount; i++)
	{
		for (j = 0; j < DEGREESOFFREEDOM; j++)
		{
			pbone[i].bonecontroller[j] = -1;
		}
	}

	for (i = 0; i < qc_cmd.bonecontrollers.size(); i++)
	{
		j = qc_cmd.bonecontrollers[i].bone;
		switch (qc_cmd.bonecontrollers[i].type & STUDIO_TYPES)
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
	pbonecontroller = (StudioBoneController *)g_currentposition;
	g_studioheader->numbonecontrollers = qc_cmd.bonecontrollers.size();
	g_studioheader->bonecontrollerindex = static_cast<int>(g_currentposition - g_bufferstart);

	for (i = 0; i < qc_cmd.bonecontrollers.size(); i++)
	{
		pbonecontroller[i].bone = qc_cmd.bonecontrollers[i].bone;
		pbonecontroller[i].index = qc_cmd.bonecontrollers[i].index;
		pbonecontroller[i].type = qc_cmd.bonecontrollers[i].type;
		pbonecontroller[i].start = qc_cmd.bonecontrollers[i].start;
		pbonecontroller[i].end = qc_cmd.bonecontrollers[i].end;
	}
	g_currentposition += qc_cmd.bonecontrollers.size() * sizeof(StudioBoneController);
	g_currentposition = (std::uint8_t *)ALIGN(g_currentposition);

	// save attachment info
	pattachment = (StudioAttachment *)g_currentposition;
	g_studioheader->numattachments = qc_cmd.attachments.size();
	g_studioheader->attachmentindex = static_cast<int>(g_currentposition - g_bufferstart);

	for (i = 0; i < qc_cmd.attachments.size(); i++)
	{
		pattachment[i].bone = qc_cmd.attachments[i].bone;
		pattachment[i].org = qc_cmd.attachments[i].org;
	}
	g_currentposition += qc_cmd.attachments.size() * sizeof(StudioAttachment);
	g_currentposition = (std::uint8_t *)ALIGN(g_currentposition);

	// save bbox info
	pbbox = (StudioHitbox *)g_currentposition;
	g_studioheader->numhitboxes = qc_cmd.hitboxes.size();
	g_studioheader->hitboxindex = static_cast<int>(g_currentposition - g_bufferstart);

	for (i = 0; i < qc_cmd.hitboxes.size(); i++)
	{
		pbbox[i].bone = qc_cmd.hitboxes[i].bone;
		pbbox[i].group = qc_cmd.hitboxes[i].group;
		pbbox[i].bbmin = qc_cmd.hitboxes[i].bmin;
		pbbox[i].bbmax = qc_cmd.hitboxes[i].bmax;
	}
	g_currentposition += qc_cmd.hitboxes.size() * sizeof(StudioHitbox);
	g_currentposition = (std::uint8_t *)ALIGN(g_currentposition);
}
void write_sequence_info(QC &qc_cmd)
{
	int i, j;

	StudioSequenceDescription *pseqdesc;
	std::uint8_t *ptransition;

	// save sequence info
	pseqdesc = (StudioSequenceDescription *)g_currentposition;
	g_studioheader->numseq = g_num_sequence;
	g_studioheader->seqindex = static_cast<int>(g_currentposition - g_bufferstart);
	g_currentposition += g_num_sequence * sizeof(StudioSequenceDescription);

	for (i = 0; i < g_num_sequence; i++, pseqdesc++)
	{
		std::strcpy(pseqdesc->label, qc_cmd.sequence[i].name.c_str());
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
			StudioAnimationEvent *pevent = (StudioAnimationEvent *)g_currentposition; // Declare inside the block
			pseqdesc->numevents = qc_cmd.sequence[i].numevents;
			pseqdesc->eventindex = static_cast<int>(g_currentposition - g_bufferstart);
			g_currentposition += pseqdesc->numevents * sizeof(StudioAnimationEvent);
			for (j = 0; j < qc_cmd.sequence[i].numevents; j++)
			{
				pevent[j].frame = qc_cmd.sequence[i].event[j].frame - qc_cmd.sequence[i].frameoffset;
				pevent[j].event = qc_cmd.sequence[i].event[j].event;
				memcpy(pevent[j].options, qc_cmd.sequence[i].event[j].options.c_str(), sizeof(pevent[j].options));
			}
			g_currentposition = (std::uint8_t *)ALIGN(g_currentposition);
		}
	}

	// save sequence group info
	StudioSequenceGroup *pseqgroup = (StudioSequenceGroup *)g_currentposition;
	g_studioheader->numseqgroups = 1; // 1 since $sequencegroup is deprecated
	g_studioheader->seqgroupindex = static_cast<int>(g_currentposition - g_bufferstart);
	g_currentposition += g_studioheader->numseqgroups * sizeof(StudioSequenceGroup);
	g_currentposition = (std::uint8_t *)ALIGN(g_currentposition);
	std::strcpy(pseqgroup[0].label, "default");
	std::strcpy(pseqgroup[0].name, "");

	// save transition graph
	ptransition = (std::uint8_t *)g_currentposition;
	g_studioheader->numtransitions = g_numxnodes;
	g_studioheader->transitionindex = static_cast<int>(g_currentposition - g_bufferstart);
	g_currentposition += g_numxnodes * g_numxnodes * sizeof(std::uint8_t);
	g_currentposition = (std::uint8_t *)ALIGN(g_currentposition);
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

	for (int i = 0; i < g_num_sequence; i++)
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
	ptexture = (StudioTexture *)g_currentposition;
	g_studioheader->numtextures = g_texturescount;
	g_studioheader->textureindex = static_cast<int>(g_currentposition - g_bufferstart);
	g_currentposition += g_texturescount * sizeof(StudioTexture);
	g_currentposition = (std::uint8_t *)ALIGN(g_currentposition);

	g_studioheader->skinindex = static_cast<int>(g_currentposition - g_bufferstart);
	g_studioheader->numskinref = g_skinrefcount;
	g_studioheader->numskinfamilies = g_skinfamiliescount;
	pref = (short *)g_currentposition;

	for (i = 0; i < g_studioheader->numskinfamilies; i++)
	{
		for (int j = 0; j < g_studioheader->numskinref; j++)
		{
			*pref = static_cast<short>(g_skinref[i][j]);
			pref++;
		}
	}
	g_currentposition = (std::uint8_t *)pref;
	g_currentposition = (std::uint8_t *)ALIGN(g_currentposition);

	g_studioheader->texturedataindex = static_cast<int>(g_currentposition - g_bufferstart); // must be the end of the file!

	for (i = 0; i < g_texturescount; i++)
	{
		std::strcpy(ptexture[i].name, g_textures[i].name.c_str());
		ptexture[i].flags = g_textures[i].flags;
		ptexture[i].width = g_textures[i].skinwidth;
		ptexture[i].height = g_textures[i].skinheight;
		ptexture[i].index = static_cast<int>(g_currentposition - g_bufferstart);
		memcpy(g_currentposition, g_textures[i].pdata, g_textures[i].size);
		g_currentposition += g_textures[i].size;
	}
	g_currentposition = (std::uint8_t *)ALIGN(g_currentposition);
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

	pbodypart = (StudioBodyPart *)g_currentposition;
	g_studioheader->numbodyparts = g_num_bodygroup;
	g_studioheader->bodypartindex = static_cast<int>(g_currentposition - g_bufferstart);
	g_currentposition += g_num_bodygroup * sizeof(StudioBodyPart);

	pmodel = (StudioModel *)g_currentposition;
	g_currentposition += g_num_submodels * sizeof(StudioModel);

	for (i = 0, j = 0; i < g_num_bodygroup; i++)
	{
		std::strcpy(pbodypart[i].name, qc_cmd.bodypart[i].name.c_str());
		pbodypart[i].nummodels = qc_cmd.bodypart[i].nummodels;
		pbodypart[i].base = qc_cmd.bodypart[i].base;
		pbodypart[i].modelindex = static_cast<int>((std::uint8_t *)&pmodel[j] - g_bufferstart);
		j += qc_cmd.bodypart[i].nummodels;
	}
	g_currentposition = (std::uint8_t *)ALIGN(g_currentposition);

	cur = reinterpret_cast<std::intptr_t>(g_currentposition);
	for (i = 0; i < g_num_submodels; i++)
	{
		int normmap[MAXSTUDIOVERTS];
		int normimap[MAXSTUDIOVERTS];
		int n = 0;

		std::strcpy(pmodel[i].name, qc_cmd.submodel[i]->name.c_str());

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
		pbone = g_currentposition;
		pmodel[i].numverts = qc_cmd.submodel[i]->numverts;
		pmodel[i].vertinfoindex = static_cast<int>(g_currentposition - g_bufferstart);
		for (j = 0; j < pmodel[i].numverts; j++)
		{
			*pbone++ = static_cast<std::uint8_t>(qc_cmd.submodel[i]->vert[j].bone);
		}
		pbone = (std::uint8_t *)ALIGN(pbone);

		// save normal bones
		pmodel[i].numnorms = qc_cmd.submodel[i]->numnorms;
		pmodel[i].norminfoindex = static_cast<int>((std::uint8_t *)pbone - g_bufferstart);
		for (j = 0; j < pmodel[i].numnorms; j++)
		{
			*pbone++ = static_cast<std::uint8_t>(qc_cmd.submodel[i]->normal[normimap[j]].bone);
		}
		pbone = (std::uint8_t *)ALIGN(pbone);

		g_currentposition = pbone;

		// save group info
		{
			Vector3 *pvert = (Vector3 *)g_currentposition;
			g_currentposition += qc_cmd.submodel[i]->numverts * sizeof(Vector3);
			pmodel[i].vertindex = static_cast<int>((std::uint8_t *)pvert - g_bufferstart);
			g_currentposition = (std::uint8_t *)ALIGN(g_currentposition);

			Vector3 *pnorm = (Vector3 *)g_currentposition;
			g_currentposition += qc_cmd.submodel[i]->numnorms * sizeof(Vector3);
			pmodel[i].normindex = static_cast<int>((std::uint8_t *)pnorm - g_bufferstart);
			g_currentposition = (std::uint8_t *)ALIGN(g_currentposition);

			for (j = 0; j < qc_cmd.submodel[i]->numverts; j++)
			{
				pvert[j] = qc_cmd.submodel[i]->vert[j].org;
			}

			for (j = 0; j < qc_cmd.submodel[i]->numnorms; j++)
			{
				pnorm[j] = qc_cmd.submodel[i]->normal[normimap[j]].org;
			}
			printf("vertices  %6d bytes (%d vertices, %d normals)\n", g_currentposition - cur, qc_cmd.submodel[i]->numverts, qc_cmd.submodel[i]->numnorms);
			cur = reinterpret_cast<std::intptr_t>(g_currentposition);
		}
		// save mesh info
		{

			StudioMesh *pmesh;
			pmesh = (StudioMesh *)g_currentposition;
			pmodel[i].nummesh = qc_cmd.submodel[i]->nummesh;
			pmodel[i].meshindex = static_cast<int>(g_currentposition - g_bufferstart);
			g_currentposition += pmodel[i].nummesh * sizeof(StudioMesh);
			g_currentposition = (std::uint8_t *)ALIGN(g_currentposition);

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

				pmesh[j].triindex = static_cast<int>(g_currentposition - g_bufferstart);
				memcpy(g_currentposition, pCmdSrc, numCmdBytes);
				g_currentposition += numCmdBytes;
				g_currentposition = (std::uint8_t *)ALIGN(g_currentposition);
				total_tris += pmesh[j].numtris;
				total_strips += g_numcommandnodes;
			}
			printf("mesh      %6d bytes (%d tris, %d strips)\n", g_currentposition - cur, total_tris, total_strips);
			cur = reinterpret_cast<std::intptr_t>(g_currentposition);
		}
	}
}

void write_file(QC &qc_cmd)
{
	FILE *modelouthandle;
	int total = 0;

	g_bufferstart = (std::uint8_t *)std::calloc(1, FILEBUFFER);

	strip_extension(qc_cmd.modelname);
	//
	// write the model output file
	//
	strcat(qc_cmd.modelname, ".mdl");

	printf("---------------------\n");
	printf("writing %s:\n", qc_cmd.modelname);
	modelouthandle = safe_open_write(qc_cmd.modelname);

	g_studioheader = (StudioHeader *)g_bufferstart;

	g_studioheader->ident = IDSTUDIOHEADER;
	g_studioheader->version = STUDIO_VERSION;
	std::strcpy(g_studioheader->name, qc_cmd.modelname);
	g_studioheader->eyeposition = qc_cmd.eyeposition;
	g_studioheader->min = qc_cmd.bbox[0];
	g_studioheader->max = qc_cmd.bbox[1];
	g_studioheader->bbmin = qc_cmd.cbox[0];
	g_studioheader->bbmax = qc_cmd.cbox[1];

	g_studioheader->flags = qc_cmd.flags;

	g_currentposition = (std::uint8_t *)g_studioheader + sizeof(StudioHeader);

	write_bone_info(qc_cmd);
	printf("bones     %6d bytes (%d)\n", g_currentposition - g_bufferstart - total, g_bonescount);
	total = static_cast<int>(g_currentposition - g_bufferstart);

	g_currentposition = write_animations(qc_cmd, g_currentposition, g_bufferstart, 0);

	write_sequence_info(qc_cmd);
	printf("sequences %6d bytes (%d frames) [%d:%02d]\n", g_currentposition - g_bufferstart - total, g_totalframes, static_cast<int>(g_totalseconds) / 60, static_cast<int>(g_totalseconds) % 60);
	total = static_cast<int>(g_currentposition - g_bufferstart);

	write_model(qc_cmd);
	printf("models    %6d bytes\n", g_currentposition - g_bufferstart - total);
	total = static_cast<int>(g_currentposition - g_bufferstart);

	write_textures();
	printf("textures  %6d bytes\n", g_currentposition - g_bufferstart - total);

	g_studioheader->length = static_cast<int>(g_currentposition - g_bufferstart);

	printf("total     %6d\n", g_studioheader->length);

	safe_write(modelouthandle, g_bufferstart, g_studioheader->length);

	fclose(modelouthandle);
}
