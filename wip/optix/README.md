# Readme

## Compilation

### Windows

1. Generating build files
```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

2. Building
```
ninja -C build
```

## Static analysis

### Windows

#### Clang-tidy
```
clang-tidy <file> -p build        
```
