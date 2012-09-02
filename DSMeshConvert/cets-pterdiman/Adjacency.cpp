///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Source code for "Creating Efficient Triangle Strips"
// (C) 2000, Pierre Terdiman (p.terdiman@wanadoo.fr)
//
// Version is 2.0.
//
// Following adjacency structures have been successfully used to implement:
//				- a triangle striper
//				- a silhouette tracker [used for shadow volumes, occlusion culling, and glow!]
//				- local search for collision detection on terrains
//				- subdivision surfaces
//
//	So that one IS useful....
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Precompiled Header
//#include "Stdafx.h"
#include "Adjacency.h"
#include "RevisitedRadix.h"
#define RELEASEARRAY(x) { if ( x ) delete[] (x); }
#define RELEASE(x) { if ( x ) delete (x); }
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//																	Adjacencies Class Implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Adjacencies::Adjacencies() : mNbEdges(0), mCurrentNbFaces(0), mEdges(0), mNbFaces(0), mFaces(0)
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Destructor
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Adjacencies::~Adjacencies()
{
	RELEASEARRAY(mEdges);
	RELEASEARRAY(mFaces);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A method to initialize the component
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	create,		the creation structure
// Output	:	-
// Return	:	true if success
// Exception:	-
// Remark	:	-
bool Adjacencies::Init(ADJACENCIESCREATE& create)
{
	// Get some bytes
	mNbFaces	= create.NbFaces;
	mFaces		= new AdjTriangle[mNbFaces];	if(!mFaces)	return false;
	mEdges		= new AdjEdge[mNbFaces*3];		if(!mEdges)	return false;

	// Feed me with triangles.....
	for(u32 i=0;i<mNbFaces;i++)
	{
		// Get correct vertex references
		u32 Ref0 = create.DFaces ? create.DFaces[i*3+0] : create.WFaces ? create.WFaces[i*3+0] : 0;
		u32 Ref1 = create.DFaces ? create.DFaces[i*3+1] : create.WFaces ? create.WFaces[i*3+1] : 1;
		u32 Ref2 = create.DFaces ? create.DFaces[i*3+2] : create.WFaces ? create.WFaces[i*3+2] : 2;

		// Add a triangle to the database
		AddTriangle(Ref0, Ref1, Ref2);
	}

	// At this point of the process we have mFaces & mEdges filled with input data. That is:
	// - a list of triangles with 3 0 links (i.e. -1)
	// - a list of mNbFaces*3 edges, each edge having 2 vertex references and an owner face.

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A method to add a new triangle to the database
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	ref0, ref1, ref2,		vertex references for the new triangle
// Output	:	-
// Return	:	true if success
// Exception:	-
// Remark	:	-
bool Adjacencies::AddTriangle(u32 ref0, u32 ref1, u32 ref2)
{
	// Store vertex-references
	mFaces[mCurrentNbFaces].VRef[0]	= ref0;
	mFaces[mCurrentNbFaces].VRef[1]	= ref1;
	mFaces[mCurrentNbFaces].VRef[2]	= ref2;

	// Reset links
	mFaces[mCurrentNbFaces].ATri[0]	= -1;
	mFaces[mCurrentNbFaces].ATri[1]	= -1;
	mFaces[mCurrentNbFaces].ATri[2]	= -1;

	// Add edge 01 to database
	if(ref0<ref1)	AddEdge(ref0, ref1, mCurrentNbFaces);
	else			AddEdge(ref1, ref0, mCurrentNbFaces);
	// Add edge 02 to database
	if(ref0<ref2)	AddEdge(ref0, ref2, mCurrentNbFaces);
	else			AddEdge(ref2, ref0, mCurrentNbFaces);
	// Add edge 12 to database
	if(ref1<ref2)	AddEdge(ref1, ref2, mCurrentNbFaces);
	else			AddEdge(ref2, ref1, mCurrentNbFaces);

	mCurrentNbFaces++;

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A method to add a new edge to the database
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	ref0, ref1,		vertex references for the new edge
//				face,			owner face
// Output	:	-
// Return	:	true if success
// Exception:	-
// Remark	:	-
bool Adjacencies::AddEdge(u32 ref0, u32 ref1, u32 face)
{
	// Store edge data
	mEdges[mNbEdges].Ref0	= ref0;
	mEdges[mNbEdges].Ref1	= ref1;
	mEdges[mNbEdges].FaceNb	= face;
	mNbEdges++;
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A method to create the adjacency structures
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	-
// Output	:	-
// Return	:	true if success
// Exception:	-
// Remark	:	-
bool Adjacencies::CreateDatabase()
{
	// Here mNbEdges should be equal to mCurrentNbFaces*3.

	RadixSorter Core;

	u32* FaceNb = new u32[mNbEdges];	if(!FaceNb)	return false;
	u32* VRefs0 = new u32[mNbEdges];	if(!VRefs0)	return false;
	u32* VRefs1 = new u32[mNbEdges];	if(!VRefs1)	return false;

	for(u32 i=0;i<mNbEdges;i++)
	{
		FaceNb[i] = mEdges[i].FaceNb;
		VRefs0[i] = mEdges[i].Ref0;
		VRefs1[i] = mEdges[i].Ref1;
	}

	// Multiple sort
	u32* Sorted = Core.Sort(FaceNb, mNbEdges).Sort(VRefs0, mNbEdges).Sort(VRefs1, mNbEdges).GetIndices();

	// Read the list in sorted order, look for similar edges
	u32 LastRef0 = VRefs0[Sorted[0]];
	u32 LastRef1 = VRefs1[Sorted[0]];
	u32 Count = 0;
	u32 TmpBuffer[3];

	for(u32 i=0;i<mNbEdges;i++)
	{
		u32 Face = FaceNb[Sorted[i]];		// Owner face
		u32 Ref0 = VRefs0[Sorted[i]];		// Vertex ref #1
		u32 Ref1 = VRefs1[Sorted[i]];		// Vertex ref #2
		if(Ref0==LastRef0 && Ref1==LastRef1)
		{
			// Current edge is the same as last one
			TmpBuffer[Count++] = Face;				// Store face number
			if(Count==3)
			{
				RELEASEARRAY(VRefs1);
				RELEASEARRAY(VRefs0);
				RELEASEARRAY(FaceNb);
				return false;				// Only works with manifold meshes (i.e. an edge is not shared by more than 2 triangles)
			}
		}
		else
		{
			// Here we have a new edge (LastRef0, LastRef1) shared by Count triangles stored in TmpBuffer
			if(Count==2)
			{
				// if Count==1 => edge is a boundary edge: it belongs to a single triangle.
				// Hence there's no need to update a link to an adjacent triangle.
				bool Status = UpdateLink(TmpBuffer[0], TmpBuffer[1], LastRef0, LastRef1);
				if(!Status)
				{
					RELEASEARRAY(VRefs1);
					RELEASEARRAY(VRefs0);
					RELEASEARRAY(FaceNb);
					return Status;
				}
			}
			// Reset for next edge
			Count = 0;
			TmpBuffer[Count++] = Face;
			LastRef0 = Ref0;
			LastRef1 = Ref1;
		}
	}
	bool Status = true;
	if(Count==2)	Status = UpdateLink(TmpBuffer[0], TmpBuffer[1], LastRef0, LastRef1);

	RELEASEARRAY(VRefs1);
	RELEASEARRAY(VRefs0);
	RELEASEARRAY(FaceNb);

	// We don't need the edges anymore
	RELEASEARRAY(mEdges);

	return Status;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A method to update the links in two adjacent triangles
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	firsttri,		index of the first triangle
//				secondtri,		index of the second triangle
//				ref0, ref1,		the common edge
// Output	:	-
// Return	:	true if success
// Exception:	-
// Remark	:	-
bool Adjacencies::UpdateLink(u32 firsttri, u32 secondtri, u32 ref0, u32 ref1)
{
	AdjTriangle* Tri0 = &mFaces[firsttri];		// Catch the first triangle
	AdjTriangle* Tri1 = &mFaces[secondtri];		// Catch the second triangle

	// Get the edge IDs. 0xff means input references are wrong.
	u8 EdgeNb0 = Tri0->FindEdge(ref0, ref1);		if(EdgeNb0==0xff)	return false;
	u8 EdgeNb1 = Tri1->FindEdge(ref0, ref1);		if(EdgeNb1==0xff)	return false;

	// Update links. The two most significant bits contain the counterpart edge's ID.
	Tri0->ATri[EdgeNb0] = secondtri	|(u32(EdgeNb1)<<30);
	Tri1->ATri[EdgeNb1] = firsttri	|(u32(EdgeNb0)<<30);

	return true;
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A method to find an edge in a tri, given two vertex references
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	vref0, vref1,	the two vertex references
// Output	:	-
// Return	:	the edge number between 0 and 2, or 0xff if input refs are wrong
// Exception:	-
// Remark	:	-
u8 AdjTriangle::FindEdge(u32 vref0, u32 vref1)
{
	u8 EdgeNb = 0xff;
			if(VRef[0]==vref0 && VRef[1]==vref1)	EdgeNb = 0;
	else	if(VRef[0]==vref1 && VRef[1]==vref0)	EdgeNb = 0;
	else	if(VRef[0]==vref0 && VRef[2]==vref1)	EdgeNb = 1;
	else	if(VRef[0]==vref1 && VRef[2]==vref0)	EdgeNb = 1;
	else	if(VRef[1]==vref0 && VRef[2]==vref1)	EdgeNb = 2;
	else	if(VRef[1]==vref1 && VRef[2]==vref0)	EdgeNb = 2;
	return EdgeNb;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A method to get the last reference given the first two
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Input	:	vref0, vref1,	the two vertex references
// Output	:	-
// Return	:	the last reference, or 0xffffffff if input refs are wrong
// Exception:	-
// Remark	:	-
u32 AdjTriangle::OppositeVertex(u32 vref0, u32 vref1)
{
	u32 Ref = 0xffffffff;
			if(VRef[0]==vref0 && VRef[1]==vref1)	Ref = VRef[2];
	else	if(VRef[0]==vref1 && VRef[1]==vref0)	Ref = VRef[2];
	else	if(VRef[0]==vref0 && VRef[2]==vref1)	Ref = VRef[1];
	else	if(VRef[0]==vref1 && VRef[2]==vref0)	Ref = VRef[1];
	else	if(VRef[1]==vref0 && VRef[2]==vref1)	Ref = VRef[0];
	else	if(VRef[1]==vref1 && VRef[2]==vref0)	Ref = VRef[0];
	return Ref;
}
