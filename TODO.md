- [ ] Group up external libraries under one folder:
    - dbg.h, 
    - miniz.h/c
    - tinyexr.h
    - tiny_bvh.h
    - stb_image.h
- [ ] Transition over to CUDA


```
cd "C:\ProgramData\NVIDIA Corporation\OptiX SDK 9.0.0\SDK"
cmake --build build -S .
cmake --build build --config Release
```