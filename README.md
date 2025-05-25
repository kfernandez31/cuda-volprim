# Readme

## TODO
- [ ] namespaces for utils,
- [ ] trailing whitespace fix

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

<!-- ### Static analysis -->
<!-- include-what-you-use ^
  -std=c++20 ^
  -DUNICODE -D_CRT_SECURE_NO_WARNINGS ^
  -DGLM_ENABLE_EXPERIMENTAL ^
  -fms-compatibility -fms-extensions ^
  -Iinclude ^
  -Ithird_party ^
  -I"C:/ProgramData/NVIDIA Corporation/OptiX SDK 9.0.0/include" ^
  -I"C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.4/include" ^
  src/main.cpp -->


<!-- cppcheck --enable=all --inconclusive --inline-suppr --std=c++20 --quiet -->