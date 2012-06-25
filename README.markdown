d3dmap
======
Standalone dmap tool from Doom 3

--------

dmap is the map compiler from Doom 3 and all the other id tech 4 games. The id tech 4 map format was unsuitable for indie games because the map compiler is integrated into the engine. In other words, you need Doom 3 to compile maps.

This repository contains the extracted source code from the [original Doom 3 source code](https://github.com/TTimo/doom3.gpl). It does not include AAS support, but is otherwise fully functional.

Compiling
---------
Open `neo/doom3.sln` in Visual Studio 2010, select your favourite configuration (probably Dedicated Release) and press F6.

Linux users are out of luck for now, but I might port the project to CMake in the future. If you don't wan't to wait that long, feel free to send me a pull request. I think I removed all platform dependent stuff (apart from `Sys_ListFiles` in `neo/dmap/main.cpp`. Maybe use Boost::FileSystem instead?).

Usage
-----
d3dmap expects a somewhat Doom 3-like asset structure in the working directory. This is an example directory structure for compiling the map `foo.map`:

    maps
    |
    +- foo.map
    
    materials
    |
    +- something.mtr

`something.mtr` must contain a material called `_tracemodel`, or the compiler will fail. Of course, the file does not need to be called `something.mtr`. The important thing is that the `_tracemodel` material exists. This should be the definition of the material:

    _tracemodel {
    	collision
    }

To compile the map, execute the following command in the working directory: `d3dmap foo`

Calling the executable without parameters displays this help text:

    Usage:	d3dmap [options] filename
    
    Options: --light-carve: Split polygons along light volume edges
             --no-carve: Doesn't cut any surfaces as if they had the noFragment global keyword
             --no-clip-sides: Doesn't clip overlapping solid brushes
             --no-cm: Do not create collission map
             --no-flood: Skips the flooding of the map allowing it to have leaks
             --no-opt: Doesn't merge/remove redundant polygons
             --no-t-junc: Doesn't create T-Junctions. Forces --no-opt
             -v, --verbose: Shows additional infromation while compiling
             --verbose-entities: Shows additional information on entities like removal of degenerate polygons
             --shadow-opt <n>: Sets the static shadow computation optimization type:
                 0: No optimization
                 1: SO_MERGE_SURFACES (default)
                 2: SO_CLIP_OCCLUDED
                 3: SO_CLIP_OCCLUDERS
                 4: SO_CLIP_SILS
                 5: SO_SIL_OPTIMIZE
    
    No AAS support in this executable.
    Extracted from the Doom 3 sourcecode, licensed under the GNU GPL v3.
    See <https://github.com/TTimo/doom3.gpl/blob/master/COPYING.txt>
    Information for this help text was taken from modwiki.net.
    See <http://modwiki.net/wiki/Dmap_(console_command)>.

**Note:** Not all options have been tested.

Todo
----
* Switch to CMake for platform independence.
* Delete the files that are not needed. Currently they are only excluded from the project.

License
-------
See [COPYING.txt](https://github.com/TTimo/doom3.gpl/blob/master/COPYING.txt) from the Doom 3 source tree.