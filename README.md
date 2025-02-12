This tool is a C++ port of Valve's goldsrc studiomdl, the Half-Life 1 MDL (version 10) file format compiler tool.

This project aims to provide a modern, cleaner, and more maintainable alternative to Valve's original GoldSrc studiomdl tool. While Sven-Coop's SDK offers a modern version, it isn't open-source for some reason.

Feel free to contribute, suggest improvements, or offer feedback!. This repositry was created by a mapper, not a programmer, so contributions from developers are especially welcome. I simply made this project because, as a mapper, I want to use the best tools for mapping.

---

## Features

*   Modernized code*:  use C++17 features.
*   Cleaned Up Code*:
*   64-bit Support:
*   UV Shift Correction: (not fully tested yet) fix for UV shifting issues.
*   Support $flatshade texture mode.

>Note¹:
>Although the code has been cleaned up, it still has a lot of messy code and is being cleaned up progressively.

>Note²:
>Developed on linux, at the moment cross platform is a bit broken

>Note³:
>Your QC and SMD files has to use LF (Unix) line-endings in order to work.

---

## Build

Prerequisites:

*   A C++17 compatible compiler (GCC, Clang, MSVC)
*   CMake (version 3.15 or later)


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
