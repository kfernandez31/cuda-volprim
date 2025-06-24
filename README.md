# Readme

## Questions to Piotr
- [ ] which (.ply) asset to get?
- [ ] quantify with psnr, ... ?
- [ ] correctness (compare w/ optimized) -> efficiency
- [X] how to understand the asset structure?
- [X] S_inv in integration matrix?
- [?] **isotropic**, centered, unit Gaussian for integration
- [X] **frequency_w_{0,1,2}** attribute - what is it?

poziomy? orientacje? assety roznia sie rotacja
zignorowac frequency_w_0, nx?
jakie to są viewpoints?
meshlab

<!-- 🔍 Checking file: ""optimized_asset_pyr0/optimized_asset_pyr0/data/root.primitives_pyr0.ply""
✅ All frequency_w_* values are zero.

🔍 Checking file: ""optimized_asset_pyr0/optimized_asset_pyr1/data/root.primitives_pyr0.ply""
✅ All frequency_w_* values are zero.

🔍 Checking file: ""optimized_asset_pyr0/optimized_asset_pyr1/data/root.primitives_pyr1.ply""
❗ Non-zero frequency at vertex 0: (9.98902, 9.98926, 9.98946)

🔍 Checking file: ""optimized_asset_pyr0/optimized_asset_pyr2/data/root.primitives_pyr0.ply""
✅ All frequency_w_* values are zero.

🔍 Checking file: ""optimized_asset_pyr0/optimized_asset_pyr2/data/root.primitives_pyr1.ply""
❗ Non-zero frequency at vertex 0: (9.98902, 9.98926, 9.98946)

🔍 Checking file: ""optimized_asset_pyr0/optimized_asset_pyr2/data/root.primitives_pyr2.ply""
❗ Non-zero frequency at vertex 0: (19.9803, 19.979, 19.9795)

🔍 Checking file: ""optimized_asset_pyr0/optimized_asset_pyr3/data/root.primitives_pyr0.ply""
✅ All frequency_w_* values are zero.

🔍 Checking file: ""optimized_asset_pyr0/optimized_asset_pyr3/data/root.primitives_pyr1.ply""
❗ Non-zero frequency at vertex 0: (9.98902, 9.98926, 9.98946)

🔍 Checking file: ""optimized_asset_pyr0/optimized_asset_pyr3/data/root.primitives_pyr2.ply""
❗ Non-zero frequency at vertex 0: (19.9803, 19.979, 19.9795)

🔍 Checking file: ""optimized_asset_pyr0/optimized_asset_pyr3/data/root.primitives_pyr3.ply""
❗ Non-zero frequency at vertex 0: (39.9575, 39.9575, 39.9575) -->


## TODO
- [ ] asset from happly
- [ ] replace stbimage with tinyexr
- [ ] get rid of underscores, they add visual clutter
- [ ] quaternion for rotation matrix - will already be normalized?

Additions
- [ ] importance sampling / non-isotrophic phase function
- [ ] anyhit over closesthit

## Compilation

### Windows

0. Open up "x64 Native Tools Command Prompt for VS 2022"

<!-- 0. Set up environment
```sh
& "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
``` -->

1. Generate build files
```sh
  cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_BUILD_TYPE=Release
```

2. Build
```sh
ninja -C build
```

<!-- ### Static analysis -->
<!-- 
include-what-you-use ^
  -std=c++20 ^
  -DUNICODE -D_CRT_SECURE_NO_WARNINGS ^
  -DGLM_ENABLE_EXPERIMENTAL ^
  -fms-compatibility -fms-extensions ^
  -Iinclude ^
  -Ithird_party ^
  -I"C:/ProgramData/NVIDIA Corporation/OptiX SDK 9.0.0/include" ^
  -I"C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.4/include" ^
  src/thesis/host/main.cpp 
  -->


<!-- cppcheck --enable=all --inconclusive --inline-suppr --std=c++20 --quiet -->