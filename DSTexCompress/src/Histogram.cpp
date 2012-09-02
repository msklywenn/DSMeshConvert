#include "Histogram.h"

#include <FreeImagePlus.h>
#include "Palette.h"

Histogram::Histogram(const Block& block)
:	totalWeight(0),
	longestEdge(0xDEADBEEF),
	longestEdgeLength(0xC0CACAFE)
{
	entries.reserve(16);

	for ( u32 y = 0 ; y < 4 ; y++ )
	{
		for ( u32 x = 0 ; x < 4 ; x++ )
		{
			const Color& c = block.pix[x + y * 4];

			totalWeight++;

			u32 i = 0;
			while ( i < entries.size() )
			{
				if ( entries[i].color == c )
				{
					entries[i].weight++;
					break;
				}

				i++;
			}

			if ( i == entries.size() )
			{
				entry e = { c, 1 };
				entries.push_back(e);
			}
		}
	}

	computeLongestEdge();
}

Histogram::Histogram(const PaletteGroup& pal)
:	totalWeight(0),
	longestEdge(0xDEADBEEF),
	longestEdgeLength(0xC0CACAFE)
{
	entries.reserve(pal.Length());

	for ( u32 i = 0 ; i < pal.Length() ; i++ )
	{
		const Palette& p = pal.GetPalette(i);

		for ( u32 j = 0 ; j < 4 ; j++ )
		{
			totalWeight++;

			const Color& c = p.colors[j];

			u32 i = 0;
			while ( i < entries.size() )
			{
				if ( entries[i].color == c )
				{
					entries[i].weight++;
					break;
				}

				i++;
			}

			if ( i == entries.size() )
			{
				entry e = { c, 1 };
				entries.push_back(e);
			}
		}
	}

	computeLongestEdge();
}

void Histogram::computeLongestEdge()
{	
	Color min, max;
	min.r = min.g = min.b = 32;

	for ( u32 i = 0 ; i < entries.size() ; i++ )
	{
		ColorCube(entries[i].color, min, max);
	}

	Color len = max - min;
	if ( len.r > len.g && len.r > len.b )
	{
		longestEdge = 0;
		longestEdgeLength = len.r;
	}
	else
	{
		if ( len.g > len.b )
		{
			longestEdge = 1;
			longestEdgeLength = len.g;
		}
		else
		{
			longestEdge = 2;
			longestEdgeLength = len.b;
		}
	}
}

Color Histogram::Average() const
{
	Color a;

	for ( u32 i = 0 ; i < entries.size() ; i++ )
	{
		a.r += entries[i].color.r * entries[i].weight;
		a.g += entries[i].color.g * entries[i].weight;
		a.b += entries[i].color.b * entries[i].weight;
	}

	a.r /= totalWeight;
	a.g /= totalWeight;
	a.b /= totalWeight;

	return a;
}

void Histogram::Split(Histogram& left, Histogram& right)
{
	// Sort data along longest edge
	static int (*cmp[])(const Color*, const Color*) =
	{
		CompareRed,
		CompareGreen,
		CompareBlue
	};

	qsort(&entries[0], entries.size(), sizeof(entries[0]), (int (*)(const void*, const void*))cmp[longestEdge]);

	// split data
	left.totalWeight = totalWeight / 2;
	right.totalWeight = totalWeight - left.totalWeight;

	u32 i = 0, w = 0;
	const entry* e = &entries[0];
	while ( w < left.totalWeight )
	{
		left.entries.push_back(*e++);
		w += left.entries[i].weight;
		i++;
	}

	if ( w > left.totalWeight )
	{
		right.entries.push_back(left.entries[left.entries.size() - 1]);
		u32 rw = w - left.totalWeight;
		left.entries[left.entries.size() - 1].weight -= rw;
		right.entries[0].weight = rw;
		i = 1;
		w = rw;
	}
	else
	{
		i = 0;
		w = 0;
	}

	while ( w < right.totalWeight )
	{
		right.entries.push_back(*e++);
		w += right.entries[i].weight;
		i++;
	}

	left.computeLongestEdge();
	right.computeLongestEdge();
}