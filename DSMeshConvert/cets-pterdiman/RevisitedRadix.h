///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Source code for "Radix Sort Revisited"
// (C) 2000, Pierre Terdiman (p.terdiman@wanadoo.fr)
//
// Works with IEEE floats only.
// Version is 1.1.
//
// This is my new radix routine:
//				- it uses indices and doesn't recopy the values anymore, hence wasting less ram
//				- it creates all the histograms in one run instead of four
//				- it sorts words faster than dwords and bytes faster than words
//				- it correctly sorts negative floats by patching the offsets
//				- it automatically takes advantage of temporal coherence
//				- multiple keys support is a side effect of temporal coherence
//				- it may be worth recoding in asm...
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __RADIXSORT_H__
#define __RADIXSORT_H__
#include "../types.h"
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//
	//																			Class RadixSorter
	//
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	class RadixSorter
	{
	public:
		// Constructor/Destructor
			RadixSorter();
			~RadixSorter();

		// Sorting methods
			RadixSorter&			Sort(u32* input, u32 nb, bool signedvalues=true);
			RadixSorter&			Sort(float* input, u32 nb);

		// Access to results
		// mIndices is a list of indices in sorted order, i.e. in the order you may further process your data
			u32*					GetIndices()				{ return mIndices; }

		// Reset the inner indices
			RadixSorter&			ResetIndices();

		// Stats
			u32					GetUsedRam();
	private:
			u32*					mHistogram;					// Counters for each byte
			u32*					mOffset;					// Offsets (nearly a cumulative distribution function)

			u32					mCurrentSize;				// Current size of the indices list
			u32*					mIndices;					// Two lists, swapped each pass
			u32*					mIndices2;
	};

#endif // __RADIXSORT_H__
