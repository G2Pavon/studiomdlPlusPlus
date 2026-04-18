This is a cross-platform, C++ version of [studiomdl](https://developer.valvesoftware.com/wiki/StudioMDL_(GoldSrc)) for GoldSrc.

studiomdl compiles [Studio Model Data](https://developer.valvesoftware.com/wiki/SMD) from [QC](https://developer.valvesoftware.com/wiki/QC) files to a [MDL](https://developer.valvesoftware.com/wiki/QC) file.

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

---

It's a fork of [fnky studiomdl](https://github.com/fnky/studiomdl).
