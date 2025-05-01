# Readme

## TODO
- [ ] Include What You Use
- [ ] moving on to the sphere render...
- [ ] compiler cache
- [ ] CRLF
- [ ] reformat again
- [ ] clean git history?
- [ ] a math header with eg. clamp

## Compilation

### Windows

0. Open up "x64 Native Tools Command Prompt for VS 2022"

<!-- 0. Set up environment
```sh
& "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
``` -->

1. Generate build files
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_BUILD_TYPE=Release
```

2. Build
```sh
ninja -C build
```

