#ifndef _PALETTE_H_
#define _PALETTE_H_

#include "Color.h"
#include <vector>

struct Palette
{
	Color	colors[4];
	u32		length;
	s32		variance;

	Palette() : length(0), variance(0) {}

	void UpdateVariance();
};

inline int ComparePalette(const Palette* a, const Palette* b)
{
	return a->variance - b->variance;
}

class PaletteGroup
{
	std::vector<Palette> palettes;
	u32			variance;

public:
	PaletteGroup() : variance(0) {}

	inline bool IsSplittable() const { return palettes.size() > 4; }
	inline u32 GetImportance() const { return variance * palettes.size(); }
	inline u32 Length() const { return palettes.size(); }
	inline const Palette& GetPalette(u32 i) const { return palettes[i]; }
	inline void Add(Palette& p) { palettes.push_back(p); }

	void UpdateVariance();
	void Split(PaletteGroup& left, PaletteGroup& right);
};

#endif // _PALETTE_H_