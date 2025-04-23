# Readme

## TODO
- [ ] working compilation on Windows
- [ ] formatting everything automatically (except third-party)
- [ ] Include What You Use
- [ ] moving on to the sphere render...

## Compilation

### Windows

0. Set up environment
```sh
& "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

1. Generating build files
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_BUILD_TYPE=Release
```

2. Building
```sh
ninja -C build
```

## Static analysis

### Windows

#### Clang-tidy
```
clang-tidy <file> -p build        
```
