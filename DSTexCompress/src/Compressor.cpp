#include "Compressor.h"

#include <FreeImagePlus.h>
#include "Histogram.h"
#include "Palette.h"
#include "Cut.h"

static bool IsPowerOfTwo(u32 x)
{
	if ( x == 0 ) return false;
	return (x & (x - 1)) == 0;
}

bool Compressor::Open(const char* input)
{
	if ( ! image.load(input) )
	{
		fprintf(stderr, "Can't load %s\n", input);
		return false;
	}

	u32 width = image.getWidth();
	u32 height = image.getHeight();

	if ( width < 8 || height < 8
		|| width > 1024 || height > 1024
		|| !IsPowerOfTwo(width) || !IsPowerOfTwo(height) )
	{
		fprintf(stderr, "Wrong texture size (%dx%d)\n", width, height);
		return false;
	}

	image.convertTo16Bits555();

	return true;
}

static void BuildReducedPalette(Histogram& histogram, Palette& pal)
{
	Histogram c[4];
	pal.length = WeirdCut(histogram, c, 4);

	for ( u32 i = 0 ; i < pal.length ; i++ )
	{
		pal.colors[i] = c[i].Average();
	}

	pal.UpdateVariance();
}

static u32 DistancePixelsPalette(Color* pixels, Palette* pal)
{
	u32 dist = 0;

	for ( u32 i = 0 ; i < 16 ; i++ )
	{
		u32 best = pixels->Distance(pal->colors[0]);

		for ( u32 i = 1 ; i < pal->length ; i++ )
		{
			u32 d = pixels->Distance(pal->colors[i]);
			if ( d < best )
			{
				best = d;
			}
		}

		dist += best;
		pixels++;
	}

	return dist;
}

static u32 FindBestPalette(Color* pixels, Palette* palettes, u32 nbPalettes)
{
	u32 id = 0;
	u32 best = DistancePixelsPalette(pixels, palettes++);

	for ( u32 i = 1 ; i < nbPalettes ; i++ )
	{
		u32 d = DistancePixelsPalette(pixels, palettes);

		if ( d < best )
		{
			id = i;
			best = d;
		}
		palettes++;
	}

	return id;
}

static inline u16 Perturb(u16 pix, Color error, s32 factor)
{
	Color c(pix);
	c.r = (c.r * 16 + error.r * factor) / 16;
	c.g = (c.g * 16 + error.g * factor) / 16;
	c.b = (c.b * 16 + error.b * factor) / 16;
	return c.EncodePC555();
}

void Compressor::Process(const char* output, u32 nbPals, bool extend, bool dither, const char* preview)
{
	u32 width = image.getWidth();
	u32 height = image.getHeight();
	u32 blockWidth = width / 4;
	u32 blockHeight = height / 4;
	u32 nbBlocks = blockWidth * blockHeight;
	u16* pixels = (u16*) image.accessPixels();

	// Final file is
	//  + header (width, height, 4-color palette count)
	//  + data (blocks, indices, palettes)
	u32 dataLength = 3 + nbBlocks + nbBlocks / 2 + nbPals * 2;
	u32* data = new u32[dataLength];
	data[0] = width;
	data[1] = height;
	data[2] = nbPals;
	u32* pBlocks = data + 3;
	u16* pIndices = (u16*)(pBlocks + nbBlocks);
	u16* pPalettes = pIndices + nbBlocks;

	// Compute one 4-color palette for each block
	printf("Palettizing...\n");
	PaletteGroup palettes;
	Block* blocks = new Block[nbBlocks];
	for ( u32 y = 0 ; y < height ; y += 4 )
	{
		for ( u32 x = 0 ; x < width ; x += 4 )
		{
			u32 ofs = (x / 4) + (y / 4) * blockWidth;

			Block& block = blocks[ofs];
			for ( u32 i = 0 ; i < 4 ; i++ )
			{
				for ( u32 j = 0 ; j < 4 ; j++ )
				{
					block.pix[j + i * 4].DecodePC555(pixels[(x + j) + (y + i) * width]);
				}
			}

			Histogram histogram(block);
			Palette p;
			BuildReducedPalette(histogram, p);
			palettes.Add(p);
		}
	}

	// Reduce palettes
	printf("Reducing to %d palettes...\n", nbPals);
	PaletteGroup* groups = new PaletteGroup[nbPals];
	u32 nbGroups = WeirdCut(palettes, groups, nbPals);

	u32 nbNewPalettes = nbPals * (extend ? 5 : 1);
	Palette* newPalettes = new Palette[nbNewPalettes];
	Palette* pNewPalettes = newPalettes;
	for ( u32 i = 0 ; i < nbGroups ; i++ )
	{
		Histogram histogram(groups[i]);
		BuildReducedPalette(histogram, *pNewPalettes);

		if ( extend )
		{
			// Build 2-color extended palettes
			// Keep first two colors, extend to 3 (mode 1)
			Palette* p = pNewPalettes + nbPals + i;
			p->length = 3;
			p->colors[0] = pNewPalettes->colors[0];
			p->colors[1] = pNewPalettes->colors[1];
			p->colors[2] = (p->colors[0] + p->colors[1]) / 2;
			// Keep last two colors, extend to 3 (mode 1)
			p++;
			p->length = 3;
			p->colors[0] = pNewPalettes->colors[2];
			p->colors[1] = pNewPalettes->colors[3];
			p->colors[2] = (p->colors[0] + p->colors[1]) / 2;
			// Keep last two colors, extend to 4 (mode 3)
			p += nbPals * 2;
			p->length = 4;
			p->colors[0] = pNewPalettes->colors[2];
			p->colors[1] = pNewPalettes->colors[3];
			p->colors[2] = (p->colors[0] * 5 + p->colors[1] * 3) / 8;
			p->colors[3] = (p->colors[0] * 3 + p->colors[1] * 5) / 8;
			// Keep first two colors, extend to 4 (mode 3)
			p--;
			p->length = 4;
			p->colors[0] = pNewPalettes->colors[0];
			p->colors[1] = pNewPalettes->colors[1];
			p->colors[2] = (p->colors[0] * 5 + p->colors[1] * 3) / 8;
			p->colors[3] = (p->colors[0] * 3 + p->colors[1] * 5) / 8;
		}

		// store to file
		for ( u32 c = 0 ; c < 4 ; c++ )
		{
			*pPalettes++ = newPalettes[i].colors[c].EncodeNDS();
		}

		pNewPalettes++;
	}

	delete[] groups;

	u32* mapping = new u32[nbBlocks];

	// Match palettes to blocks and save indices
	printf("Matching palettes...\n");
	for ( u32 y = 0 ; y < blockHeight ; y++ )
	{
		for ( u32 x = 0 ; x < blockWidth ; x++ )
		{
			u32 ofs = x + y * blockWidth;
			Block& block = blocks[ofs];

			// (good thing we have fast cpus now, brute force rulez.)
			u32 id = FindBestPalette(block.pix, newPalettes, nbNewPalettes);

			mapping[x + y * blockWidth] = id;

			// Encode index data to file
			u32 mode;
			u32 type = id / nbPals;
			if ( type == 0 )
			{
				mode = 2;
				id *= 2;
			}
			else if ( type < 3 )
			{
				mode = 1;
				id -= nbPals;
			}
			else
			{
				mode = 3;
				id -= nbPals * 3;
			}

			*pIndices++ = id | (mode << 14);
		}
	}

	// Encode blocks
	printf("Compressing...\n");
	memset(pBlocks, 0, nbBlocks * sizeof(u32));
	for ( u32 y = 0 ; y < height ; y++ )
	{
		for ( u32 x = 0 ; x < width ; x++ )
		{
			u32 blockOffset = x / 4 + (y / 4) * blockWidth;
			u32 id = mapping[blockOffset];

			// Encode block to file
			Palette& pal = newPalettes[id];

			u16* pix = &pixels[x + y * width];
			Color c(*pix);

			// find best matching color in palette
			u32 best = 0;
			u32 dist = c.Distance(pal.colors[0]);
			for ( u32 k = 1 ; k < pal.length ; k++ )
			{
				u32 d = c.Distance(pal.colors[k]);
				if ( d < dist )
				{
					dist = d;
					best = k;
				}
			}

			// encode
			pBlocks[blockOffset] |= best << (2 * ((x % 4) + (y % 4) * 4));

#if DITHERING
			// floyd-steinberg dithering
			//
			// results are terrible with dithering enabled
			// I guess the error diffusion makes the palette
			// not match with the blocks anymore
			if ( dither )
			{
				Color error = c - pal.colors[best];
				if ( x < width - 1 )
				{
					pix[1] = Perturb(pix[1], error, 7);
					if ( y < height - 1 )
					{
						pix[width + 1] = Perturb(pix[width + 1], error, 1);
					}
				}
				if ( y < height - 1 )
				{
					pix[width] = Perturb(pix[width], error, 5);
					if ( x > 0 )
					{
						pix[width - 1] = Perturb(pix[width - 1], error, 3);
					}
				}
			}
#endif

			// for preview
			*pix = pal.colors[best].EncodePC555();
		}
	}

	// Store file
	FILE* f = fopen(output, "wb");
	if ( f )
	{
		fwrite(data, sizeof(data[0]), dataLength, f);
		fclose(f);
	}
	else
	{
		fprintf(stderr, "Warning: Could not save to %s\n", output);
	}

	// Store preview
	if ( preview )
	{
		image.convertTo24Bits();
		image.save(preview);
	}

	delete[] mapping;
	delete[] data;
	delete[] newPalettes;
	delete[] blocks;
}