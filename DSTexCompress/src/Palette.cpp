#include "Palette.h"

s32 Max(s32 a, s32 b)
{
	return a > b ? a : b;
}

s32 Abs(s32 x)
{
	return x < 0 ? -x : x;
}

void Palette::UpdateVariance()
{
	Color max, min;
	min.r = min.g = min.b = 32;

	for ( u32 j = 0 ; j < length ; j++ )
	{
		ColorCube(colors[j], min, max);
	}

	Color diff = max - min;
	//variance = diff.r + diff.g + diff.b; // A-Type
	//variance = Max(Max(diff.r, diff.g), diff.b); // B-Type
	variance = Max(Max(Abs(diff.r), Abs(diff.g)), Abs(diff.b)); // C-Type
}

void PaletteGroup::UpdateVariance()
{
	Color min, max;
	min.r = min.g = min.b = 32;
	Palette* p = &palettes[0];
	for ( u32 i = palettes.size() ; i > 0 ; i-- )
	{
		for ( u32 j = 0 ; j < p->length ; j++ )
		{
			ColorCube(p->colors[j], min, max);
		}
		p++;
	}
	Color diff = max - min;
	//variance = diff.r + diff.g + diff.b; // A-Type
	//variance = Max(Max(diff.r, diff.g), diff.b); // B-Type
	variance = Max(Max(Abs(diff.r), Abs(diff.g)), Abs(diff.b)); // C-Type
}

void PaletteGroup::Split(PaletteGroup& left, PaletteGroup& right)
{
	qsort(&palettes[0], palettes.size(), sizeof(palettes[0]), (int (*)(const void*, const void*))ComparePalette);

	left.palettes.insert(left.palettes.begin(),
		&palettes[0],
		&palettes[0] + palettes.size() / 2);
	right.palettes.insert(right.palettes.begin(),
		&palettes[0] + palettes.size() - left.palettes.size(),
		&palettes[0] + palettes.size());

	left.UpdateVariance();
	right.UpdateVariance();
}