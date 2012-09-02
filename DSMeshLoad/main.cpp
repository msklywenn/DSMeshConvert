#include <stdio.h>
#include <string>
#define AI_WONT_RETURN
#include "../DSMeshConvert/assimp--1.1.700-sdk/include/aiVector3D.h"
#include "../DSMeshConvert/assimp--1.1.700-sdk/include/aiMatrix4x4.h"
#include "../DSMeshConvert/assimp--1.1.700-sdk/include/aiMatrix4x4.inl"

typedef unsigned int u32;
typedef signed int s32;

int main(int argc, char** argv)
{
	if ( argc != 2 )
	{
		fprintf(stderr, "Usage: %s <file.msh>\n", argv[0]);
		return 42;
	}

	FILE* f = fopen(argv[1], "rb");
	if ( ! f )
	{
		fprintf(stderr, "Could not open %s\n", argv[1]);
		return 1;
	}

	fseek(f, 0, SEEK_END);
	u32 len = ftell(f) / 4;
	fseek(f, 0, SEEK_SET);

	u32* list = new u32[len];
	u32* start = list;
	u32* end = list + len;

	fread(list, 4, len, f);

	aiMatrix4x4 mtx;
	float* pMtx = (float*)(&mtx);

	u32 index = 0;
	u32 command = *list++;
	while ( list < end )
	{
		u32 c = (command >> index) & 0xFF;
		switch ( c )
		{
			case 0x19: // mul mtx 4x3
			{
				printf("mul4x3 ");
				for ( u32 i = 0 ; i < 4 ; i++ )
				{
					for ( u32 j = 0 ; j < 3 ; j++ )
					{
						s32 v = *list++;
						float value = v / float(1 << 12);
						pMtx[i*4+j] = value;
						printf("%f ", value);
					}
					printf("%f ", 0);
				}
				printf("\n");
				break;
			}
			case 0x21: // normal
			{
				s32 n = *list;
				s32 nx = n & 0x3FF; if ( nx > 0x200 ) nx -= 0x200;
				s32 ny = (n >> 10) & 0x3FF; if ( ny >= 0x200 ) ny -= 0x400;
				s32 nz = (n >> 20) & 0x3FF; if ( nz >= 0x200 ) nz -= 0x400;
				printf("normal %f %f %f\n",
					nx / float(1 << 9),
					ny / float(1 << 9),
					nz / float(1 << 9));
				break;
			}
			case 0x22: // tex coord
			{
				s32 t = *list++;
				s32 tx = t & 0xFFFF;
				s32 ty = (t >> 16) & 0xFFFF;
				printf("texcoord %f %f\n", tx / float(1 << 15), ty / float(1 << 15));
				break;
			}
			case 0x24: // vtx 10
			{
				s32 v = *list++;
				s32 vx = v & 0x3FF; if ( vx > 0x200 ) vx -= 0x200;
				s32 vy = (v >> 10) & 0x3FF; if ( vy >= 0x200 ) vy -= 0x400;
				s32 vz = (v >> 20) & 0x3FF; if ( vz >= 0x200 ) vz -= 0x400;
				//printf("vtx10 %f %f %f\n",
				//	vx / float(1 << 6),
				//	vy / float(1 << 6),
				//	vz / float(1 << 6));
				aiVector3D vec(vx / float(1 << 6), vy / float(1 << 6), vz / float(1 << 6));
				vec *= mtx;
				printf("vtx10 %f %f %f\n", vec.x, vec.y, vec.z);
				printf("");
				break;
			}
			case 0x40: // begin
			{
				printf("begin %s\n", *list++ ? "strip" : "list");
				break;
			}
			default:
			{
				printf("Unknown command %x\n", c);
			}
		}

		index += 8;
		if ( index == 32 )
		{
			index = 0;
			command = *list++;
		}
	}

	delete[] start;

	return 0;
}