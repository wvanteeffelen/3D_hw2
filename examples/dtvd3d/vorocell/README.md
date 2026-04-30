
## Code for the small exercise in class

Extracts a single Voronoi cell from a 3D Delaunay triangulation of 1000 random points and writes it as an OFF mesh (`onecell.off`). 

Uses CGAL's `dual()` for Voronoi vertices and `Cell_circulator` for face ordering.

Observe that the generated `onecell.off` is not triangulated (its faces) and that MeshLab displays gibberish.

Mapple displays it correctly ✅
