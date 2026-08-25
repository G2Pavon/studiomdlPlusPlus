This is a cross-platform, C++ version of [studiomdl](https://developer.valvesoftware.com/wiki/StudioMDL_(GoldSrc)) for GoldSrc.

studiomdl compiles [Studio Model Data](https://developer.valvesoftware.com/wiki/SMD) from [QC](https://developer.valvesoftware.com/wiki/QC) files to a [MDL](https://developer.valvesoftware.com/wiki/QC) file.

The repository now also includes:

* `mdltool++`: a binary MDL v10 inspection and skin-family editing CLI for ready-made GoldSrc / Counter-Strike 1.6 models.
* `mdltool_gui`: a Dear ImGui desktop editor built on the same `mdl_core` library.

## Features
*   UV Shift Correction (not fully tested).
*   Support $flatshade texture mode.

---

## Build

Prerequisites:

*   A C++17 compatible compiler (GCC, Clang, MSVC)
*   CMake (version 3.14 or later)
*   OpenGL 3 capable environment for `mdltool_gui`


Clone the repository:
```bash
git clone https://github.com/G2Pavon/studiomdlPlusPlus.git
cd studiomdlPlusPlus
```

Create a build directory and build files:
```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
ctest --output-on-failure
```

Or use presets:

```bash
cmake --preset release
cmake --build --preset build-release
ctest --preset test-release
```

For core-only builds without GUI dependencies:

```bash
cmake --preset core-release
cmake --build --preset build-core-release
ctest --preset test-core-release
```

## Targets

The project builds the following targets:

* `mdl_core`
* `studiomdl++`
* `mdltool++`
* `mdltool_gui`

## Usage

```bash
studiomdl++ <input qc> [options]

[-f]                Invert normals
[-a <angle>]        Set vertex normal blend angle override, in degrees
[-b]                Keep all unused bones

```

```bash
mdltool++ inspect model.mdl
mdltool++ validate model.mdl
mdltool++ submodels list model.mdl
mdltool++ textures list model.mdl
mdltool++ skins list model.mdl
mdltool++ add-skin banners.mdl ban1.bmp 2 def_post.bmp
mdltool++ add-skin --model banners.mdl --bmp ban1.bmp --submodel 2 --target def_post.bmp --output banners_new.mdl
```

### GUI Usage

```bash
mdltool_gui.exe
mdltool_gui.exe bannersv12.mdl
```

The GUI supports:

* opening `.mdl` files from the menu, drag-and-drop, or the first command-line argument
* browsing submodels, meshes, textures, and skin families
* indexed texture preview with nearest-neighbor filtering
* `Add skin`, `Replace texture`, `Remove skin`, and `Undo last operation`
* safe `Save` and `Save as`
* built-in validation and copyable `model.mdl:submodel:skin` references

The first GUI version does not implement a 3D model viewport.

`mdltool++ add-skin` performs binary MDL v10 editing. It does not decompile QC/SMD and it keeps geometry, meshes, bones, animations, events, hitboxes, attachments, bodyparts, submodels, and existing texture data intact.

Expected `add-skin` output:

```text
Input model:       banners.mdl
Submodel:          2
Target texture:    def_post.bmp
Imported texture:  ban1.bmp
New skin index:    5
Output model:      banners_new.mdl
Reference:         banners_new.mdl:2:5
```

### GoldSrc Skin Families

GoldSrc skin families are global to the model. `StudioMesh::skinref` is not a direct texture index. The final texture is resolved through:

```text
skin family + skinref -> texture index
```

`mdltool++ add-skin` finds the target `skinref` through the selected submodel and texture name, copies a source skin family, and replaces only the matching `skinref` entries in the new family.

### Supported BMP Input

`mdltool++` accepts only GoldSrc-compatible BMP input:

* BMP
* indexed 8-bit
* palette up to 256 colors
* no hidden conversion from 24/32-bit formats

The same validation rules are used by `mdltool_gui`.

Example error:

```text
Error: "ban1.bmp" is not an indexed 8-bit BMP.
```

### Safe Writing

* `--output <path>` writes a separate MDL file.
* `--in-place` writes to a temporary file first and validates it before replacement.
* `--backup` creates `model.mdl.bak` before replacing the original.

`mdltool_gui` uses the same safe write flow:

1. write to a temporary file next to the destination
2. reopen and validate the generated MDL
3. create `.bak` when enabled
4. replace the destination only after successful validation

The rebuilt writer preserves pre-texture sections and rebuilds texture headers, skin table, and texture payloads only once. It avoids duplicating the original texture section in the saved MDL.

### Build And Test

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
ctest --output-on-failure
```

## GUI Dependencies

`mdltool_gui` fetches fixed upstream revisions:

* Dear ImGui docking: `fd13a1e8923a0a7077b404fc36fd063b25a0c0b5`
* GLFW: `e5ca45dddfb081afd23ec77fa62b2d01c41fe56b` (`3.3.10`)
* Native File Dialog Extended: `86f742bab39f1b253ad111e8ce776b46dd1ccdbe` (`v1.3.0`)

---

It's a fork of [fnky studiomdl](https://github.com/fnky/studiomdl).
