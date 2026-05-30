/*
+------------------------------------------------------------------------------+
|                                                                              |
|                                 Hugo Ledoux                                  |
|                             h.ledoux@tudelft.nl                              |
|                                  2026-05-10                                  |
|                                                                              |
+------------------------------------------------------------------------------+
*/

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <queue>

#include <CGAL/linear_least_squares_fitting_3.h>
#include <CGAL/Simple_cartesian.h>
#include <CGAL/Plane_3.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangle_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>

typedef CGAL::Simple_cartesian<double> Kernel;
typedef Kernel::Point_2 Point_2;
typedef Kernel::Point_3 Point_3;
typedef Kernel::Plane_3 Plane_3;

struct FaceInfo { int nesting_level; };
typedef CGAL::Triangulation_vertex_base_2<Kernel>                                    Vb;
typedef CGAL::Constrained_triangulation_face_base_2<Kernel>                          Cbf;
typedef CGAL::Triangulation_face_base_with_info_2<FaceInfo, Kernel, Cbf>             Fb;
typedef CGAL::Triangulation_data_structure_2<Vb, Fb>                                 TDS;
typedef CGAL::Constrained_Delaunay_triangulation_2<Kernel, TDS>                      CDT;




//-- https://github.com/nlohmann/json
//-- used to read and write (City)JSON
#include "json.hpp" //-- it is in the /include/ folder

using json = nlohmann::json;


int   get_no_roof_surfaces(const json& j);
void  list_all_vertices(const json& j);
void  visit_roofsurfaces(const json& j);



int main(int argc, const char * argv[]) {
  //-- will read the file passed as argument or twobuildings.city.json if nothing is passed
  const char* filename = (argc > 1) ? argv[1] : "../../data/nextbk_2b.city.json";
  std::cout << "Processing: " << filename << std::endl;
  std::ifstream input(filename);
  if (!input.is_open()) {
    std::cerr << "Error: cannot open " << filename << std::endl;
    return 1;
  }
  json j;
  input >> j;
  input.close();

  //-- get total number of RoofSurface in the file
  int noroofsurfaces = get_no_roof_surfaces(j);
  std::cout << "Total RoofSurface: " << noroofsurfaces << std::endl;

  list_all_vertices(j);

  visit_roofsurfaces(j);

  //-- print out the number of Buildings in the file
  int nobuildings = 0;
  for (auto& co : j["CityObjects"]) {
    if (co["type"] == "Building") {
      nobuildings += 1;
    }
  }
  std::cout << "There are " << nobuildings << " Buildings in the file" << std::endl;

  //-- print out the number of vertices in the file
  std::cout << "Number of vertices " << j["vertices"].size() << std::endl;

  std::srand(std::time(nullptr));

  //-- add an attribute "volume"
  for (auto& co : j["CityObjects"]) {
    if (co["type"] == "Building") {
      co["attributes"]["volume"] = rand();
    }
  }

  // Step 1: Keep only LoD 2.2 and delete BuildingPart
  

  // Store BuildingPart IDs to remove later
  std::vector<std::string> to_delete;

  // Iterate over CityObjects
  for (auto& [id, obj] : j["CityObjects"].items())
    {
      // Only process Buildings
      if (obj["type"] != "Building")
          continue;

      // Only process buildings with children
      if (!obj.contains("children"))
          continue;

      json lod22_geometries = json::array();

      // Get children
      auto children = obj["children"];

      // Loop through children
      for (auto& child_id_json : children)
        {
          std::string child_id = child_id_json;

          auto& child = j["CityObjects"][child_id];

          // Check geometries in BuildingPart
          if (child.contains("geometry"))
          {
            for (auto& geom : child["geometry"])
              {
                // Keep only LoD2.2
                if (geom["lod"] == "2.2")
                  {lod22_geometries.push_back(geom);}
                }
            }

        // Mark BuildingPart for deletion
          to_delete.push_back(child_id);
        }

        // Remove old geometry
        obj.erase("geometry");

        // Add LoD2.2 geometry to Building
        obj["geometry"] = lod22_geometries;

        // Remove children attribute
        obj.erase("children");
    }

  // Delete BuildingParts AFTER iteration
  for (const auto& part_id : to_delete)
  {
    j["CityObjects"].erase(part_id);
  }

  // Step 2: Triangulation of the surfaces
  Plane_3 plane;
  std::vector<Point_3> pts;
  std::vector<Point_2> projected_pts;

  auto get_point = [&](int vi) -> Point_3 {
    std::vector<int> v = j["vertices"][vi];
    double x = v[0] * j["transform"]["scale"][0].get<double>() + j["transform"]["translate"][0].get<double>();
    double y = v[1] * j["transform"]["scale"][1].get<double>() + j["transform"]["translate"][1].get<double>();
    double z = v[2] * j["transform"]["scale"][2].get<double>() + j["transform"]["translate"][2].get<double>();
    return Point_3(x, y, z);
  };


  for (auto& co : j["CityObjects"].items()) {
    for (auto& g : co.value()["geometry"]) {
      if (g["type"] == "Solid") {
        json new_boundaries = json::array();
        json new_sem_values = json::array();
        for (int shell_idx = 0; shell_idx < (int)g["boundaries"].size(); shell_idx++) {
          auto& shell = g["boundaries"][shell_idx];
          json new_shell = json::array(); 
          json new_shell_sem = json::array(); // keep track of new semantics to make it valid 
          for (int surf_idx = 0; surf_idx < (int)shell.size(); surf_idx++) {
            auto& s = shell[surf_idx];
            int sem_idx = g["semantics"]["values"][shell_idx][surf_idx];



            // Step 2.1: compute its best-fitting plane
            pts.clear();
            for (auto& ring : s) {
              for (auto& vertex : ring) {
                pts.push_back(get_point(vertex.get<int>()));

              }   
           
            }

            // Dimension 0 since we're using points
            CGAL::linear_least_squares_fitting_3(pts.begin(),pts.end(),plane,CGAL::Dimension_tag<0>());

            //Step 2.2/3 Triangulation 
            CDT cdt;
            
            for (auto& ring : s) {
              for (int i = 0; i < ring.size(); i++) {
                Point_3 p1 = get_point(ring[i].get<int>());
                Point_3 p2 = get_point(ring[(i+1) % ring.size()].get<int>());
                Point_2 proj1 = plane.to_2d(p1);
                Point_2 proj2 = plane.to_2d(p2);
                
                cdt.insert_constraint(proj1, proj2);
              }
            }
            
            // Step 2.4: odd-even rule
            // assign every face with -1 (not yet visited)

            for (auto fit = cdt.all_faces_begin(); fit != cdt.all_faces_end(); ++fit)
              fit->info().nesting_level = -1;

            std::queue<CDT::Face_handle> queue;
            cdt.infinite_face()->info().nesting_level = 0;
            queue.push(cdt.infinite_face());

            while (!queue.empty()) {
              CDT::Face_handle fh = queue.front(); queue.pop();
                for (int i = 0; i < 3; i++) {
                CDT::Face_handle nb = fh->neighbor(i);
                if (nb->info().nesting_level != -1) continue;  // skip if already visited
                bool constrained = cdt.is_constrained(CDT::Edge(fh, i));
                if (constrained)
                  nb->info().nesting_level = fh->info().nesting_level + 1;
                else
                  nb->info().nesting_level = fh->info().nesting_level;
                queue.push(nb);
               }
              }


            // Step 2.5: Keep only interior triangles and store in json file + new semantics
            std::vector<Kernel::Triangle_2> interior_triangles;
            
            for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
            if (fit->info().nesting_level % 2 == 0) continue;  //  even = exterior, odd is interior
            

  
            json triangle_ring = json::array();
            for (int i = 0; i < 3; i++) {
              Point_3 p = plane.to_3d(fit->vertex(i)->point());

              int vx = (int)std::round((p.x() - j["transform"]["translate"][0].get<double>()) / j["transform"]["scale"][0].get<double>());
              int vy = (int)std::round((p.y() - j["transform"]["translate"][1].get<double>()) / j["transform"]["scale"][1].get<double>());
              int vz = (int)std::round((p.z() - j["transform"]["translate"][2].get<double>()) / j["transform"]["scale"][2].get<double>());

              j["vertices"].push_back({vx, vy, vz});
              triangle_ring.push_back((int)j["vertices"].size() - 1);
            }

            json triangle_surface = json::array();
            triangle_surface.push_back(triangle_ring);
            new_shell.push_back(triangle_surface); 
            new_shell_sem.push_back(sem_idx);
          }

        }  

        new_boundaries.push_back(new_shell); 
        new_sem_values.push_back(new_shell_sem);
      }  

      g["boundaries"] = new_boundaries;  // replace old boundaries
      g["semantics"]["values"] = new_sem_values;

    }
  }
}

pts.clear();


  

  //-- write to disk the modified city model (insert "_out" before ".city.json")
  std::string outfile = filename;
  size_t pos = outfile.rfind(".city.json");
  if (pos != std::string::npos) {
    outfile.insert(pos, "_out");
  } else {
    outfile = "out.city.json";
  }
  std::ofstream o(outfile);
  if (!o.is_open()) {
    std::cerr << "Error: cannot write " << outfile << std::endl;
    return 1;
  }
  std::cout << "Written to: " << outfile << std::endl;
  o << j.dump(2) << std::endl;
  o.close();



  return 0;
}


// Visit every 'RoofSurface' in the CityJSON model and output its geometry (the arrays of indices)
// Useful to learn to visit the geometry boundaries and at the same time check their semantics.
void visit_roofsurfaces(const json& j) {
  for (auto& co : j["CityObjects"].items()) {
    for (auto& g : co.value()["geometry"]) {
      if (g["type"] == "Solid") {
        for (int i = 0; i < g["boundaries"].size(); i++) {
          for (int k = 0; k < g["boundaries"][i].size(); k++) {
            int sem_index = g["semantics"]["values"][i][k];
            if (g["semantics"]["surfaces"][sem_index]["type"].get<std::string>().compare("RoofSurface") == 0) {
              std::cout << "RoofSurface: " << g["boundaries"][i][k] << std::endl;
            }
          }
        }
      }
    }
  }
}


// Returns the number of 'RooSurface' in the CityJSON model
int get_no_roof_surfaces(const json& j) {
  int total = 0;
  for (auto& co : j["CityObjects"].items()) {
    for (auto& g : co.value()["geometry"]) {
      if (g["type"] == "Solid") {
        for (auto& shell : g["semantics"]["values"]) {
          for (auto& s : shell) {
            if (g["semantics"]["surfaces"][s.get<int>()]["type"].get<std::string>().compare("RoofSurface") == 0) {
              total += 1;
            }
          }
        }
      }
    }
  }
  return total;
}




// CityJSON files have their vertices compressed: https://www.cityjson.org/specs/1.1.1/#transform-object
// this function visits all the surfaces and print the (x,y,z) coordinates of each vertex encountered
void list_all_vertices(const json& j) {
  for (auto& co : j["CityObjects"].items()) {
    std::cout << "= CityObject: " << co.key() << std::endl;
    for (auto& g : co.value()["geometry"]) {
      if (g["type"] == "Solid") {
        for (auto& shell : g["boundaries"]) {
          for (auto& surface : shell) {
            for (auto& ring : surface) {
              std::cout << "---" << std::endl;
              for (auto& v : ring) { 
                std::vector<int> vi = j["vertices"][v.get<int>()];
                double x = (vi[0] * j["transform"]["scale"][0].get<double>()) + j["transform"]["translate"][0].get<double>();
                double y = (vi[1] * j["transform"]["scale"][1].get<double>()) + j["transform"]["translate"][1].get<double>();
                double z = (vi[2] * j["transform"]["scale"][2].get<double>()) + j["transform"]["translate"][2].get<double>();
                std::cout << std::setprecision(2) << std::fixed << v << " (" << x << ", " << y << ", " << z << ")" << std::endl;                
              }
            }
          }
        }
      }
    }
  }
}
