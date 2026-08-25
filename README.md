This is a cross-platform, C++ version of [studiomdl](https://developer.valvesoftware.com/wiki/StudioMDL_(GoldSrc)) for GoldSrc.

studiomdl compiles [Studio Model Data](https://developer.valvesoftware.com/wiki/SMD) from [QC](https://developer.valvesoftware.com/wiki/QC) files to a [MDL](https://developer.valvesoftware.com/wiki/QC) file.

The repository now also includes `mdltool++`, a binary MDL v10 inspection and skin-family editing tool for ready-made GoldSrc / Counter-Strike 1.6 models.

## Features
*   UV Shift Correction (not fully tested).
*   Support $flatshade texture mode.

---

## Build

Prerequisites:

*   A C++17 compatible compiler (GCC, Clang, MSVC)
*   CMake (version 3.14 or later)


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
```

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

Example error:

```text
Error: "ban1.bmp" is not an indexed 8-bit BMP.
```

### Safe Writing

* `--output <path>` writes a separate MDL file.
* `--in-place` writes to a temporary file first and validates it before replacement.
* `--backup` creates `model.mdl.bak` before replacing the original.

### Build And Test

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
ctest --output-on-failure
```

---

It's a fork of [fnky studiomdl](https://github.com/fnky/studiomdl).
