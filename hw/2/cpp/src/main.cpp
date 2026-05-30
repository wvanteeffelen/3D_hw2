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

#include <CGAL/linear_least_squares_fitting_3.h>
#include <CGAL/Simple_cartesian.h>
#include <CGAL/Plane_3.h>

typedef CGAL::Simple_cartesian<double> Kernel;
typedef Kernel::Point_2 Point_2;
typedef Kernel::Point_3 Point_3;
typedef Kernel::Plane_3 Plane_3;

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
        for (auto& shell : g["boundaries"]) {
          for (auto& s : shell){


            
            pts.clear();
            for (auto& ring : s) {
              for (auto& vertex : ring) {
                pts.push_back(get_point(vertex.get<int>()));

              }   
           
            }

          // Dimension 0 since we're using points
            CGAL::linear_least_squares_fitting_3(pts.begin(),pts.end(),plane,CGAL::Dimension_tag<0>());
            CGAL::Constrained_Delaunay_triangulation_2<Kernel> cdt;
            projected_pts.clear();
            for (auto& ring : s) {
              for (int i = 0; i < ring.size(); i++) {
                Point_3 p1 = get_point(ring[i].get<int>());
                Point_3 p2 = get_point(ring[(i+1) % ring.size()].get<int>());
                Point_2 proj1 = plane.to_2d(p1);
                Point_2 proj2 = plane.to_2d(p2);
                projected_pts.push_back(proj1);
                cdt.insert(proj1);
                cdt.insert(proj2);
                cdt.insert_constraint(proj1, proj2);
              }
            }

            std::vector<CGAL::Triangle_2> interior_triangles;
            
            for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
              if (cdt.is_infinite(fit)) continue;
              if (fit->is_constrained()) continue;
              interior_triangles.push_back(CGAL::Triangle_2(fit->vertex(0)->point(), fit->vertex(1)->point(), fit->vertex(2)->point()));
          }

          for (const auto& tri : interior_triangles) {
            for (int i = 0; i < 3; i++) {
              Point_2 proj = tri.vertex(i);
              Point_3 p3d = plane.to_3d(proj);
              std::cout << "Triangle vertex: (" << p3d.x() << ", " << p3d.y() << ", " << p3d.z() << ")" << std::endl;
            }
        }
      }
    }
  };

  pts.clear();
  projected_pts.clear();


  


  

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
