#ifndef _CUT_H_
#define _CUT_H_

template <typename T>
T* FindMostImportantSplittable(T* list, u32 len)
{
	T* r = 0;
	u32 importance = 0;

	for ( u32 i = len ; i > 0 ; i-- )
	{
		if ( list->IsSplittable() )
		{
			u32 imp = list->GetImportance();
			if ( imp > importance )
			{
				importance = imp;
				r = list;
			}
		}

		list++;
	}

	return r;
}

// Median cut inspired
//
// Instead of recursively splitting everything
// it splits the "most important"/"bigger" element
// until the required number of elements
// is met or there is nothing to split.
//
// Returns the number of elements.
template <typename T>
u32 WeirdCut(const T& src, T* dst, u32 dstLength)
{
	if ( src.IsSplittable() && dstLength > 1 )
	{
		T copy(src);
		copy.Split(dst[0], dst[1]);

		T* toSplit = FindMostImportantSplittable(dst, 2);

		u32 len = 2;
		while ( len < dstLength && toSplit )
		{
			T splitMe(*toSplit);

			// reset element, gruik style
			toSplit->~T();
			toSplit = new(toSplit) T;

			splitMe.Split(*toSplit, dst[len]);
			len++;
			toSplit = FindMostImportantSplittable(dst, len);
		}

		return len;
	}
	else
	{
		dst[0] = src;

		return 1;
	}
}

#endif // _CUT_H_