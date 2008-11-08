/*
 * Copyright (c) 2008, Willow Garage, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <topological_map/bottleneck_graph.h>
#include <iostream>
#include <algorithm>
#include <boost/graph/breadth_first_search.hpp>     

using std::cout;
using std::endl;


namespace topological_map 
{

// Typedefs for graphs
struct coords_t // The property tag
{
  typedef boost::vertex_property_tag kind;
};
typedef boost::property<coords_t,Coords> coords_property; // The property
typedef boost::adjacency_list<boost::listS, boost::listS, boost::undirectedS, boost::property<boost::vertex_index_t, int, coords_property> > Graph; 
typedef boost::property_map<Graph, coords_t>::type CoordsMap; // The property map for coordinates
typedef boost::property_map<Graph, boost::vertex_index_t>::type IndexMap; // And for indices
typedef boost::graph_traits<Graph>::vertex_descriptor Vertex;
typedef boost::graph_traits<Graph>::vertex_iterator vertex_iter;
typedef boost::graph_traits<Graph>::out_edge_iterator edge_iter;


// Struct containing a graph over the vertices of a 2d grid together with a 2d array allowing vertices to be looked up quickly
typedef boost::multi_array<Vertex, 2> VertexMap;

typedef std::list<Coords> CoordsList;

struct GridGraph 
{
  Graph g;
  VertexMap m;

  GridGraph (const grid_size* dims) : m(boost::extents[dims[0]][dims[1]]) {}
};


// Bfs visitor that throws an exception if target node is found or distance threshold is reached

struct TerminateBfs
{
  bool found;
  TerminateBfs (bool f) : found(f) {}
};

class BfsVisitor : public boost::default_bfs_visitor {
public:
  void discover_vertex (Vertex u, const Graph& g) const
  {
    int r, c;
    boost::tie (r, c) = boost::get (coords_t(), g, u);
    // cout << "Discovering vertex " <<  r << ", " << c << endl;
    if ((r == r_goal) && (c == c_goal)) {
      throw TerminateBfs (true);
    }
    else if ((r == rmin) || (r == rmax) || (c == cmin) || (c == cmax)) {
      throw TerminateBfs (false);
    }
  }

  BfsVisitor (int r0, int c0, int threshold, int rg, int cg)
  {
    rmin = r0 - threshold;
    rmax = r0 + threshold;
    cmin = c0 - threshold;
    cmax = c0 + threshold;
    r_goal = rg;
    c_goal = cg;
  }


private:
  int rmin, rmax, cmin, cmax, r_goal, c_goal;
};



  

GridGraph makeGraphFromGrid (const GridArray& grid)
{
  const grid_size* dims = grid.shape();
  GridGraph gr(dims);
  Vertex v;

  CoordsMap coords = boost::get (coords_t(), gr.g);
  
  
  for (grid_size r=0; r!=dims[0]; r++) {
    for (grid_size c=0; c!=dims[1]; c++) {
      
      if (!grid[r][c]) {
        v = add_vertex(gr.g);
        gr.m[r][c] = v;
        boost::put (coords, v, *(new Coords(r,c)));
        
        if ((r>0) && !grid[r-1][c]) {
          boost::add_edge (v, gr.m[r-1][c], gr.g);
        }
        if ((c>0) && !grid[r][c-1]) {
          boost::add_edge (v, gr.m[r][c-1], gr.g);
        }
      }
    }
  }

  return gr;
}

void printGraph (const Graph& g) 
{
  vertex_iter vi, vg;
  edge_iter ei, eg;
  coords_t coords_key;
  
  cout << endl;
  for (boost::tie(vi,vg) = boost::vertices(g); vi != vg; vi++)
  {
    int r, c;
    boost::tie (r, c) = boost::get (coords_key, g, *vi);
    std::cout << r << "," << c << " " << boost::get (boost::vertex_index, g, *vi) << " ";
    for (boost::tie(ei, eg) = boost::out_edges (*vi, g); ei != eg; ei++) {
      int r2, c2;
      boost::tie (r2, c2) = boost::get (coords_key, g, boost::target (*ei, g));
      std::cout << r2 << "," << c2 << " ";
    }
    std::cout << std::endl;
  }
}




bool distLessThan (GridGraph* gr, int r0, int c0, int r1, int c1, int threshold)
{
  BfsVisitor vis (r0, c0, threshold, r1, c1);
  const grid_size* dims = gr->m.shape();
  Vertex v;
  int i=0;
  IndexMap indices = boost::get (boost::vertex_index, gr->g);
  // printGraph (gr->g);
  
  // Set the indices for the current graph
  for (grid_size r=0; r!=dims[0]; r++) {
    for (grid_size c=0; c!=dims[1]; c++) {
      v = gr->m[r][c];
      if (v) {
        boost::put (indices, v, i++);
      }
    }
  }
        
  try {
    boost::breadth_first_search (gr->g, gr->m[r0][c0], boost::visitor(vis));
  }
  catch (TerminateBfs e)
  {
    if (e.found) {
      return true;
    }
  }


  return false;
}






void removeBlock (GridGraph* gr, grid_size r0, grid_size c0, int s, CoordsList* removed)
{
  for (grid_size r=r0; r<r0+s; r++) {
    for (grid_size c=c0; c<c0+s; c++) {
      Vertex v = gr->m[r][c];
      if (v) {
        clear_vertex (v, gr->g);
        remove_vertex (v, gr->g);
        removed->push_back(Coords(r,c));
        gr->m[r][c] = 0;
      }
    }
  }
}


void addBlock (GridGraph* gr, CoordsList* removed, int numRows, int numCols) 
{
  CoordsMap coordsMap = boost::get (coords_t(), gr->g);
  while (!removed->empty()) {
    Coords coords = removed->front();
    int r = coords.first, c=coords.second;
    removed->pop_front();
    Vertex v;
    v = add_vertex (gr->g);
    gr->m[r][c] = v;
    boost::put (coordsMap, v, coords);
    if ((r>0) && (gr->m[r-1][c]))
      add_edge (v, gr->m[r-1][c], gr->g);
    if ((c>0) && (gr->m[r][c-1]))
      add_edge (v, gr->m[r][c-1], gr->g);
    if ((r<numRows-1) && (gr->m[r+1][c]))
      add_edge (v, gr->m[r+1][c], gr->g);
    if ((c<numCols-1) && (gr->m[r][c+1]))
      add_edge (v, gr->m[r][c+1], gr->g);
  }
}
      




BottleneckGraph makeBottleneckGraph (GridArray grid, int bottleneckSize, int bottleneckSkip,
                                     int distanceMultMin=3, int distanceMultMax=6)
{
  GridGraph gr = makeGraphFromGrid(grid);
  BottleneckGraph g;
  const grid_size* dims = grid.shape();
  CoordsList removedVertices;
  
  // Loop over top-left corner of block to remove
  for (grid_size r=1; r<dims[0]-bottleneckSize; r+=bottleneckSkip) {
    for (grid_size c=0; c<=dims[1]-bottleneckSize; c+=bottleneckSkip) {
      assert (removedVertices.empty());

      // Check if the block disconnects either of the diagonally opposite corners
      bool disconnected = false;
      for (unsigned int i=0; i<2; i++) {
        int r0=r-1+i*(bottleneckSize+1);
        int c0=std::max((int)c-1,0);
        int r1=r-1+(1-i)*(bottleneckSize+1);
        int c1=std::min(c+bottleneckSize,dims[1]-1);

        // Check that rows are within range
        if ((std::min(r0,r1)<0) || (std::max(r0,r1)>=(int)dims[0])) continue;
        
        // But what if the cells are obstacles... slide along the box till you have vertices that aren't
        for (;!gr.m[r0][c0] && c0<=c1;c0++);
        if (c0>c1) continue;
        for (;!gr.m[r1][c1] && c1>=c0;c1--);
        if (c1<c0) continue;


        if (distLessThan(&gr, r0, c0, r1, c1, distanceMultMin*bottleneckSize)) {
          // cout << "Original distance is within bounds... checking new distance " << endl;
          removeBlock (&gr, r, c, bottleneckSize, &removedVertices);
          // printGraph (gr.g);
          disconnected = !distLessThan(&gr, r0, c0, r1, c1, distanceMultMax*bottleneckSize);
          addBlock (&gr, &removedVertices, dims[0], dims[1]);
          if (disconnected) break;
        }
        
      }

      if (disconnected) cout << "Disconnected!" << endl;
      else cout << "Not disconnected!" << endl;
    }
  }
  return g;
}






} // namespace topological_map


int main (int, char* argv[])
{
  
  // Initialize grid
  topological_map::GridArray grid(boost::extents[4][5]);
  grid[0][2] = true;
  grid[2][2] = true;
  grid[3][2] = true;

  topological_map::GridGraph gr = topological_map::makeGraphFromGrid(grid);
  topological_map::printGraph (gr.g);
  topological_map::makeBottleneckGraph (grid, atoi(argv[1]), atoi(argv[2]));
 
  return 0;
}
