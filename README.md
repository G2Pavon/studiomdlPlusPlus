## studiomdl++

C++ port of goldsrc studiomdl model compiler tool.

- Removed unused code
- Cleaned up a bit
- 64 bit
- UV shift correction (didn't test too much to confirm if it works well tbh)
- Flatshade texture mode

>Note¹:
>Developed on linux, cross platform is broken

>Note²:
>Your QC and SMD files has to use LF (Unix) line-endings in order to work.


## Build

Clone this repo
```bash
git clone https://github.com/G2Pavon/studiomdlPlusPlus.git
cd studiomdlPlusPlus
```

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

``
