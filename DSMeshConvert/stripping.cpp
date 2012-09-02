#include "stripping.h"
#include <map>
#include <set>
#define AI_WONT_RETURN
#include <aiAssert.h>

u32 GetNodeFromSet(std::set<u32> (&unconnected)[4], std::set<u32> (&connected)[2], u32& settype, u32& setid)
{
	u32 node;

	if ( unconnected[0].size() > 0 )
	{
		node = *unconnected[0].begin();
		unconnected[0].erase(node);
		settype = 0;
		setid = 0;
	}
	else if ( unconnected[1].size() > 0 )
	{
		node = *unconnected[1].begin();
		unconnected[1].erase(node);
		settype = 0;
		setid = 1;
	}
	else if ( connected[0].size() > 0 )
	{
		node = *connected[1].begin();
		connected[1].erase(node);
		settype = 1;
		setid = 0;
	}
	else if ( unconnected[2].size() > 0 )
	{
		node = *unconnected[2].begin();
		unconnected[2].erase(node);
		settype = 0;
		setid = 2;
	}
	else if ( connected[1].size() > 0 )
	{
		node = *connected[2].begin();
		connected[0].erase(node);
		settype = 1;
		setid = 1;
	}
	else 
	{
		node = *unconnected[3].begin();
		unconnected[3].erase(node);
		settype = 0;
		setid = 3;
	}

	return node;
}

void Find(u32 node, std::set<u32> (&unconnected)[4], std::set<u32> (&connected)[2], u32& settype, u32& setid)
{
	for ( u32 i = 0 ; i < 4 ; i++ )
	{
		std::set<u32>::iterator it = unconnected[i].find(node);
		if ( it != unconnected[i].end() )
		{
			settype = 0;
			setid = i;
			return;
		}
	}

	for ( u32 i = 0 ; i < 2 ; i++ )
	{
		std::set<u32>::iterator it = connected[i].find(node);
		if ( it != connected[i].end() )
		{
			settype = 1;
			setid = i;
			return;
		}
	}
}

void MoveNodeToSet(u32 node, u32 nodeSetID, u32 nodeSetType, std::set<u32> (&connected)[2], std::set<u32> &fullyConnected)
{
	if ( nodeSetType == 0 && nodeSetID > 0 )
	{
		if ( nodeSetID == 1 )
		{
			connected[0].insert(node);
		}
		else
		{
			connected[nodeSetID - 2].insert(node);
		}
	}
	else
	{
		fullyConnected.insert(node);
	}
}

// Multi-Path Algorithm for Triangle Strips
// Petr Vanecek, Ivana Kolingerova
// From the draft of September 16, 2004
std::vector<u32> BuildTriangleStrips(std::vector<u32> indices)
{
	u32 nbTriangles = indices.size() / 3;

	// build edge map
	std::map<std::pair<u32, u32>, std::vector<u32> > connections;
	u32 triangle = 0;
	for ( u32 i = 0 ; i < indices.size() ; i += 3, triangle++ )
	{
		for ( u32 j = 0 ; j < 3 ; j++ )
		{
			static u32 a[] = { 0, 0, 1 };
			static u32 b[] = { 1, 2, 2 };
			std::pair<u32, u32> edge(indices[i + a[j]], indices[i + b[j]]);
			if ( edge.first > edge.second )
			{
				std::swap(edge.first, edge.second);
			}

			std::map<std::pair<u32, u32>, std::vector<u32> >::iterator it =	connections.find(edge);
			if ( it == connections.end() )
			{
				std::pair<std::pair<u32, u32>, std::vector<u32> > entry;
				entry.first = edge;
				entry.second.push_back(triangle);
				connections.insert(entry);
			}
			else
			{
				it->second.push_back(triangle);
			}
		}
	}

	// build dual graph
	std::vector<std::set<u32> > graph(nbTriangles);
	for ( u32 i = 0, triangle = 0 ; i < indices.size() ; i += 3, triangle++ )
	{
		for ( u32 j = 0 ; j < 3 ; j++ )
		{
			static u32 a[] = { 0, 0, 1 };
			static u32 b[] = { 1, 2, 2 };
			std::pair<u32, u32> edge(indices[i + a[j]], indices[i + b[j]]);
			if ( edge.first > edge.second )
			{
				std::swap(edge.first, edge.second);
			}

			std::map<std::pair<u32, u32>, std::vector<u32> >::iterator it =	connections.find(edge);
			if ( it != connections.end() )
			{
				graph[triangle].insert(it->second.begin(), it->second.end());
			}
		}
		graph[triangle].erase(triangle);
	}

	// build sets
	std::set<u32> unconnected[4];
	for ( u32 i = 0 ; i < nbTriangles ; i++ )
	{
		ai_assert(graph[i].size() < 4);
		unconnected[graph[i].size()].insert(i);
	}

	std::set<u32> connected[2];
	std::set<u32> fullyConnected;

	while ( fullyConnected.size() < nbTriangles )
	{
		u32 nodeSetID, nodeSetType;
		u32 node = GetNodeFromSet(unconnected, connected, nodeSetType, nodeSetID);
		u32 node2 = *graph[node].begin();
		u32 node2SetID, node2SetType;
		Find(node2, unconnected, connected, node2SetType, node2SetID);
		if ( node2SetType == 0 )
		{
			unconnected[node2SetID].erase(node2);
		}
		else
		{
			connected[node2SetID].erase(node2);
		}

		// Create strip
		std::vector<u32> strip;
		strip.push_back(node);
		strip.push_back(node2);

		// Remove edge from graph
		graph[node].erase(node2);
		graph[node2].erase(node);

		//strip = ConcatenateStrips(strip);

		MoveNodeToSet(node, nodeSetID, nodeSetType, connected, fullyConnected);
		MoveNodeToSet(node2, node2SetID, node2SetType, connected, fullyConnected);

		//UpdateNeighbors(node);
		//UpdateNeighbors(node2);

		//RemoveLoop(strip);

		//UpdateNeighbors(*strip.begin());
		if ( strip.begin() != strip.end() )
		{
			//UpdateNeighbors(*(strip.end() - 1));
		}
	}

	std::vector<u32> strips;
	return strips;
}