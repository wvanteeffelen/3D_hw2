
# How to constrained tetrahedralise 2 buildings (demo showed in class)

I did the following (parts about 3DBAG and CityJSON formats/tools will become clearer in the coming weeks)

1. download 3dbag tile https://data.3dbag.nl/v20250903/tiles/9/288/556/9-288-556.city.json
2. extract building 'NL.IMBAG.Pand.0503100000037626': `cjio 9-288-556.city.json subset --id NL.IMBAG.Pand.0503100000037626 lod_filter "2.2" save 626.json `
3. extract building 'NL.IMBAG.Pand.0503100000037627': `cjio 9-288-556.city.json subset --id NL.IMBAG.Pand.0503100000037627 lod_filter "2.2" save 627.json`
4. merge the 2 buildings into one CityJSON file: `cjio 626.json merge 627.json save b2.city.json`
5. export to OBJ (will triangulate the surfaces): `cjio b2.city.json export OBJ b2.obj`

You should be able to see that file in MeshLab/Mapple/etc, it's in `./data/b2.obj`

Because many software prefer [OFF format](https://en.wikipedia.org/wiki/OFF_(file_format)), I have converted the file to OFF with [meshio](https://pypi.org/project/meshio/) with `meshio convert b2.obj b2.off` (file `b2.off` is also in `./data/`).


## Option 1: CGAL tetrahedraliser

<https://doc.cgal.org/latest/Constrained_triangulation_3/index.html#Chapter_CT_3>

You need CGAL >=6.1

I put the code in `t1.cpp`, compile it and run it.

If all goes well it outputs `out.mesh` which you can view partly with Mapple, which you can convert to [VTK](https://docs.vtk.org/en/latest/vtk_file_formats/index.html) with meshio:

`meshio convert out.mesh out.vtk`

and then use [ParaView](https://www.paraview.org/) to view the tetrahedra and slice them.


## Option 2: TetGen

<https://www.wias-berlin.de/software/tetgen/>

1. Download and install it
2. Remove the commented lines (starting with `#`) from `b2.off` because TetGen doesn't like them
3. `tetgen -ck ./data/b2.off`

this will output a file `b2.1.vtk` that you can view with [ParaView](https://www.paraview.org/)
