# Project Summary

This repository implements a **physically based volumetric path tracer** using **OptiX + CUDA**.  
It renders participating media represented by **tessellated icospheres** that act as shells enclosing Gaussian ellipsoids.  
OptiX built-in spheres were avoided since they fail to register hits when a ray originates inside them.

Each primitive contributes **two intersections per ray**: entry (`t_in`) and exit (`t_out`).  
Between those bounds, the renderer assumes a continuous medium and integrates density and optical depth analytically or via numerical solvers.  

### Key Components
- `__raygen__rg.cu` – main ray generation, scattering loop, Russian roulette, environment lighting.
- `__closesthit__ch.cu` – stores hit data (`t_hit`, primitive index, exit flag).
- `__miss__ms.cu` – returns environment color.
- `sampling.cuh` – integrates optical depth, samples scattering events, phase directions, and evaluates albedo.
- `trace.cuh` – handles OptiX ray queries and payloads.
- `launch_params.cuh` – camera, primitives, env map, and image buffers.

### Core Model
- Optical depth sampling: τ = −ln(1 − χ)
- Transmittance: T = exp(−τ)
- Phase function: isotropic (1 / 4π)
- Medium evaluation: Monte Carlo integration over entry–exit segments
- No true volume geometry, only analytic density fields

### Current Limitation
Rays starting **inside** a primitive detect only the exit face.  
Since the interior volume has no geometric representation, medium traversal and scattering fail to initialize.
