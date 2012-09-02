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

/*
To do:
	- add an offset parameter between two input values (avoid some data recopy sometimes)
	- unroll ? asm ?
	- warning when skipping last pass
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Precompiled Header
//#include "Stdafx.h"
#include "RevisitedRadix.h"
#include <string.h>
#define RELEASEARRAY(x) { if ( x ) delete[] (x); }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//																	RadixSorter Class Implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RadixSorter::RadixSorter()
{
	// Initialize
	mIndices		= 0;
	mIndices2		= 0;
	mCurrentSize	= 0;

	// Allocate input-independent ram
	mHistogram		= new u32[256*4];
	mOffset			= new u32[256];

	// Initialize indices
	ResetIndices();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Destructor
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RadixSorter::~RadixSorter()
{
	// Release everything
	RELEASEARRAY(mOffset);
	RELEASEARRAY(mHistogram);
	RELEASEARRAY(mIndices2);
	RELEASEARRAY(mIndices);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main sort routine
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	input,			a list of integer values to sort
//				nb,				#values to sort
//				signedvalues,	true to handle negative values, false if you know your input buffer only contains positive values
// Output	:	mIndices,		a list of indices in sorted order, i.e. in the order you may process your data
// Return	:	Self-Reference
// Exception:	-
// Remark	:	this one is for integer values
RadixSorter& RadixSorter::Sort(u32* input, u32 nb, bool signedvalues)
{
	// Resize lists if needed
	if(nb>mCurrentSize)
	{
		// Free previously used ram
		RELEASEARRAY(mIndices2);
		RELEASEARRAY(mIndices);

		// Get some fresh one
		mIndices		= new u32[nb];
		mIndices2		= new u32[nb];
		mCurrentSize	= nb;

		// Initialize indices so that the input buffer is read in sequential order
		ResetIndices();
	}

	// Clear counters
	memset(mHistogram, 0, 256*4*sizeof(u32));

	// Create histograms (counters). Counters for all passes are created in one run.
	// Pros:	read input buffer once instead of four times
	// Cons:	mHistogram is 4Kb instead of 1Kb
	// We must take care of signed/unsigned values for temporal coherence.... I just
	// have 2 code paths even if just a single opcode changes. Self-modifying code, someone?

	// Temporal coherence
	bool AlreadySorted = true;						// Optimism...
	u32* Indices = mIndices;
	// Prepare to count
	u8* p = (u8*)input;
	u8* pe = &p[nb*4];
	u32* h0= &mHistogram[0];						// Histogram for first pass (LSB)
	u32* h1= &mHistogram[256];					// Histogram for second pass
	u32* h2= &mHistogram[512];					// Histogram for third pass
	u32* h3= &mHistogram[768];					// Histogram for last pass (MSB)
	if(!signedvalues)
	{
		// Temporal coherence
		u32 PrevVal = input[mIndices[0]];

		while(p!=pe)
		{
			// Temporal coherence
			u32 Val = input[*Indices++];				// Read input buffer in previous sorted order
			if(Val<PrevVal)	AlreadySorted = false;		// Check whether already sorted or not
			PrevVal = Val;								// Update for next iteration

			// Create histograms
			h0[*p++]++;	h1[*p++]++;	h2[*p++]++;	h3[*p++]++;
		}
	}
	else
	{
		// Temporal coherence
		s32 PrevVal = (s32)input[mIndices[0]];

		while(p!=pe)
		{
			// Temporal coherence
			s32 Val = (s32)input[*Indices++];		// Read input buffer in previous sorted order
			if(Val<PrevVal)	AlreadySorted = false;		// Check whether already sorted or not
			PrevVal = Val;								// Update for next iteration

			// Create histograms
			h0[*p++]++;	h1[*p++]++;	h2[*p++]++;	h3[*p++]++;
		}
	}

	// If all input values are already sorted, we just have to return and leave the previous list unchanged.
	// That way the routine may take advantage of temporal coherence, for example when used to sort transparent faces.
	if(AlreadySorted) return *this;

	// Compute #negative values involved if needed
	u32 NbNegativeValues = 0;
	if(signedvalues)
	{
		// An efficient way to compute the number of negatives values we'll have to deal with is simply to sum the 128
		// last values of the last histogram. Last histogram because that's the one for the Most Significant Byte,
		// responsible for the sign. 128 last values because the 128 first ones are related to positive numbers.
		u32* h3= &mHistogram[768];
		for(u32 i=128;i<256;i++)	NbNegativeValues += h3[i];	// 768 for last histogram, 128 for negative part
	}

	// Radix sort, j is the pass number (0=LSB, 3=MSB)
	for(u32 j=0;j<4;j++)
	{
		// Shortcut to current counters
		u32* CurCount = &mHistogram[j<<8];

		// Reset flag. The sorting pass is supposed to be performed. (default)
		bool PerformPass = true;

		// Check pass validity [some cycles are lost there in the generic case, but that's ok, just a little loop]
		for(u32 i=0;i<256;i++)
		{
			// If all values have the same byte, sorting is useless. It may happen when sorting bytes or words instead of dwords.
			// This routine actually sorts words faster than dwords, and bytes faster than words. Standard running time (O(4*n))is
			// reduced to O(2*n) for words and O(n) for bytes. Running time for floats depends on actual values...
			if(CurCount[i]==nb)
			{
				PerformPass=false;
				break;
			}
			// If at least one count is not 0, we suppose the pass must be done. Hence, this test takes very few CPU time in the generic case.
			if(CurCount[i])	break;
		}

		// Sometimes the fourth (negative) pass is skipped because all numbers are negative and the MSB is 0xFF (for example). This is
		// not a problem, numbers are correctly sorted anyway.
		if(PerformPass)
		{
			// Should we care about negative values?
			if(j!=3 || !signedvalues)
			{
				// Here we deal with positive values only

				// Create offsets
				mOffset[0] = 0;
				for(u32 i=1;i<256;i++)		mOffset[i] = mOffset[i-1] + CurCount[i-1];
			}
			else
			{
				// This is a special case to correctly handle negative integers. They're sorted in the right order but at the wrong place.

				// Create biased offsets, in order for negative numbers to be sorted as well
				mOffset[0] = NbNegativeValues;												// First positive number takes place after the negative ones
				for(u32 i=1;i<128;i++)		mOffset[i] = mOffset[i-1] + CurCount[i-1];	// 1 to 128 for positive numbers

				// Fixing the wrong place for negative values
				mOffset[128] = 0;
				for(u32 i=129;i<256;i++)	mOffset[i] = mOffset[i-1] + CurCount[i-1];
			}

			// Perform Radix Sort
			u8* InputBytes = (u8*)input;
			u32* Indices = mIndices;
			u32* IndicesEnd = &mIndices[nb];
			InputBytes += j;
			while(Indices!=IndicesEnd)
			{
				u32 id = *Indices++;
				mIndices2[mOffset[InputBytes[id<<2]]++] = id;
			}

			// Swap pointers for next pass
			u32* Tmp	= mIndices;
			mIndices	= mIndices2;
			mIndices2	= Tmp;
		}
	}
	return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main sort routine
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	input,			a list of floating-point values to sort
//				nb,				#values to sort
// Output	:	mIndices,		a list of indices in sorted order, i.e. in the order you may process your data
// Return	:	Self-Reference
// Exception:	-
// Remark	:	this one is for floating-point values
RadixSorter& RadixSorter::Sort(float* input2, u32 nb)
{
	u32* input = (u32*)input2;

	// Resize lists if needed
	if(nb>mCurrentSize)
	{
		// Free previously used ram
		RELEASEARRAY(mIndices2);
		RELEASEARRAY(mIndices);

		// Get some fresh one
		mIndices		= new u32[nb];
		mIndices2		= new u32[nb];
		mCurrentSize	= nb;

		// Initialize indices so that the input buffer is read in sequential order
		ResetIndices();
	}

	// Clear counters
	memset(mHistogram, 0, 256*4*sizeof(u32));

	// Create histograms (counters). Counters for all passes are created in one run.
	// Pros:	read input buffer once instead of four times
	// Cons:	mHistogram is 4Kb instead of 1Kb
	// Floating-point values are always supposed to be signed values, so there's only one code path there.
	// Please note the floating point comparison needed for temporal coherence! Although the resulting asm code
	// is dreadful, this is surprisingly not such a performance hit - well, I suppose that's a big one on first
	// generation Pentiums....We can't make comparison on integer representations because, as Chris said, it just
	// wouldn't work with mixed positive/negative values....
	{
		// 3 lines for temporal coherence support
		float PrevVal = input2[mIndices[0]];
		bool AlreadySorted = true;						// Optimism...
		u32* Indices = mIndices;

		// Prepare to count
		u8* p = (u8*)input;
		u8* pe = &p[nb*4];
		u32* h0= &mHistogram[0];						// Histogram for first pass (LSB)
		u32* h1= &mHistogram[256];					// Histogram for second pass
		u32* h2= &mHistogram[512];					// Histogram for third pass
		u32* h3= &mHistogram[768];					// Histogram for last pass (MSB)
		while(p!=pe)
		{
			// Temporal coherence
			float Val = input2[*Indices++];				// Read input buffer in previous sorted order
			if(Val<PrevVal)	AlreadySorted = false;		// Check whether already sorted or not
			PrevVal = Val;								// Update for next iteration

			// Create histograms
			h0[*p++]++;	h1[*p++]++;	h2[*p++]++;	h3[*p++]++;
		}

		// If all input values are already sorted, we just have to return and leave the previous list unchanged.
		// That way the routine may take advantage of temporal coherence, for example when used to sort transparent faces.
		if(AlreadySorted) return *this;
	}

	// Compute #negative values involved if needed
	u32 NbNegativeValues = 0;
	// An efficient way to compute the number of negatives values we'll have to deal with is simply to sum the 128
	// last values of the last histogram. Last histogram because that's the one for the Most Significant Byte,
	// responsible for the sign. 128 last values because the 128 first ones are related to positive numbers.
	u32* h3= &mHistogram[768];
	for(u32 i=128;i<256;i++)	NbNegativeValues += h3[i];	// 768 for last histogram, 128 for negative part

	// Radix sort, j is the pass number (0=LSB, 3=MSB)
	for(u32 j=0;j<4;j++)
	{
		// Shortcut to current counters
		u32* CurCount = &mHistogram[j<<8];

		// Reset flag. The sorting pass is supposed to be performed. (default)
		bool PerformPass = true;

		// Check pass validity [some cycles are lost there in the generic case, but that's ok, just a little loop]
		for(u32 i=0;i<256;i++)
		{
			// If all values have the same byte, sorting is useless. It may happen when sorting bytes or words instead of dwords.
			// This routine actually sorts words faster than dwords, and bytes faster than words. Standard running time (O(4*n))is
			// reduced to O(2*n) for words and O(n) for bytes. Running time for floats depends on actual values...
			if(CurCount[i]==nb)
			{
				PerformPass=false;
				break;
			}
			// If at least one count is not 0, we suppose the pass must be done. Hence, this test takes very few CPU time in the generic case.
			if(CurCount[i])	break;
		}

		if(PerformPass)
		{
			// Should we care about negative values?
			if(j!=3)
			{
				// Here we deal with positive values only

				// Create offsets
				mOffset[0] = 0;
				for(u32 i=1;i<256;i++)		mOffset[i] = mOffset[i-1] + CurCount[i-1];

				// Perform Radix Sort
				u8* InputBytes = (u8*)input;
				u32* Indices = mIndices;
				u32* IndicesEnd = &mIndices[nb];
				InputBytes += j;
				while(Indices!=IndicesEnd)
				{
					u32 id = *Indices++;
					mIndices2[mOffset[InputBytes[id<<2]]++] = id;
				}
			}
			else
			{
				// This is a special case to correctly handle negative values

				// Create biased offsets, in order for negative numbers to be sorted as well
				mOffset[0] = NbNegativeValues;												// First positive number takes place after the negative ones
				for(u32 i=1;i<128;i++)		mOffset[i] = mOffset[i-1] + CurCount[i-1];	// 1 to 128 for positive numbers

				// We must reverse the sorting order for negative numbers!
				mOffset[255] = 0;
				for(u32 i=0;i<127;i++)		mOffset[254-i] = mOffset[255-i] + CurCount[255-i];	// Fixing the wrong order for negative values
				for(u32 i=128;i<256;i++)	mOffset[i] += CurCount[i];							// Fixing the wrong place for negative values

				// Perform Radix Sort
				for(u32 i=0;i<nb;i++)
				{
					u32 Radix = input[mIndices[i]]>>24;								// Radix byte, same as above. AND is useless here (u32).
					// ### cmp to be killed. Not good. Later.
					if(Radix<128)		mIndices2[mOffset[Radix]++] = mIndices[i];		// Number is positive, same as above
					else				mIndices2[--mOffset[Radix]] = mIndices[i];		// Number is negative, flip the sorting order
				}
			}

			// Swap pointers for next pass
			u32* Tmp	= mIndices;
			mIndices	= mIndices2;
			mIndices2	= Tmp;
		}
	}
	return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A method to reset the indices.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	-
// Output	:	mIndices is reset
// Return	:	Self-Reference
// Exception:	-
// Remark	:	-
RadixSorter& RadixSorter::ResetIndices()
{
	for(u32 i=0;i<mCurrentSize;i++)
	{
		mIndices[i] = i;
	}
	return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A method to get the ram used.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	-
// Output	:	-
// Return	:	u32,	memory used in bytes
// Exception:	-
// Remark	:	-
u32 RadixSorter::GetUsedRam()
{
	u32 UsedRam = 0;
	UsedRam += 256*4*sizeof(u32);			// Histograms
	UsedRam += 256*sizeof(u32);				// Offsets
	UsedRam += 2*mCurrentSize*sizeof(u32);	// 2 lists of indices
	return UsedRam;
}

