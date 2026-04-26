# How the OptiX denoiser works

Your code (`include/thesis/host/optix/denoiser.h`) is doing about ~30 lines of glue around what is, internally, a fairly elaborate piece of machine learning infrastructure. Here's the full stack, from your radiance buffer to the smoothed pixel.

---

## 1. The problem it solves

Path tracing produces a Monte Carlo estimate of pixel radiance:

$$L_{\text{pixel}} \approx \frac{1}{N} \sum_{i=1}^{N} f(\omega_i)$$

For finite N, the variance of this estimate is `Var[f]/N`. That variance shows up as **per-pixel grain** — independent noise that's uncorrelated between adjacent pixels. The signal you want (the actual radiance distribution) is highly correlated between adjacent pixels: a cloud's silhouette doesn't change shape from pixel to pixel.

This is exactly the situation a denoiser exploits: **decompose noisy = signal + noise, where signal is spatially smooth and noise isn't.** Convergence as N→∞ is `O(1/√N)`. Doubling SPP only cuts noise by ~30%. To go from 1024 SPP to "indistinguishable from converged" you'd need ~16384 SPP. The denoiser short-circuits this — at the cost of some bias.

---

## 2. What kind of model is it

The OptiX denoiser is a **convolutional autoencoder** trained on pairs of (noisy render, ground-truth render) across thousands of NVIDIA-rendered scenes. The model architecture is descended from work by Bako et al. (Disney, 2017) and Chaitanya et al. (NVIDIA, 2017) — "kernel-predicting" or "direct prediction" CNNs for Monte Carlo denoising.

Specifically:

- **Encoder-decoder with skip connections** (a U-Net variant). The encoder progressively downsamples while expanding feature channels, capturing larger-scale structure. The decoder upsamples back to image resolution. Skip connections preserve high-frequency detail that would otherwise be lost in the bottleneck.
- **HDR-aware**. Most denoisers fail catastrophically on HDR data because pixel values span 6+ orders of magnitude. NVIDIA trains the network on a **tone-mapped** version of the input (typically log or μ-law compression) and inverts the mapping on output. That's what `OPTIX_DENOISER_MODEL_KIND_HDR` does in your code — selects the HDR-trained weights.
- **Trained at NVIDIA, not redistributable as raw weights**. The weights ship as part of the OptiX runtime.

There are several model variants:
- `LDR` — for tonemapped 8-bit input.
- `HDR` — what you're using.
- `AOV` — accepts arbitrary auxiliary buffers.
- `TEMPORAL` — exploits frame-to-frame coherence (animations).
- `UPSCALE2X` — denoise + 2× super-resolution in one pass.

You're on `HDR`, which is the right choice for a single-frame thesis renderer.

---

## 3. What happens at `optixDenoiserCreate`

```cpp
optixDenoiserCreate(optix_ctx, OPTIX_DENOISER_MODEL_KIND_HDR, &options, &handle_);
```

This loads the model weights from NVIDIA's runtime into a `OptixDenoiser` handle. The weights are **fixed** — there's no per-scene training. The model is a few MB of float16 weights parameterizing a CNN that takes an `H×W×C` tensor and produces an `H×W×3` tensor.

Your `options.guideAlbedo = 0; options.guideNormal = 0;` says "don't expect guide layers." This tells the model to use a smaller variant trained on radiance-only input. With guides enabled, the network has more channels in its first conv layer (radiance + albedo + normal → ~9 input channels) and produces visibly cleaner edges.

---

## 4. What happens at `optixDenoiserComputeMemoryResources` + `optixDenoiserSetup`

The denoiser needs two buffers:

- **State buffer.** Holds intermediate activations and the loaded model weights in a layout tuned for the GPU. Created once per (model, resolution) pair.
- **Scratch buffer.** Working memory for the forward pass — convolution outputs, im2col tensors, etc.

`optixDenoiserSetup` does a JIT-compile-equivalent step: it specializes the network for your specific resolution. CNN inference is much faster when the spatial dimensions are baked in (allows static shape optimization, fixed kernel launch grids, fused convolutions via cuDNN-style autotuning).

The reason your code synchronizes after setup is that this step issues async kernels that prepare the state buffer.

For an 900×600 HDR denoiser, `state` is on the order of tens of MB and `scratch` is similar.

---

## 5. The actual forward pass (`optixDenoiserInvoke`)

Your code calls:

```cpp
OPTIX_CHECK(optixDenoiserInvoke(handle_, stream_->get(), &params,
                                state_.cu_device_ptr(), state_.size_bytes(),
                                &guide, &layer, 1, 0, 0,
                                scratch_.cu_device_ptr(), scratch_.size_bytes()));
```

Inside, it does roughly this pipeline:

### 5a. Pre-tonemap

Apply a fixed log-like compression: `x' = log(1 + k·x)` or similar. This brings the input into a range the network was trained on. **Why:** ReLU/sigmoid activations saturate or vanish on raw HDR values; the loss landscape at training time was shaped on tone-mapped data.

### 5b. Encoder pass

Walk a series of conv layers, e.g.:

| Stage | Operation | Output channels | Spatial |
|---|---|---|---|
| 0 | Conv 3×3, stride 1, ReLU | 32 | H×W |
| 1 | Conv 3×3, stride 2 (downsample), ReLU | 64 | H/2 × W/2 |
| 2 | Conv 3×3, stride 1, ReLU | 64 | H/2 × W/2 |
| 3 | Conv 3×3, stride 2, ReLU | 128 | H/4 × W/4 |
| ... | ... | ... | ... |

By the bottom of the U, the network is reasoning about 32×32 macroblocks of the image with hundreds of feature channels. This is where the network "understands" what's signal (the cloud shape) vs noise (the salt-and-pepper grain).

### 5c. Decoder pass

Symmetric upsampling back to full resolution, with **skip connections** from each encoder stage concatenated to the matching decoder stage. The skip connections are critical: without them, the bottleneck would smear high-frequency edges (your cloud silhouette would blur).

### 5d. Output projection

A final 1×1 conv produces 3 channels: the denoised RGB. Plus an inverse tonemap to get back to HDR scale.

### 5e. In-place write

You're writing to the same buffer (`layer.input == layer.output`). OptiX handles this correctly — the network reads the input fully into scratch before overwriting.

---

## 6. Why it works (and where the bias comes from)

The network learned a **prior** over what natural rendered images look like:
- Surfaces have piecewise-smooth radiance fields.
- Edges are usually correlated with geometry (and would be sharper with normal/albedo guides).
- Volumetric falloff is smooth in screen space.
- Caustics and specular highlights are localized and bright.

It applies this prior as a **regularizer** on top of your noisy estimate. Mathematically you can think of it as a learned MAP estimator:

$$\hat{L} = \arg\max_L p(L \mid \text{noisy}) \propto p(\text{noisy} \mid L) \cdot p(L)$$

The second term is the prior — entirely learned from training data.

**The bias:** the prior pushes your image toward the manifold of "things that looked like training renders." If your scene contains structures that are out-of-distribution — extreme caustics, single-pixel features, very sparse Gaussian splats — the denoiser will smooth them away or hallucinate something plausible. For your cloud asset this is fine; for, say, a scene full of one-pixel-wide laser beams it would butcher them.

This is why production studios *also* render at high SPP — the denoiser is a quality multiplier, not a replacement for sampling.

---

## 7. Why it's so fast on GPU

For 900×600, denoising takes ~50–100ms on RTX 3090. The reasons:

- **Convolutions map to tensor cores.** Modern NVIDIA GPUs have hardware specifically for `mma.sync` instructions doing 4×4 matmuls in one cycle. CNN inference saturates these.
- **Fixed shapes, fused kernels.** The setup phase pre-compiles the network for your resolution. No runtime shape checks, no tensor allocation in the hot path.
- **Float16 compute.** The model weights and intermediate activations are FP16. Half the memory bandwidth, double the throughput on tensor cores.
- **Single-pass.** Unlike iterative methods (BM3D, NLM), the network is feed-forward — one pass through ~10 conv layers, no convergence loop.

---

## 8. What you'd gain from guide buffers

If you set `guideAlbedo = 1` and pass first-hit albedo + first-hit normal:

- **Albedo:** decouples illumination noise from texture detail. The network can denoise `radiance / albedo` (which is roughly the "lighting") and re-multiply at output. Crisp textures, no blurred patterns.
- **Normal:** tells the network where geometric edges are. Sharp cloud silhouettes, no bleed across boundaries.

For your specific scene this means roughly:
- 1024 SPP + radiance-only denoise → 1024 SPP + AOV denoise: probably 10–20% RMSE improvement, mostly visible at silhouette edges.
- 256 SPP + AOV ≈ 1024 SPP + radiance-only.

You'd compute these AOVs cheaply: at the first scatter event, store the local Gaussian's albedo and the gradient direction (`∇σ_t` — the analog of a "normal" for volumetric primitives, or just the view direction if you don't have a meaningful surface).

This is what Tier 1.1 in `docs/quality-roadmap.md` is about — and it's the highest-ROI thing on the list outside of NEE.

---

## 9. Failure modes to watch for

1. **NaN/Inf in input.** The denoiser does not check; it'll happily propagate NaNs and you get a blank or rainbow output. Always sanitize before invocation.
2. **Sub-1024 SPP on extreme contrast.** With very few samples, fireflies (single-pixel outliers from low-pdf paths) can dominate the network's view of "signal." Either clamp before denoising, or pre-filter fireflies with a max-luminance heuristic.
3. **Over-smoothing of fine detail.** If your scene has Gaussian-scale features below ~3 pixels, the denoiser will erase them. Increase resolution or add guide buffers.
4. **Tonemap mismatch.** The HDR model assumes radiance is in linear (scene-referred) space, roughly 0–1000. If you're feeding it post-tonemap data, results are unpredictable.

---

## 10. Comparison to alternatives

| Method | Quality | Speed | Cost |
|---|---|---|---|
| OptiX denoiser (HDR) | High | ~50ms | Free runtime, no training |
| OIDN (Intel Open Image Denoise) | Slightly higher in some scenes | ~100ms (CPU)/ ~80ms (GPU) | Free, open source |
| BM3D | Lower | ~5s (CPU) | Free, no learning |
| Custom-trained kernel-predicting CNN | Highest in-domain | Similar | Requires data + training |
| Brute-force SPP | Highest, unbiased | Hours | $$$ |

For a thesis renderer, OptiX is the right choice — it's free, fast, integrated, and HDR-aware. Adding OIDN as a comparison baseline would be a respectable thesis bullet point ("we evaluated against Intel OIDN and found OptiX faster but OIDN slightly cleaner on cloud silhouettes" or whatever you find).

---

## TL;DR

A pre-trained U-Net CNN runs in ~50ms on the GPU. It takes your noisy HDR render, applies a tonemap, runs encoder-decoder convolutions to separate signal from per-pixel grain using a learned prior, and inverts the tonemap. It's biased (toward "looks like training data") but for natural scenes the bias is invisible. Adding albedo+normal guide buffers is the single biggest quality lever you can pull on top of what you have.
