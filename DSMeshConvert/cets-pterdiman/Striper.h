///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Source code for "Creating Efficient Triangle Strips"
// (C) 2000, Pierre Terdiman (p.terdiman@wanadoo.fr)
//
// Version is 2.0.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __STRIPER_H__
#define __STRIPER_H__

#include "Adjacency.h"
#include "CustomArray.h"
#include "../types.h"

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//
	//																Class Striper
	//
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	struct STRIPERCREATE{
				STRIPERCREATE()
				{
					DFaces				= 0;
					WFaces				= 0;
					NbFaces				= 0;
					AskForWords			= true;
					OneSided			= true;
					SGIAlgorithm		= true;
					ConnectAllStrips	= false;
				}
				u32					NbFaces;			// #faces in source topo
				u32*					DFaces;				// list of faces (dwords) or 0
				u16*					WFaces;				// list of faces (words) or 0
				bool					AskForWords;		// true => results are in words (else dwords)
				bool					OneSided;			// true => create one-sided strips
				bool					SGIAlgorithm;		// true => use the SGI algorithm, pick least connected faces first
				bool					ConnectAllStrips;	// true => create a single strip with void faces
	};

	struct STRIPERRESULT{
				u32					NbStrips;			// #strips created
				u32*					StripLengths;		// Lengths of the strips (NbStrips values)
				void*					StripRuns;			// The strips in words or dwords, depends on AskForWords
				bool					AskForWords;		// true => results are in words (else dwords)
	};

	class Striper
	{
	private:
				Striper&				FreeUsedRam();
				u32					ComputeBestStrip(u32 face);
				u32					TrackStrip(u32 face, u32 oldest, u32 middle, u32* strip, u32* faces, bool* tags);
				bool					ConnectAllStrips(STRIPERRESULT& result);

				Adjacencies*			mAdj;				// Adjacency structures
				bool*					mTags;				// Face markers

				u32					mNbStrips;			// The number of strips created for the mesh
				CustomArray*			mStripLengths;		// Array to store strip lengths
				CustomArray*			mStripRuns;			// Array to store strip indices

				u32					mTotalLength;		// The length of the single strip
				CustomArray*			mSingleStrip;		// Array to store the single strip

				// Flags
				bool					mAskForWords;
				bool					mOneSided;
				bool					mSGIAlgorithm;
				bool					mConnectAllStrips;

	public:
				Striper();
				~Striper();

				bool					Init(STRIPERCREATE& create);
				bool					Compute(STRIPERRESULT& result);
	};

#endif // __STRIPER_H__
