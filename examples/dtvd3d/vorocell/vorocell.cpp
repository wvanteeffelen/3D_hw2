#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Delaunay_triangulation_3.h>

#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <random>
#include <cassert>

typedef CGAL::Exact_predicates_inexact_constructions_kernel     K;
typedef CGAL::Delaunay_triangulation_3<K, CGAL::Fast_location>  Delaunay;
typedef Delaunay::Point                                         Point;

int main()
{
  //-- an empty DT in 3D
  Delaunay dt3d;

  //-- generate randomly 1000 points in 100x100x100 cube
  std::mt19937 rng(42); //-- use a seed for reproducibility
  std::uniform_real_distribution<double> dist(0, 100);
  for (int i = 0; i < 1000; ++i)
  {
    dt3d.insert(Point(dist(rng), dist(rng), dist(rng)));
  }

  std::cout << "Inserted " << dt3d.number_of_vertices() << " points into Delaunay triangulation." << std::endl;
  std::cout << "Number of vertices: " << dt3d.number_of_vertices() << std::endl;
  std::cout << "Number of cells:    " << dt3d.number_of_cells() << std::endl;

  //-- find interior vertex not on convex hull; we know middle is (50, 50, 50) 
  double cx = 50., cy = 50., cz = 50.;
  Delaunay::Vertex_handle v_mid; //-- a Vertex_handle is a smart unique identifier to a vertex (a smart pointer)
  for (auto vit = dt3d.finite_vertices_begin(); vit != dt3d.finite_vertices_end(); vit++) {
    double dx = vit->point().x()-cx;
    double dy = vit->point().y()-cy;
    double dz = vit->point().z()-cz;
    double d = dx*dx + dy*dy + dz*dz;
    if (d < 100) { //-- it's close enough to the centre to be interior
      v_mid = vit;
      break;
    }
  }
  std::cout << "Selected vertex at " << v_mid->point() << std::endl;

  //-- collect Voronoi vertices: circumcentres of all incident tetrahedra
  std::vector<Delaunay::Cell_handle> inc_cells;
  dt3d.incident_cells(v_mid, std::back_inserter(inc_cells));
  std::cout << "Number of incident tets:    " << inc_cells.size() << std::endl;

  std::map<Delaunay::Cell_handle, int> cell_idx;
  std::vector<Point> voro_verts;
  for (auto c : inc_cells) {
    cell_idx[c] = (int)voro_verts.size();
    voro_verts.push_back(dt3d.dual(c)); // circumcentre = Voronoi vertex = dual
  }

  //-- collect Voronoi faces: one polygon per incident Delaunay edge
  //   circulate around the edge to get the ordered ring of circumcenters
  std::vector<Delaunay::Edge> inc_edges;
  dt3d.incident_edges(v_mid, std::back_inserter(inc_edges));
  std::cout << "Number of incident edges:    " << inc_edges.size() << std::endl;

  std::vector<std::vector<int>> faces;
  for (auto& edge : inc_edges) {
    Delaunay::Cell_circulator cc = dt3d.incident_cells(edge);
    Delaunay::Cell_circulator done = cc;
    std::vector<int> face;
    bool valid = true;
    do {
      auto it = cell_idx.find(cc);
      if (it == cell_idx.end()) { valid = false; break; }
      face.push_back(it->second);
      cc++;
    } while (cc != done);
    if (valid && !face.empty()) {
      faces.push_back(face);
    }
  }

  //-- write OFF
  std::ofstream off("onecell.off");
  off << "OFF\n";
  off << voro_verts.size() << " " << faces.size() << " 0\n";
  for (auto& p : voro_verts)
    off << p.x() << " " << p.y() << " " << p.z() << "\n";
  for (auto& f : faces) {
    off << f.size();
    for (int i : f) off << " " << i;
    off << "\n";
  }
  off.close();

  std::cout << "Wrote " << voro_verts.size() << " Voronoi vertices and "
            << faces.size() << " faces to onecell.off" << std::endl;

  return 0;
}
