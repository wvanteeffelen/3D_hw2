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
