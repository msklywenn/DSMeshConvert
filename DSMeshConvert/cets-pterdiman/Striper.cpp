///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Source code for "Creating Efficient Triangle Strips"
// (C) 2000, Pierre Terdiman (p.terdiman@wanadoo.fr)
//
// Version is 2.0.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Precompiled Header
//#include "Stdafx.h"
#include "Striper.h"
#include <string.h>
#include "RevisitedRadix.h"
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//																	Striper Class Implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Striper::Striper() : mAdj(0), mTags(0), mStripLengths(0), mStripRuns(0), mSingleStrip(0)
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Destructor
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Striper::~Striper()
{
	FreeUsedRam();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A method to free possibly used ram
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	-
// Output	:	-
// Return	:	Self-reference
// Exception:	-
// Remark	:	-
Striper& Striper::FreeUsedRam()
{
	RELEASE(mSingleStrip);
	RELEASE(mStripRuns);
	RELEASE(mStripLengths);
	RELEASEARRAY(mTags);
	RELEASE(mAdj);
	return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A method to initialize the striper
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	create,		the creation structure
// Output	:	-
// Return	:	true if success
// Exception:	-
// Remark	:	-
bool Striper::Init(STRIPERCREATE& create)
{
	// Release possibly already used ram
	FreeUsedRam();

	// Create adjacencies
	{
		mAdj = new Adjacencies;
		if(!mAdj)	return false;

		ADJACENCIESCREATE ac;
		ac.NbFaces	= create.NbFaces;
		ac.DFaces	= create.DFaces;
		ac.WFaces	= create.WFaces;
		bool Status = mAdj->Init(ac);
		if(!Status)	{ RELEASE(mAdj); return false; }

		Status = mAdj->CreateDatabase();
		if(!Status)	{ RELEASE(mAdj); return false; }

		mAskForWords		= create.AskForWords;
		mOneSided			= create.OneSided;
		mSGIAlgorithm		= create.SGIAlgorithm;
		mConnectAllStrips	= create.ConnectAllStrips;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A method to create the triangle strips
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	result,		the result structure
// Output	:	-
// Return	:	true if success
// Exception:	-
// Remark	:	-
bool Striper::Compute(STRIPERRESULT& result)
{
	// You must call Init() first
	if(!mAdj)	return false;

	// Get some bytes
	mStripLengths			= new CustomArray;				if(!mStripLengths)	return false;
	mStripRuns				= new CustomArray;				if(!mStripRuns)		return false;
	mTags					= new bool[mAdj->mNbFaces];		if(!mTags)			return false;
	u32* Connectivity	= new u32[mAdj->mNbFaces];	if(!Connectivity)	return false;

	// mTags contains one bool/face. True=>the face has already been included in a strip
	memset(mTags, 0, mAdj->mNbFaces*sizeof(bool));

	// Compute the number of connections for each face. This buffer is further recycled into
	// the insertion order, ie contains face indices in the order we should treat them
	memset(Connectivity, 0, mAdj->mNbFaces*sizeof(u32));
	if(mSGIAlgorithm)
	{
		// Compute number of adjacent triangles for each face
		for(u32 i=0;i<mAdj->mNbFaces;i++)
		{
			AdjTriangle* Tri = &mAdj->mFaces[i];
			if(!IS_BOUNDARY(Tri->ATri[0]))	Connectivity[i]++;
			if(!IS_BOUNDARY(Tri->ATri[1]))	Connectivity[i]++;
			if(!IS_BOUNDARY(Tri->ATri[2]))	Connectivity[i]++;
		}

		// Sort by number of neighbors
		RadixSorter RS;
		u32* Sorted = RS.Sort(Connectivity, mAdj->mNbFaces).GetIndices();

		// The sorted indices become the order of insertion in the strips
		memcpy(Connectivity, Sorted, mAdj->mNbFaces*sizeof(u32));
	}
	else
	{
		// Default order
		for(u32 i=0;i<mAdj->mNbFaces;i++)	Connectivity[i] = i;
	}

	mNbStrips			= 0;	// #strips created
	u32 TotalNbFaces	= 0;	// #faces already transformed into strips
	u32 Index		= 0;	// Index of first face

	while(TotalNbFaces!=mAdj->mNbFaces)
	{
		// Look for the first face [could be optimized]
		while(mTags[Connectivity[Index]])	Index++;
		u32 FirstFace = Connectivity[Index];

		// Compute the three possible strips from this face and take the best
		TotalNbFaces += ComputeBestStrip(FirstFace);

		// Let's wrap
		mNbStrips++;
	}

	// Free now useless ram
	RELEASEARRAY(Connectivity);
	RELEASEARRAY(mTags);

	// Fill result structure and exit
	result.NbStrips		= mNbStrips;
	result.StripLengths	= (u32*)	mStripLengths	->Collapse();
	result.StripRuns	=			mStripRuns		->Collapse();

	if(mConnectAllStrips)	ConnectAllStrips(result);

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A method to compute the three possible strips starting from a given face
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	face,		the first face
// Output	:	-
// Return	:	u32,		the #faces included in the strip
// Exception:	-
// Remark	:	mStripLengths and mStripRuns are filled with strip data
u32 Striper::ComputeBestStrip(u32 face)
{
	u32* Strip[3];		// Strips computed in the 3 possible directions
	u32* Faces[3];		// Faces involved in the 3 previous strips
	u32 Length[3];		// Lengths of the 3 previous strips

	u32 FirstLength[3];	// Lengths of the first parts of the strips are saved for culling

	// Starting references
	u32 Refs0[3];
	u32 Refs1[3];
	Refs0[0] = mAdj->mFaces[face].VRef[0];
	Refs1[0] = mAdj->mFaces[face].VRef[1];

	// Bugfix by Eric Malafeew!
	Refs0[1] = mAdj->mFaces[face].VRef[2];
	Refs1[1] = mAdj->mFaces[face].VRef[0];

	Refs0[2] = mAdj->mFaces[face].VRef[1];
	Refs1[2] = mAdj->mFaces[face].VRef[2];

	// Compute 3 strips
	for(u32 j=0;j<3;j++)
	{
		// Get some bytes for the strip and its faces
		Strip[j] = new u32[mAdj->mNbFaces+2+1+2];	// max possible length is NbFaces+2, 1 more if the first index gets replicated
		Faces[j] = new u32[mAdj->mNbFaces+2];
		memset(Strip[j], 0xff, (mAdj->mNbFaces+2+1+2)*sizeof(u32));
		memset(Faces[j], 0xff, (mAdj->mNbFaces+2)*sizeof(u32));

		// Create a local copy of the tags
		bool* Tags	= new bool[mAdj->mNbFaces];
		memcpy(Tags, mTags, mAdj->mNbFaces*sizeof(bool));

		// Track first part of the strip
		Length[j] = TrackStrip(face, Refs0[j], Refs1[j], &Strip[j][0], &Faces[j][0], Tags);

		// Save first length for culling
		FirstLength[j] = Length[j];
//		if(j==1)	FirstLength[j]++;	// ...because the first face is written in reverse order for j==1

		// Reverse first part of the strip
		for(u32 i=0;i<Length[j]/2;i++)
		{
			Strip[j][i]				^= Strip[j][Length[j]-i-1];
			Strip[j][Length[j]-i-1]	^= Strip[j][i];
			Strip[j][i]				^= Strip[j][Length[j]-i-1];
		}
		for(u32 i=0;i<(Length[j]-2)/2;i++)
		{
			Faces[j][i]				^= Faces[j][Length[j]-i-3];
			Faces[j][Length[j]-i-3]	^= Faces[j][i];
			Faces[j][i]				^= Faces[j][Length[j]-i-3];
		}

		// Track second part of the strip
		u32 NewRef0 = Strip[j][Length[j]-3];
		u32 NewRef1 = Strip[j][Length[j]-2];
		u32 ExtraLength = TrackStrip(face, NewRef0, NewRef1, &Strip[j][Length[j]-3], &Faces[j][Length[j]-3], Tags);
		Length[j]+=ExtraLength-3;

		// Free temp ram
		RELEASEARRAY(Tags);
	}

	// Look for the best strip among the three
	u32 Longest	= Length[0];
	u32 Best		= 0;
	if(Length[1] > Longest)	{	Longest = Length[1];	Best = 1;	}
	if(Length[2] > Longest)	{	Longest = Length[2];	Best = 2;	}

	u32 NbFaces = Longest-2;

	// Update global tags
	for(u32 j=0;j<Longest-2;j++)	mTags[Faces[Best][j]] = true;

	// Flip strip if needed ("if the length of the first part of the strip is odd, the strip must be reversed")
	if(mOneSided && FirstLength[Best]&1)
	{
		// Here the strip must be flipped. I hardcoded a special case for triangles and quads.
		if(Longest==3 || Longest==4)
		{
			// Flip isolated triangle or quad
			Strip[Best][1] ^= Strip[Best][2];
			Strip[Best][2] ^= Strip[Best][1];
			Strip[Best][1] ^= Strip[Best][2];
		}
		else
		{
			// "to reverse the strip, write it in reverse order"
			for(u32 j=0;j<Longest/2;j++)
			{
				Strip[Best][j]				^= Strip[Best][Longest-j-1];
				Strip[Best][Longest-j-1]	^= Strip[Best][j];
				Strip[Best][j]				^= Strip[Best][Longest-j-1];
			}

			// "If the position of the original face in this new reversed strip is odd, you're done"
			u32 NewPos = Longest-FirstLength[Best];
			if(NewPos&1)
			{
				// "Else replicate the first index"
				for(u32 j=0;j<Longest;j++)	Strip[Best][Longest-j] = Strip[Best][Longest-j-1];
				Longest++;
			}
		}
	}

	// Copy best strip in the strip buffers
	for(u32 j=0;j<Longest;j++)
	{
		u32 Ref = Strip[Best][j];
		if(mAskForWords)	mStripRuns->Store((u16)Ref);	// Saves word reference
		else				mStripRuns->Store(Ref);			// Saves dword reference
	}
	mStripLengths->Store(Longest);

	// Free local ram
	for(u32 j=0;j<3;j++)
	{
		RELEASEARRAY(Faces[j]);
		RELEASEARRAY(Strip[j]);
	}

	// Returns #faces involved in the strip
	return NbFaces;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A method to extend a strip in a given direction, starting from a given face
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	face,				the starting face
//				oldest, middle,		the two first indices of the strip == a starting edge == a direction
// Output	:	strip,				a buffer to store the strip
//				faces,				a buffer to store the faces of the strip
//				tags,				a buffer to mark the visited faces
// Return	:	u32,				the strip length
// Exception:	-
// Remark	:	-
u32 Striper::TrackStrip(u32 face, u32 oldest, u32 middle, u32* strip, u32* faces, bool* tags)
{
	u32 Length = 2;														// Initial length is 2 since we have 2 indices in input
	strip[0] = oldest;														// First index of the strip
	strip[1] = middle;														// Second index of the strip

	bool DoTheStrip = true;
	while(DoTheStrip)
	{
		u32 Newest = mAdj->mFaces[face].OppositeVertex(oldest, middle);	// Get the third index of a face given two of them
		strip[Length++] = Newest;											// Extend the strip,...
		*faces++ = face;													// ...keep track of the face,...
		tags[face] = true;													// ...and mark it as "done".

		u8 CurEdge = mAdj->mFaces[face].FindEdge(middle, Newest);		// Get the edge ID...

		u32 Link = mAdj->mFaces[face].ATri[CurEdge];						// ...and use it to catch the link to adjacent face.
		if(IS_BOUNDARY(Link))	DoTheStrip = false;							// If the face is no more connected, we're done...
		else
		{
			face = MAKE_ADJ_TRI(Link);										// ...else the link gives us the new face index.
			if(tags[face])	DoTheStrip=false;								// Is the new face already done?
		}
		oldest = middle;													// Shift the indices and wrap
		middle = Newest;
	}
	return Length;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A method to link all strips in a single one.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	result,		the result structure
// Output	:	the result structure is updated
// Return	:	true if success
// Exception:	-
// Remark	:	-
bool Striper::ConnectAllStrips(STRIPERRESULT& result)
{
	mSingleStrip = new CustomArray;
	if(!mSingleStrip) return false;

	mTotalLength	= 0;
	u16* wrefs	= mAskForWords ? (u16*)result.StripRuns : 0;
	u32* drefs	= mAskForWords ? 0 : (u32*)result.StripRuns;

	// Loop over strips and link them together
	for(u32 k=0;k<result.NbStrips;k++)
	{
		// Nothing to do for the first strip, we just copy it
		if(k)
		{
			// This is not the first strip, so we must copy two void vertices between the linked strips
			u32 LastRef	= drefs ? drefs[-1] : (u32)wrefs[-1];
			u32 FirstRef	= drefs ? drefs[0] : (u32)wrefs[0];
			if(mAskForWords)	mSingleStrip->Store((u16)LastRef).Store((u16)FirstRef);
			else				mSingleStrip->Store(LastRef).Store(FirstRef);
			mTotalLength += 2;

			// Linking two strips may flip their culling. If the user asked for single-sided strips we must fix that
			if(mOneSided)
			{
				// Culling has been inverted only if mTotalLength is odd
				if(mTotalLength&1)
				{
					// We can fix culling by replicating the first vertex once again...
					u32 SecondRef = drefs ? drefs[1] : (u32)wrefs[1];
					if(FirstRef!=SecondRef)
					{
						if(mAskForWords)	mSingleStrip->Store((u16)FirstRef);
						else				mSingleStrip->Store(FirstRef);
						mTotalLength++;
					}
					else
					{
						// ...but if flipped strip already begin with a replicated vertex, we just can skip it.
						result.StripLengths[k]--;
						if(wrefs)	wrefs++;
						if(drefs)	drefs++;
					}
				}
			}
		}

		// Copy strip
		for(u32 j=0;j<result.StripLengths[k];j++)
		{
			u32 Ref = drefs ? drefs[j] : (u32)wrefs[j];
			if(mAskForWords)	mSingleStrip->Store((u16)Ref);
			else				mSingleStrip->Store(Ref);
		}
		if(wrefs)	wrefs += result.StripLengths[k];
		if(drefs)	drefs += result.StripLengths[k];
		mTotalLength += result.StripLengths[k];
	}

	// Update result structure
	result.NbStrips		= 1;
	result.StripRuns	= mSingleStrip->Collapse();
	result.StripLengths	= &mTotalLength;

	return true;
}
