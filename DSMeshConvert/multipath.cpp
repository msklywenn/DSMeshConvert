#include "types.h"
#include <assert.h>
#include <vector>
#include <map>
#include "List.h"
#include "opengl.h"

#define MAX_STRIPS 1024

float* vertices;

struct Face
{
	u32 idx[3];

	u32 connectivity;
	Face* adj[3];

	u32 dualConnectivity;
	bool dual[3];

	enum Set
	{
		UNCONNECTED,
		CONNECTED,
		FULLY_CONNECTED
	};
	Set set;

	s32 strip;

	LIST_LINK(Face) dualLink;
	LIST_LINK(Face) stripLink;

	Face() : connectivity(0), dualConnectivity(0), set(UNCONNECTED), strip(-1) { adj[0] = adj[1] = adj[2] = 0; dual[0] = dual[1] = dual[2] = false; }

	void RemoveFromDual(Face* f)
	{
		assert(dualConnectivity > 0);
		dualConnectivity--;
		for ( u32 i = 0 ; i < connectivity ; i++ )
		{
			if ( adj[i] == f )
			{
				dual[i] = false;
				return;
			}
		}
		assert(0 && "face is not adjacent in dual graph...");
	}

	bool IsAdjacent(Face* f)
	{
		for ( u32 i = 0 ; i < connectivity ; i++ )
			if ( dual[i] && f == adj[i] )
				return true;
		return false;
	}
};

void Disconnect(Face* f, Face* except, LIST_DECLARE(Face, dualLink) unconnected[4], LIST_DECLARE(Face, dualLink) connected[2])
{
	for ( u32 i = 0 ; i < f->connectivity ; i++ )
	{
		if ( f->dual[i] && f->adj[i] != except )
		{
			f->adj[i]->RemoveFromDual(f);
			f->adj[i]->dualLink.Unlink();
			if ( f->adj[i]->set == Face::UNCONNECTED )
			{
				unconnected[f->adj[i]->dualConnectivity].InsertTail(f->adj[i]);
			}
			else if ( f->adj[i]->set == Face::CONNECTED )
			{
				u32 c = f->adj[i]->dualConnectivity - 1;
				assert(c < 2);
				connected[c].InsertTail(f->adj[i]);
			}
			f->dual[i] = false;
		}
	}
	f->dualConnectivity = 0;
	f->set = Face::FULLY_CONNECTED;
}

template <typename T>
u32 Length(T& unconnected)
{
	u32 len = 0;
	Face* f = unconnected.Head();
	while ( f != 0 )
	{
		len++;
		f = f->dualLink.Next();
	}
	return len;
}

bool ShareEdge(Face* a, Face* b)
{
	int edge0 = -1, edge1 = -1;
	for ( u32 i = 0 ; i < 3 ; i++ )
	{
		for ( u32 j = 0 ; j < 3 ; j++ )
		{
			if ( a->idx[j] == b->idx[i] )
			{
				if ( edge0 == -1 && edge0 != a->idx[j] )
					edge0 = a->idx[j];
				else if ( edge1 == -1 && edge1 != a->idx[j] )
					edge1 = a->idx[j];
			}
		}
	}
	return edge0 != -1 && edge1 != -1;
}

void DrawFace(const Face& face)
{
	glVertex3f(vertices[face.idx[0] * 3], vertices[face.idx[0] * 3 + 1], vertices[face.idx[0] * 3 + 2]);
	glVertex3f(vertices[face.idx[1] * 3], vertices[face.idx[1] * 3 + 1], vertices[face.idx[1] * 3 + 2]);
	glVertex3f(vertices[face.idx[2] * 3], vertices[face.idx[2] * 3 + 1], vertices[face.idx[2] * 3 + 2]);
}

void DrawBorder(const Face& face)
{
	float v[3][3];
	for ( u32 i = 0 ; i < 3 ; i++ )
	{
		v[i][0] = vertices[face.idx[i] * 3];
		v[i][1] = vertices[face.idx[i] * 3 + 1];
		v[i][2] = vertices[face.idx[i] * 3 + 2];
	}

	float cx = (v[0][0] + v[1][0] + v[2][0]) / 3.f;
	float cy = (v[0][1] + v[1][1] + v[2][1]) / 3.f;
	float cz = (v[0][2] + v[1][2] + v[2][2]) / 3.f;

	glBegin(GL_TRIANGLES);
	for ( u32 i = 0 ; i < 3 ; i++ )
	{
		float x, y, z;
		x = v[i][0];
		y = v[i][1];
		z = v[i][2];

		x = (x - cx) * .9f;
		y = (y - cy) * .9f;
		z = (z - cz) * .9f;

		x += cx;
		y += cy;
		z += cz;

		glVertex3f(x, y, z);
	}
	glEnd();
}

void DrawFaces(const Face* faces, u32 nbTris)
{
	glBegin(GL_TRIANGLES);
	for ( u32 i = 0 ; i < nbTris ; i++ )
	{
		float c = faces[i].dualConnectivity / 3.f;
		glColor3f(c, c, c);
		DrawFace(faces[i]);
	}
	glEnd();
}

void DrawEdge(Face* a, Face* b)
{
	float ax = (vertices[a->idx[0] * 3] + vertices[a->idx[1] * 3] + vertices[a->idx[2] * 3]) / 3.f;
	float ay = (vertices[a->idx[0] * 3 + 1] + vertices[a->idx[1] * 3 + 1] + vertices[a->idx[2] * 3 + 1]) / 3.f;
	float az = (vertices[a->idx[0] * 3 + 2] + vertices[a->idx[1] * 3 + 2] + vertices[a->idx[2] * 3 + 2]) / 3.f;
	float bx = (vertices[b->idx[0] * 3] + vertices[b->idx[1] * 3] + vertices[b->idx[2] * 3]) / 3.f;
	float by = (vertices[b->idx[0] * 3 + 1] + vertices[b->idx[1] * 3 + 1] + vertices[b->idx[2] * 3 + 1]) / 3.f;
	float bz = (vertices[b->idx[0] * 3 + 2] + vertices[b->idx[1] * 3 + 2] + vertices[b->idx[2] * 3 + 2]) / 3.f;

	glBegin(GL_LINES);
	glVertex3f(ax, ay, az);
	glVertex3f(bx, by, bz);
	glEnd();
}

void DrawStrip(LIST_DECLARE(Face, stripLink)& strip, u32 color)
{
	if ( strip.Head() == 0 )
		return;

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glBegin(GL_TRIANGLES);
	Face* f = strip.Head();
	while ( f )
	{
		ColorOpenGL(color, .5f);
		DrawFace(*f);
		f = f->stripLink.Next();
	}
	glEnd();

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	ColorOpenGL(1);
	DrawBorder(*strip.Head());
	ColorOpenGL(0);
	DrawBorder(*strip.Tail());
}

void DrawAdjacency(Face* f)
{
	for ( u32 i = 0 ; i < f->connectivity ; i++ )
	{
		if ( f->dual[i] )
		{
			DrawEdge(f, f->adj[i]);
		}
	}
}

void ShowDebug(Face* faces, u32 nbTris, Face* f0, Face* f1, LIST_DECLARE(Face, stripLink) strips[MAX_STRIPS], u32 nbStrips)
{
	while ( EndSceneOpenGL() )
	{
		BeginSceneOpenGL();

		for ( u32 i = 0 ; i < nbStrips ; i++ )
		{
			DrawStrip(strips[i], i);
		}

		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		glLineStipple(4, 0xAAAA);
		glEnable(GL_LINE_STIPPLE);
		glLineWidth(1.f);
		DrawFaces(faces, nbTris);

		if ( f0 && f1 )
		{
			glColor3f(1.f, 0.f, 0.f);
			glDisable(GL_LINE_STIPPLE);
			glLineWidth(5.f);
			DrawEdge(f0, f1);

			glLineWidth(2.f);
			glColor3f(0.f, 1.f, 0.f);
			DrawAdjacency(f0);
			glColor3f(0.f, 0.f, 1.f);
			DrawAdjacency(f1);
		}
	}
}

void MultiPathStripper(const std::vector<u32>& indices, std::vector<u32>& stripLengths, std::vector<u32>& stripIndices)
{
	u32 nbTris = indices.size() / 3;

	// build dual graph
	Face* faces = new Face[nbTris];
	u32 nbEdges = 0;
	const u32 *idx = &indices[0];
	std::map<std::pair<u32, u32>, Face*> edges;
	for ( u32 i = 0 ; i < nbTris ; i++ )
	{
		Face& f = faces[i];
		f.idx[0] = *idx++;
		f.idx[1] = *idx++;
		f.idx[2] = *idx++;

		for ( u32 v = 0 ; v < 3 ; v++ )
		{
			std::pair<u32, u32> edge(f.idx[v], f.idx[(v + 1) % 3]);
			if ( edge.first > edge.second )
				std::swap(edge.first, edge.second);
			auto iter = edges.find(edge);
			if ( iter == edges.end() )
			{
				edges[edge] = &f;
			}
			else
			{
				if ( iter->second )
				{
					f.adj[f.connectivity] = iter->second;
					f.dual[f.connectivity] = true;
					f.connectivity++;
					f.dualConnectivity++;
					iter->second->adj[iter->second->connectivity] = &f;
					iter->second->dual[iter->second->connectivity] = true;
					iter->second->connectivity++;
					iter->second->dualConnectivity++;

					assert(iter->second->connectivity <= 3);
					assert(f.connectivity <= 3);

					iter->second = 0;
					nbEdges++;
				}
				else
				{
					fprintf(stderr, "degenerate at edge(%d, %d)\n", edge.first, edge.second);
				}
			}
		}
	}

	// classify
	LIST_DECLARE(Face, dualLink) unconnected[4];
	LIST_DECLARE(Face, dualLink) connected[2];

	for ( u32 i = 0 ; i < nbTris ; i++ )
		unconnected[faces[i].connectivity].InsertTail(&faces[i]);

	assert(connected[0].Empty());
	assert(connected[1].Empty());

	u32 nbStrips = 0;
	LIST_DECLARE(Face, stripLink) strips[MAX_STRIPS];

	// stripify
	for ( u32 e = 0 ; e < nbEdges ; e++ )
	{
#ifdef _DEBUG
		u32 realNbStrips = 0;
		for ( u32 i = 0 ; i < nbStrips ; i++ )
			if ( !strips[i].Empty() )
				realNbStrips++;

		printf("[%d] strips:%d(%d) U0:%d U1:%d C1:%d U2:%d C2:%d U3:%d\n",
			e, realNbStrips, nbStrips,
			Length(unconnected[0]), Length(unconnected[1]), Length(connected[0]),
			Length(unconnected[2]), Length(connected[1]), Length(unconnected[3]));
#endif
		Face* face[2] = { 0, 0 };
		if ( !unconnected[1].Empty() )
		{
			face[0] = unconnected[1].Tail();
			assert(face[0]->dualConnectivity == 1);
		}
		else if ( !connected[0].Empty() )
		{
			face[0] = connected[0].Tail();
			assert(face[0]->dualConnectivity == 1);
		}
		else if ( !unconnected[2].Empty() )
		{
			face[0] = unconnected[2].Tail();
			assert(face[0]->dualConnectivity == 2);
		}
		else if ( !connected[1].Empty() )
		{
			face[0] = connected[1].Tail();
			assert(face[0]->dualConnectivity == 2);
		}
		else if ( !unconnected[3].Empty() )
		{
			face[0] = unconnected[3].Tail();
			assert(face[0]->dualConnectivity == 3);
		}
		else
		{
			break;
		}
		assert(face[0]);
		assert(face[0]->dualConnectivity > 0);
		face[0]->dualLink.Unlink();

		for ( u32 i = 0 ; face[1] == 0 && i < face[0]->connectivity ; i++ )
		{
			if ( face[0]->dual[i] )
				face[1] = face[0]->adj[i];
		}
		assert(face[1]);
		face[1]->dualLink.Unlink();

		ShowDebug(faces, nbTris, face[0], face[1], strips, nbStrips);

		assert(face[0]->set != Face::FULLY_CONNECTED);
		assert(face[1]->set != Face::FULLY_CONNECTED);

		// attempt to concatenate with an existing strip
		s32 stripID = -1;
		if ( face[0]->set == Face::CONNECTED && face[1]->set == Face::CONNECTED )
		{
			if ( face[0]->strip != face[1]->strip )
			{
				LIST_DECLARE(Face, stripLink)& strip0 = strips[face[0]->strip];
				LIST_DECLARE(Face, stripLink)& strip1 = strips[face[1]->strip];
				Face* strip0head = strip0.Head();
				Face* strip0tail = strip0.Tail();
				Face* strip1head = strip1.Head();
				Face* strip1tail = strip1.Tail();
				if ( strip0.Head()->IsAdjacent(strip1.Tail()) )
				{
					Face* f = strip1.Tail();
					while ( f )
					{
						Face* next = f->stripLink.Prev();
						f->stripLink.Unlink();
						assert(ShareEdge(f, strip0.Head()));
						strip0.InsertHead(f);
						f->strip = face[0]->strip;
						f = next;
					}
					stripID = face[0]->strip;
				}
				else if ( strip1.Head()->IsAdjacent(strip0.Tail()) )
				{
					Face* f = strip0.Tail();
					while ( f )
					{
						Face* next = f->stripLink.Prev();
						f->stripLink.Unlink();
						assert(ShareEdge(f, strip1.Head()));
						strip1.InsertHead(f);
						f->strip = face[1]->strip;
						f = next;
					}
					stripID = face[1]->strip;
				}
				else if ( strip1.Tail()->IsAdjacent(strip0.Tail()) )
				{
					Face* f = strip1.Tail();
					while ( f )
					{
						Face* next = f->stripLink.Prev();
						f->stripLink.Unlink();
						assert(ShareEdge(f, strip0.Tail()));
						strip0.InsertTail(f);
						f->strip = strip0.Head()->strip;
						f = next;
					}
					stripID = strip0.Head()->strip;
				}
				else if ( strip1.Head()->IsAdjacent(strip0.Head()) )
				{
					Face* f = strip1.Head();
					while ( f )
					{
						Face* next = f->stripLink.Next();
						f->stripLink.Unlink();
						assert(ShareEdge(f, strip0.Head()));
						strip0.InsertHead(f);
						f->strip = strip0.Tail()->strip;
						f = next;
					}
					stripID = strip0.Head()->strip;
				}
			}
			else
			{
				stripID = face[0]->strip;
			}
		}
		else if ( face[0]->set == Face::CONNECTED )
		{
			if ( face[1]->set != Face::CONNECTED )
			{
				LIST_DECLARE(Face, stripLink)& strip = strips[face[0]->strip];
				if ( face[0]->stripLink.Next() == 0 )
				{
					Face* tail = strip.Tail();
					assert(ShareEdge(face[1], tail));
					strip.InsertTail(face[1]);
					stripID = face[0]->strip;
				}
				else if ( face[0]->stripLink.Prev() == 0 )
				{
					assert(ShareEdge(face[1], strip.Head()));
					strip.InsertHead(face[1]);
					stripID = face[0]->strip;
				}
			}
		}
		else if ( face[1]->set == Face::CONNECTED )
		{
			if ( face[0]->set != Face::CONNECTED )
			{
				LIST_DECLARE(Face, stripLink)& strip = strips[face[1]->strip];
				if ( face[1]->stripLink.Next() == 0 )
				{
					assert(ShareEdge(face[0], strip.Tail()));
					strip.InsertTail(face[0]);
					stripID = face[1]->strip;
				}
				else if ( face[1]->stripLink.Prev() == 0 )
				{
					assert(ShareEdge(face[0], strip.Head()));
					strip.InsertHead(face[0]);
					stripID = face[1]->strip;
				}
			}
		}
		else
		{
			assert(face[0]->set != Face::FULLY_CONNECTED);
			assert(face[1]->set != Face::FULLY_CONNECTED);
			for ( u32 s = 0 ; stripID == -1 && s < nbStrips ; s++ )
			{
				LIST_DECLARE(Face, stripLink)& tmp = strips[s];

				if ( tmp.Empty() )
					continue;

				Face* head = tmp.Head();
				assert(head);
				assert(ShareEdge(head, head->stripLink.Next()));
				for ( u32 f = 0 ; stripID == -1 && f < head->connectivity ; f++ )
				{
					if ( head->dual[f] )
					{
						if ( head->adj[f] == face[0] )
						{
							if ( !face[0]->stripLink.IsLinked() )
							{
								assert(ShareEdge(face[0], tmp.Head()));
								tmp.InsertHead(face[0]);
							}
							if ( !face[1]->stripLink.IsLinked() && head != face[1] && tmp.Tail() != face[1] )
							{
								assert(ShareEdge(face[1], tmp.Head()));
								tmp.InsertHead(face[1]);
							}
							stripID = s;
						}
						else if ( head->adj[f] == face[1] )
						{
							if ( !face[1]->stripLink.IsLinked() )
							{
								assert(ShareEdge(face[1], tmp.Head()));
								tmp.InsertHead(face[1]);
							}
							if ( !face[0]->stripLink.IsLinked() && head != face[0] && tmp.Tail() != face[0] )
							{
								assert(ShareEdge(face[0], tmp.Head()));
								tmp.InsertHead(face[0]);
							}
							stripID = s;
						}
					}
				}
				if ( stripID != -1 )
					break;
				Face* tail = tmp.Tail();
				assert(tail);
				assert(ShareEdge(tail, tail->stripLink.Prev()));
				for ( u32 f = 0 ; stripID == -1 && f < tail->connectivity ; f++ )
				{
					if ( tail->dual[f] )
					{
						if ( tail->adj[f] == face[0] )
						{
							if ( !face[0]->stripLink.IsLinked() )
							{
								assert(ShareEdge(face[0], tmp.Tail()));
								tmp.InsertTail(face[0]);
							}
							if ( !face[1]->stripLink.IsLinked() && tail != face[1] && tmp.Head() != face[1] )
							{
								assert(ShareEdge(face[1], tmp.Tail()));
								tmp.InsertTail(face[1]);
							}
							stripID = s;
						}
						else if ( tail->adj[f] == face[1] )
						{
							if ( !face[1]->stripLink.IsLinked() )
							{
								assert(ShareEdge(face[1], tmp.Tail()));
								tmp.InsertTail(face[1]);
							}
							if ( !face[0]->stripLink.IsLinked() && tail != face[0] && tmp.Head() != face[0] )
							{
								assert(ShareEdge(face[0], tmp.Tail()));
								tmp.InsertTail(face[0]);
							}
							stripID = s;
						}
					}
				}
			}
			if ( stripID == -1 )
			{
				assert(nbStrips < MAX_STRIPS);
				stripID = nbStrips;
				LIST_DECLARE(Face, stripLink)* strip = &strips[stripID];
				nbStrips++;
				assert(face[0] && face[1]);
				assert(!face[0]->stripLink.IsLinked());
				assert(!face[1]->stripLink.IsLinked());
				assert(ShareEdge(face[0], face[1]));
				strip->InsertHead(face[0]);
				strip->InsertTail(face[1]);
				assert(strip->Head());
				assert(strip->Tail());
			}
		}

		if ( stripID != -1 )
		{
			for ( u32 i = 0 ; i < 2 ; i++ )
			{
				if ( face[i]->set != Face::UNCONNECTED )
				{
					assert(face[i]->strip == stripID);
				}
				else
				{
					assert(face[i]->strip == -1);
					face[i]->strip = stripID;
				}
			}
		}

		for ( u32 f = 0 ; f < 2 ; f++ )
		{
			if ( face[f]->strip != -1 )
			{
				if ( face[f]->set == Face::UNCONNECTED )
				{
					face[f]->set = Face::CONNECTED;
					if ( face[f]->dualConnectivity >= 2 )
					{
						u32 c = face[f]->dualConnectivity - 2;
						connected[c].InsertTail(face[f]);
					}
					face[f]->RemoveFromDual(face[(f + 1) % 2]);
				}
				else if ( face[f]->set == Face::CONNECTED )
				{
					Disconnect(face[f], face[(f + 1) % 2], unconnected, connected);
				}
				else
				{
					assert(0 && "disconnecting a fully connected node !");
				}
			}
		}
	}

	assert(unconnected[1].Empty());
	assert(unconnected[2].Empty());
	assert(unconnected[3].Empty());
	assert(connected[0].Empty());
	assert(connected[1].Empty());

	while ( !unconnected[0].Empty() )
	{
		Face* f = unconnected[0].Tail();
		f->dualLink.Unlink();

		assert(nbStrips < MAX_STRIPS);
		LIST_DECLARE(Face, stripLink)& strip = strips[nbStrips];
		nbStrips++;
		strip.InsertHead(f);
	}

	ShowDebug(faces, nbTris, 0, 0, strips, nbStrips);

	delete[] faces;
}