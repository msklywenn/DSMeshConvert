#ifndef _HISTOGRAM_H_
#define _HISTOGRAM_H_

#include <vector>
#include "Color.h"
class fipImage;
class PaletteGroup;

struct Block
{
	Color pix[16];
};

class Histogram
{
	struct entry
	{
		Color	color;
		u32		weight;
	};

	std::vector<entry> entries;
	u32		totalWeight;
	u32		longestEdge;
	u32		longestEdgeLength;

	void computeLongestEdge();

public:
	Histogram() : entries(0), totalWeight(0), longestEdge(0xFFFFFFFF), longestEdgeLength(0xFFFFFFFF) {}
	Histogram(const Block& block);
	Histogram(const PaletteGroup& group);

	inline u32 Length() const { return entries.size(); }
	inline Color GetColor(u32 c) const { return entries[c].color; }
	inline bool IsSplittable() const { return entries.size() > 1; }
	inline u32 GetImportance() const { return entries.size() * totalWeight * longestEdgeLength; }

	void Split(Histogram& left, Histogram& right);

	Color Average() const;
};

#endif // _HISTOGRAM_H_