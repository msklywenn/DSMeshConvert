#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include <set>

#include <assimp.hpp>
#include <aiPostProcess.h>
#include <aiConfig.h>
#include <aiMesh.h>
#include <aiScene.h>

#include <aiVector3D.inl>

//#define NVTRISTRIP // buggy?!!
//#define CETS_PTERDIMAN // http://www.codercorner.com/Strips.htm // memory corruption...
//#define ACTC // http://plunk.org/~grantham/public/actc/
#define MULTIPATH // "Multi-Path Algorithm for Triangle Strips" implementation
#include "NvTriStrip.h"
#include "cets-pterdiman/Striper.h"
#include "ac/tc.h"

#include "stripping.h"

#include "types.h"
#include <assert.h>

#include "List.h"

#include "opengl.h"

struct Box
{
	aiVector3D min, max;
};

Box ComputeBoundingBox(aiVector3D* vertices, u32 nbVertices)
{
	Box aabb = { aiVector3D(100000000.0f), aiVector3D(-100000000.0f) };

	for ( u32 i = nbVertices ; i > 0 ; i-- )
	{
		if ( vertices->x < aabb.min.x ) aabb.min.x = vertices->x;
		if ( vertices->y < aabb.min.y ) aabb.min.y = vertices->y;
		if ( vertices->z < aabb.min.z ) aabb.min.z = vertices->z;
		if ( vertices->x > aabb.max.x ) aabb.max.x = vertices->x;
		if ( vertices->y > aabb.max.y ) aabb.max.y = vertices->y;
		if ( vertices->z > aabb.max.z ) aabb.max.z = vertices->z;
		vertices++;
	}

	return aabb;
}

aiMatrix4x4 operator+(aiMatrix4x4& a, aiMatrix4x4& b)
{
	return aiMatrix4x4(
		a.a1 + b.a1, a.a2 + b.a2, a.a3 + b.a3, a.a4 + b.a4,
		a.b1 + b.b1, a.b2 + b.b2, a.b3 + b.b3, a.b4 + b.b4,
		a.c1 + b.c1, a.c2 + b.c2, a.c3 + b.c3, a.c4 + b.c4,
		a.d1 + b.d1, a.d2 + b.d2, a.d3 + b.d3, a.d4 + b.d4);
}

template <typename t>
t map(t x, t a, t b, t c, t d)
{
	t s = b - a;
	t t = (b * c - a * d) / s;
	s = (d - c) / s;
	return x * s + t;
}

void PushValue(std::vector<u32>& list, u32& cmdOffset, u32& cmdIndex, u32 cmd, u32 value)
{
	list[cmdOffset] |= cmd << (cmdIndex * 8); // color
	cmdIndex = (cmdIndex + 1) % 4;
	list.push_back(value);
	if ( cmdIndex == 0 )
	{
		list.push_back(0);
		cmdOffset = list.size() - 1;
	}
}

void MultiPathStripper(const std::vector<u32>& indices, std::vector<u32>& stripLengths, std::vector<u32>& stripIndices);

int Convert(const char* input, const char* output)
{
#ifdef NVTRISTRIP
	// Configure NVTriStrip
	SetStitchStrips(false);
	SetCacheSize(64); // ds has no cache, give me longest strips possible ffs !
#endif

	// Configure Assimp
	Assimp::Importer importer;
	importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
		aiPrimitiveType_POINT
		| aiPrimitiveType_LINE);
	importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
		aiComponent_TANGENTS_AND_BITANGENTS
		//| aiComponent_COLORS
		| aiComponent_COLORSn(1)
		| aiComponent_COLORSn(2)
		| aiComponent_COLORSn(3)
		| aiComponent_TEXCOORDSn(1)
		| aiComponent_TEXCOORDSn(2)
		| aiComponent_TEXCOORDSn(3)
		| aiComponent_BONEWEIGHTS
		| aiComponent_ANIMATIONS
		| aiComponent_TEXTURES
		| aiComponent_LIGHTS
		| aiComponent_CAMERAS
		| aiComponent_MATERIALS);

	// Import file
	const aiScene* scene = importer.ReadFile(input,
		aiProcess_FixInfacingNormals
		| aiProcess_GenUVCoords
		| aiProcess_TransformUVCoords
		| aiProcess_JoinIdenticalVertices
		| aiProcess_Triangulate
		| aiProcess_PreTransformVertices
		| aiProcess_FindDegenerates
		| aiProcess_SortByPType
		| aiProcess_FindInstances 
		| aiProcess_OptimizeMeshes 
		| aiProcess_ImproveCacheLocality
		| aiProcess_RemoveComponent);

	if ( scene == 0 )
	{
		fprintf(stderr, "Could not import %s\n", input);
		return 1;
	}

	// TODO: collapse all meshes into one, with warning output
	if ( scene->mNumMeshes > 1 )
	{
		fprintf(stderr, "%s has multiple meshes, not exporting\n", input);
		return 2;
	}

	aiMesh* mesh = scene->mMeshes[0];

	if ( mesh->mFaces->mNumIndices > 65535 )
	{
		fprintf(stderr, "Model is too complex, not exporting\n");
		return 3;
	}

	InitOpenGL();

	//std::vector<u32> indices;
	//for ( u32 i = 0 ; i < mesh->mNumFaces ; i++ )
	//{
	//	ai_assert(mesh->mFaces[i].mNumIndices == 3);
	//	indices.push_back(mesh->mFaces[i].mIndices[0]);
	//	indices.push_back(mesh->mFaces[i].mIndices[1]);
	//	indices.push_back(mesh->mFaces[i].mIndices[2]);
	//}

	//BuildDualGraph(indices);

	// Generate triangle strips
#ifdef NVTRISTRIP
	u32 nbIndices = mesh->mNumFaces * 3;
	u16* indices = new u16[nbIndices];
	u16* pIndices = indices;
	for ( u32 i = 0 ; i < nbIndices / 3 ; i++ )
	{
		ai_assert(mesh->mFaces[i].mNumIndices == 3);
		*pIndices++ = mesh->mFaces[i].mIndices[0];
		*pIndices++ = mesh->mFaces[i].mIndices[1];
		*pIndices++ = mesh->mFaces[i].mIndices[2];
	}
	u16 nbStrips = 0;
	PrimitiveGroup* strips = 0;
	if ( ! GenerateStrips(indices, nbIndices, &strips, &nbStrips) )
	{
		fprintf(stderr, "Couldn't generate triangle strips, aborting\n");
		delete[] indices;
		return 4;
	}
#endif
#ifdef CETS_PTERDIMAN
	u32 nbIndices = mesh->mNumFaces * 3;
	u32* indices = new u32[nbIndices];
	u32* pIndices = indices;
	for ( u32 i = 0 ; i < nbIndices / 3 ; i++ )
	{
		*pIndices++ = mesh->mFaces[i].mIndices[0];
		*pIndices++ = mesh->mFaces[i].mIndices[1];
		*pIndices++ = mesh->mFaces[i].mIndices[2];
	}
	STRIPERCREATE sc;
	sc.DFaces			= indices;
	sc.NbFaces			= mesh->mNumFaces;
	sc.AskForWords		= true;
	sc.ConnectAllStrips	= false;
	sc.OneSided			= false;
	sc.SGIAlgorithm		= false;

	Striper Strip;
	Strip.Init(sc);

	STRIPERRESULT sr;
	Strip.Compute(sr);
	u32 nbStrips = sr.NbStrips;
#endif
#ifdef ACTC
	ACTCData *tc;
	tc = actcNew();
	actcParami(tc, ACTC_OUT_MIN_FAN_VERTS, INT_MAX);
	actcBeginInput(tc);
	for ( u32 i = 0 ; i < mesh->mNumFaces ; i++ )
		actcAddTriangle(tc,
			mesh->mFaces[i].mIndices[0], 
			mesh->mFaces[i].mIndices[1], 
			mesh->mFaces[i].mIndices[2]);
	actcEndInput(tc);
	actcBeginOutput(tc);
	u32 prim;
	u32 v1, v2, v3;
	u32 nbStrips = 0;
	std::vector<u32> stripLengths;
	std::vector<u16> stripIndices;
    while ( (prim = actcStartNextPrim(tc, &v1, &v2)) != ACTC_DATABASE_EMPTY )
	{
		nbStrips++;
		stripIndices.push_back(v1);
		stripIndices.push_back(v2);
		u32 len = 2;
		while ( actcGetNextVert(tc, &v3) != ACTC_PRIM_COMPLETE )
		{
			len++;
			stripIndices.push_back(v3);
		}
		stripLengths.push_back(len);
    }
	actcEndOutput(tc);
#endif
#ifdef MULTIPATH
	std::vector<u32> indices;
	u32 nbIndices = mesh->mNumFaces * 3;
	indices.reserve(nbIndices);
	for ( u32 i = 0 ; i < nbIndices / 3 ; i++ )
	{
		indices.push_back(mesh->mFaces[i].mIndices[0]);
		indices.push_back(mesh->mFaces[i].mIndices[1]);
		indices.push_back(mesh->mFaces[i].mIndices[2]);
	}
	u32 nbStrips = 0;
	std::vector<u32> stripLengths;
	std::vector<u32> stripIndices;
	extern float* vertices;
	vertices = new float[mesh->mNumVertices * 3];
	for ( u32 i = 0 ; i < mesh->mNumVertices ; i++ )
	{
		vertices[i * 3 + 0] = mesh->mVertices[i].x;
		vertices[i * 3 + 1] = mesh->mVertices[i].y;
		vertices[i * 3 + 2] = mesh->mVertices[i].z;
	}
	MultiPathStripper(indices, stripLengths, stripIndices);
#endif
	printf("%d strips generated for %d triangles\n", nbStrips, mesh->mNumFaces);

	// TODO: AABB => OBB, for higher precision
	Box box = ComputeBoundingBox(mesh->mVertices, mesh->mNumVertices);
	aiVector3D minDS(-7.99f);
	aiVector3D maxDS(7.99f);
	aiVector3D scale = box.max - box.min;
	aiVector3D translate = aiVector3D(
		box.max.x * minDS.x - box.min.x * maxDS.x,
		box.max.y * minDS.y - box.min.y * maxDS.y,
		box.max.z * minDS.z - box.min.z * maxDS.z);
	translate = translate / scale;
	scale = (maxDS - minDS) / scale;

	// Generate display list
	std::vector<u32> list;

	aiMatrix4x4 transform;
	aiMatrix4x4::Scaling(1.0f / scale, transform);
	aiMatrix4x4 tmp;
	aiMatrix4x4::Translation(-translate, tmp);
	tmp.a1 = 0; tmp.b2 = 0; tmp.c3 = 0;
	transform = transform + tmp;
	list.push_back(0x19); // mult matrix 4x3 command
	float* mtx = transform[0];
	for ( u32 i = 0 ; i < 4 ; i++ )
	{
		for ( u32 j = 0 ; j < 3 ; j++ )
		{
			list.push_back(s32(mtx[i + j * 4] * float(1 << 12)));
		}
	}

	u32 cmdindex = 1;
	u32 command = 0;

	u16* idx = 0;
#ifdef CETS_PTERDIMAN
	idx = (u16*)sr.StripRuns;
#endif
#ifdef ACTC
	idx = &stripIndices[0];
#endif
	for ( u32 i = 0 ; i < nbStrips ; i++ )
	{
#ifdef NVTRISTRIP
		idx = strips[i].indices;
		u32 idxLen = strips[i].numIndices;
		if ( strips[i].type == PT_STRIP )
		{
			PushValue(list, command, cmdindex, 0x40, 2); // begin triangle strip
			printf("begin strip\n");
		}
		else if ( strips[i].type == PT_LIST )
		{
			PushValue(list, command, cmdindex, 0x40, 0); // begin triangle list
			//printf("begin list\n");
		}
		else // if ( strips[i]->type == PT_FAN )
		{
			fprintf(stderr, "Export failed, fan list generated\n");
			return 42;
		}
#endif
#ifdef CETS_PTERDIMAN
		PushValue(list, command, cmdindex, 0x40, 2); // begin triangle strip
		//printf("begin strip\n");
		u32 idxLen = sr.StripLengths[i];
#endif
#ifdef ACTC
		u32 idxLen = stripLengths[i];
		PushValue(list, command, cmdindex, 0x40, 2); // begin triangle strip
		//printf("begin strip\n");
#endif
#ifdef MULTIPATH
		u32 idxLen = stripLengths[i];
		PushValue(list, command, cmdindex, 0x40, 2); // start strip
#endif

		for ( u32 j = idxLen ; j > 0 ; j--, idx++ )
		{
			if ( mesh->HasTextureCoords(0) )
			{
				aiVector3D t = mesh->mTextureCoords[0][*idx];
				//printf("texcoord %f %f\n", t.x, t.y);
				t *= 1024.0f * float(1 << 4);//float(1 << 15);
				PushValue(list, command, cmdindex, 0x22, (s32(t.x) & 0xFFFF)  | ((s32(t.y) & 0xFFFF) << 16));
			}

			if ( mesh->HasNormals() )
			{
				// remove this ?
				if ( mesh->HasVertexColors(0) )
				{
					u32 ar = 0; u32 ag = 0; u32 ab = 0; // ambiant color, TODO: add command line parameter to set it
					s32 r = (s32)(mesh->mColors[*idx][0].r * 31); if ( r < 0 ) r = 0; if ( r > 31 ) r = 31;
					s32 g = (s32)(mesh->mColors[*idx][0].g * 31); if ( g < 0 ) g = 0; if ( g > 31 ) g = 31;
					s32 b = (s32)(mesh->mColors[*idx][0].b * 31); if ( b < 0 ) b = 0; if ( b > 31 ) b = 31;
					PushValue(list, command, cmdindex, 0x30, r | (g << 5) | (b << 10) | (ar << 16) | (ag << 21) | (ab << 26)); // material diffuse + ambiant
				}

				aiVector3D n = mesh->mNormals[*idx];
				n.Normalize();
				//printf("normal %f %f %f\n", n.x, n.y, n.z);
				n *= float(1 << 9);
				PushValue(list, command, cmdindex, 0x21, (s32(n.x) & 0x3FF | ((s32(n.y) & 0x3FF) << 10) | ((s32(n.z) & 0x3FF) << 20)));
			}
			else if ( mesh->HasVertexColors(0) )
			{
				s32 r = (s32)(mesh->mColors[*idx][0].r * 31); if ( r < 0 ) r = 0; if ( r > 31 ) r = 31;
				s32 g = (s32)(mesh->mColors[*idx][0].g * 31); if ( g < 0 ) g = 0; if ( g > 31 ) g = 31;
				s32 b = (s32)(mesh->mColors[*idx][0].b * 31); if ( b < 0 ) b = 0; if ( b > 31 ) b = 31;
				PushValue(list, command, cmdindex, 0x20, r | (g << 5) | (b << 10) | (1 << 15)); // color
			}

			aiVector3D p = mesh->mVertices[*idx];
			//printf("vtx10 %f %f %f\n", p.x, p.y, p.z);
			p.x *= scale.x; p.y *= scale.y; p.z *= scale.z;
			p += translate;
			p *= float(1 << 6);
			s32 px = (s32)p.x;
			s32 py = (s32)p.y;
			s32 pz = (s32)p.z;
			PushValue(list, command, cmdindex, 0x24, ((px) & 0x3FF) | (((py) & 0x3FF) << 10) | (((pz) & 0x3FF) << 20));
		}
	}

	// Output file
	FILE* f = fopen(output, "wb");
	fwrite(&list[0], sizeof(list[0]), list.size(), f);
	fclose(f);

#ifdef NVTRISTRIP
	delete[] strips;
	delete[] indices;
#endif
#ifdef CETS_PTERDIMAN
	delete[] indices;
#endif

	return 0;
}

int main(int argc, char** argv)
{
	if ( argc < 3 )
	{
		fprintf(stderr, "Usage: %s <input> <output>\n", argv[0]);
		return 42;
	}

	return Convert(argv[1], argv[2]);
}