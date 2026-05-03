# SynthID Image Watermark Research Report

> **Author**: Allen Kuo
> **Date**: December 2025 (Updated after reading Gowal et al., 2025)
> **Status**: Ongoing Research
> **Conclusion**: Removing SynthID without significantly degrading image quality remains very difficult in practice.

---

## Update Notice (May 2025)

This document was originally written based on **black-box experiments and reverse engineering**. When I started this research in late 2025, the publicly available material on SynthID consisted mostly of Google's announcements and blog posts — there was no detailed technical documentation for outsiders. Reverse engineering was the only practical path.

Since then, Google DeepMind has published a detailed paper, **"SynthID-Image: Image watermarking at internet scale" (Gowal et al., 2025)**, which I had not been actively tracking. A community member kindly brought it to my attention (see Issue #32), which I appreciate — this is the kind of free update every research note benefits from.

After reading the paper, I have revised several theoretical claims in this document:

- I described SynthID-Image as **ad-hoc** (intervening during diffusion generation via Tournament Sampling). According to the paper, **SynthID-Image is post-hoc and model-independent**, using an encoder-decoder approach.
- The "Tournament Sampling" concept I referenced applies to **SynthID-Text** (Dathathri et al., 2024), not to SynthID-Image. I conflated the two.
- "SynthID" is best understood as a **family of watermarking technologies** (text, image, audio, video) using different underlying mechanisms, not a single unified algorithm.

The experimental observations in this report remain valid as black-box findings. The theoretical framing has been revised to align with the paper.

---

## Executive Summary

This document records my reverse-engineering research into Google's **SynthID** invisible watermarking technology, specifically the image variant deployed in Gemini.

After extensive experimentation with dozens of attack strategies, I found that **SynthID watermarks cannot be removed without significantly degrading image quality**. The original theoretical explanation has been revised based on the official paper: SynthID-Image is a post-hoc encoder-decoder system, not an ad-hoc generation-time intervention.

### Key Practical Finding

> Whether the watermark is "in" the image or "added to" the image, the practical result is the same: removing it without quality loss is extremely difficult, because the encoder is trained to embed the watermark in a way that is robust to most common transformations.

---

## 1. What is SynthID?

SynthID is Google DeepMind's invisible watermarking technology for AI-generated content. It is deployed across Google services including Gemini, Imagen, Lyria, and Veo.

Importantly, **"SynthID" is a brand covering multiple modalities**, each using different underlying techniques:

| Modality | Approach | Reference |
|----------|----------|-----------|
| **SynthID-Text** | Ad-hoc (Tournament Sampling on tokens) | Dathathri et al., Nature 2024 |
| **SynthID-Image** | Post-hoc (encoder-decoder) | Gowal et al., 2025 |
| **SynthID-Audio** | Likely post-hoc (limited public details) | — |
| **SynthID-Video** | Likely post-hoc (limited public details) | — |

This research focuses on **SynthID-Image**.

### Key Properties (from the paper)

- **Invisible**: Imperceptible to human eyes
- **Robust**: Trained to survive common image transformations
- **Post-hoc**: Applied after generation by an encoder
- **Model-independent**: Works with any generative model output

---

## 2. How SynthID-Image Works (Revised Understanding)

### 2.1 Post-hoc Encoder-Decoder Architecture

According to Gowal et al. (2025), SynthID-Image uses an encoder-decoder approach:

```
Original image x
       ↓
   Encoder f
       ↓
Watermarked image x' = f(x), where d(x, f(x)) ≤ ε
       ↓
   Decoder g
       ↓
   Detection: g(x') ∈ {±1}
```

The encoder `f` is a neural network trained to add a watermark perturbation that:
1. Is perceptually close to the original (`d(x, f(x)) ≤ ε`)
2. Is detectable by the decoder `g`
3. Is robust under a distribution of transformations `T`

This is fundamentally a **learned perturbation**, not a fixed algorithmic transform.

### 2.2 Training Objective (from the paper)

The robust watermarking problem is formulated as:

```
min  E[ℓ(g(τ(f(x))), +1) + ℓ(g(τ(x)), -1)]
f,g
s.t. d(x, f(x)) ≤ ε
```

Where `τ` is sampled from a distribution of valid transformations. This means the encoder is trained against augmentations like noise, compression, rotation, color changes, etc., making the watermark robust to those specific attacks by construction.

### 2.3 Why the Watermark is Hard to Remove

Although the watermark is technically "added" by the encoder (not part of the generation process), it has properties that make it behave almost like an ad-hoc watermark in practice:

1. **The perturbation lives in a high-dimensional learned representation**, not in simple pixel patterns.
2. **The encoder is adversarially trained** — it has effectively pre-computed defenses against common removal attacks.
3. **The watermark distributes across the image** based on content, not at fixed locations.
4. **Random or symmetric attacks tend to be orthogonal** to the encoder's learned direction.

### 2.4 Original Hypothesis (Now Refined)

> **Original Hypothesis**: SynthID is a "statistical bias" embedded during generation, and the watermark IS the image itself.
>
> **Refined Understanding**: SynthID-Image is added by an encoder, but the encoder is trained to embed the watermark deeply into the image's learned features. The practical effect is similar — the watermark is hard to separate from the image — but the underlying mechanism is post-hoc, not ad-hoc.

---

## 3. Experiment Results Summary

These experimental observations were collected through black-box testing against Google's SynthID detection tool. They remain valid regardless of the corrected mechanism understanding.

### 3.1 Failed Methods (Do NOT Work)

| Method | Observed Result | Likely Reason |
|--------|----------------|---------------|
| Negative / Grayscale | Watermark detected | Linear transform preserves features |
| Physical re-capture (camera) | Watermark detected | Encoder trained on similar transforms |
| Gaussian / Uniform noise | Watermark detected | Encoder trained on noise augmentation |
| Geometric perturbation | Watermark detected | Encoder trained on rotations / flips |
| Frequency domain attacks | Watermark detected | Encoder operates on full feature space |
| Fake watermark injection | No effect on detection | Fake patterns orthogonal to learned key |
| AI Super-resolution | Watermark detected | Faithful restoration preserves features |
| Downscale-upscale | Watermark detected | Encoder trained on resize transforms |
| Iterative dilution (1000+ rounds) | Watermark detected | Random perturbations average out |

### 3.2 Partially Successful Methods

| Method | Result | Notes |
|--------|--------|-------|
| Laplacian noise | "Part of" | Sparse destruction, ~30% affected |
| AI Repaint (low denoising) | "Part of" | Some bias retained from original |
| Cross-AI repaint (careful tuning) | Unstable | Narrow "golden zone" between detection and visible change |

![Preview](removed_id_case_01.png)

### 3.3 Successful Methods (With Significant Trade-offs)

| Method | Result | Trade-off |
|--------|--------|-----------|
| True binarization (1-bit) | Removed | Image becomes a skeleton |
| Extreme destruction (PSNR < 25dB) | Removed | Heavy artifacts, image barely usable |
| AI Deep Repaint (denoise > 0.7) | Removed | Style and details change significantly |

![Preview](removed_id_case_02.jpg)

These successful methods align with attack vectors that the paper itself identifies as relevant threats:

- **Quantization** is listed as a robustness challenge in §4
- **Re-generation attacks** are explicitly listed in §6 as a known threat model

So my experiments are best understood as **empirical validation of the paper's own threat model**, not as discoveries of unknown weaknesses.

---

## 4. The Impossible Triangle

```
                Visual Fidelity
                      △
                     / \
                    /   \
                   /  X  \
                  / Cannot\
                 / achieve \
                /   all 3   \
               /_____________\
        Remove SynthID    Stable/Repeatable
```

This trade-off is consistent with the paper's discussion of the **quality-robustness-payload trade-off**. In practical terms:

- You can preserve quality and remove the watermark, but only sometimes (unstable).
- You can stably remove the watermark, but at the cost of visible quality loss.
- You can preserve quality and have a stable result, but the watermark stays.

A common framing:

> "If you want the painting, you keep the signature. If you want to remove the signature, you change the painting."

---

## 5. Removal Approaches

Based on my experiments and the paper's threat model, three categories of removal approaches are worth discussing.

### Path 1: Destroy the Canvas (Quantization Attack)

```python
# Extreme quantization eliminates the continuous values the encoder relies on
_, binary = cv2.threshold(gray, 128, 255, cv2.THRESH_BINARY)
```

- **Pros**: Reliably defeats detection
- **Cons**: Image becomes unusable (lines / outlines only)
- **Use case**: Line art, text documents
- **Why it works**: The encoder was trained on continuous-tone images, not binary ones. Binary quantization is essentially out-of-distribution.

### Path 2: Change the Painter (Re-generation Attack)

```
Use another image generation model (Stable Diffusion, etc.) to repaint
the image with high denoising strength (> 0.7).

The new image is produced by a different encoder/decoder pair, so
SynthID's encoded perturbation is mostly washed out.
```

- **Pros**: Output remains visually plausible
- **Cons**: Style and fine details change
- **Use case**: When approximate visual similarity is acceptable
- **Why it works**: Listed by the paper itself as a known threat (§6)

### Path 3: Adversarial / Surrogate Attacks (Theoretical)

Since SynthID-Image is post-hoc, in principle it is exposed to attack vectors that pure ad-hoc systems would not be:

1. **Adversarial perturbations** against the decoder (requires decoder access)
2. **Surrogate model training** with paired (clean, watermarked) data, then transferring attacks
3. **Latent-space inversion** to find the encoder's perturbation direction

In practice, all of these are gated by:

- Limited / no white-box access to Google's encoder/decoder
- Lack of large-scale paired data
- The encoder's adversarial training (defends against the easier transfer attacks)

So while post-hoc theoretically opens more attack surface than ad-hoc, the production system is hardened against the obvious ones.

---

## 6. Why Post-hoc Doesn't Make Removal Easy

A natural follow-up question: if SynthID-Image is post-hoc, isn't there always a "clean" image x that could in principle be recovered from x' = f(x)?

In theory yes. In practice, several factors make this very hard:

1. **High-dimensional learned perturbation**
   - δ = f(x) - x is not a fixed pattern.
   - It depends on x, on the payload, and on training-time randomness.
   - There is no closed-form way to express δ in terms of x.

2. **Adversarial training of f**
   - The encoder explicitly optimizes against being undone by common transforms.
   - Most "obvious" attacks were already part of the training augmentation set.

3. **Robustness via redundancy**
   - The watermark is distributed across the image, not localized.
   - Local edits or filters do not cleanly remove it.

4. **No black-box → white-box shortcut**
   - Without access to f's weights, attacks are limited to transfer attacks and queries.
   - The detection API is rate-limited and provides only coarse feedback.

So even though "post-hoc" opens more *theoretical* attack vectors, the system was designed knowing this, and most of those attacks have already been hardened against.

---

## 7. Mathematical Sketch

The paper's formulation can be summarized as a constrained optimization:

```
min  E_{x, τ} [ℓ(g(τ(f(x))), +1) + ℓ(g(τ(x)), -1)]
f, g
s.t. d(x, f(x)) ≤ ε
```

In words: train an encoder that adds an imperceptible perturbation, and a decoder that detects it, even after a wide range of transformations are applied.

For an attacker, the goal is essentially the inverse:

```
Find x* such that:
  d(x*, x') ≤ small      (still looks like watermarked image)
  AND g(x*) → -1          (decoder no longer detects)
```

For most simple attacks (random noise, filters, etc.), the perturbation introduced is roughly orthogonal to the encoder's learned direction, so it does not move `g(x*)` across the decision boundary. Only attacks that significantly disrupt the image's high-level structure (regeneration, quantization) tend to flip the decoder.

---

## 8. Conclusions

### What I Learned (Updated)

1. SynthID-Image is **post-hoc and model-independent**, per the official paper.
2. Despite being post-hoc, the encoder is trained to embed the watermark deeply enough that simple removal attacks fail.
3. Successful removal generally requires either heavy degradation (quantization) or regeneration through a different model — both of which are listed in the paper's own threat model.
4. The "watermark is the image" intuition I had originally was technically incorrect, but practically reflected the right outcome: the watermark is hard to separate without disturbing the image.

### Recommendations

| Goal | Recommended Approach |
|------|---------------------|
| Remove for line art / text docs | Binarization |
| Remove for photographs | AI repaint with high denoising strength |
| Remove without any quality loss | Not currently feasible with public tools |

### Future Research Directions

1. **Quantization threshold mapping**
   Find the minimum bit depth or color count where SynthID detection consistently fails.

2. **Dithered quantization**
   Test whether perceptually-aware dithering can keep visual quality while disrupting the encoder's features.

3. **Selective region processing**
   Process only high-entropy regions where the encoder has the most "room" to embed the watermark.

4. **Surrogate detector training**
   If a publicly accessible detector becomes available, training a surrogate could enable transfer-based adversarial attacks.

5. **Cross-modal observations**
   The Veo (video) watermark exhibits strong frame-to-frame variation, which is more consistent with a post-hoc encoder than a generation-time intervention. More systematic study across SynthID modalities could be useful.

---

## 9. References

1. **Gowal et al.** "SynthID-Image: Image watermarking at internet scale." arXiv:2510.09263 (October 2025) — The authoritative technical reference.
2. **Dathathri et al.** "Scalable watermarking for identifying large language model outputs." Nature 634, 818-823 (2024) — SynthID-Text (the ad-hoc Tournament Sampling system).
3. **Bui et al.** "RoSteALS: Robust steganography using autoencoder latent space." CVPR 2023 — Prior post-hoc encoder-decoder watermarking.
4. **Tancik et al.** "StegaStamp: Invisible hyperlinks in physical photographs." CVPR 2020 — Foundational post-hoc watermarking.
5. **Wen et al.** "Tree-Ring Watermarks: Fingerprints for Diffusion Images that are Invisible and Robust." NeurIPS 2023 — Ad-hoc watermarking (originally cited in my earlier analysis, and partly the source of my initial misattribution).
6. Google DeepMind. "SynthID: Identifying AI-generated content." https://deepmind.google/technologies/synthid/

---

## Disclaimer

This research is for **educational and academic purposes only**. The goal is to understand the robustness of AI watermarking technologies, not to facilitate misuse.

SynthID represents a significant engineering achievement in AI content authentication. My experiments confirm its resilience and validate the paper's design goals. The corrections in this updated document are made in the spirit of accurate research; the experimental findings remain useful as black-box observations, while the theoretical framing now properly defers to the official paper.

---

## Related

- [SynthID Image Watermark Research Report (Medium)](https://allenkuo.medium.com/synthid-image-watermark-research-report-9b864b19f9cf) — *will be updated to reflect the corrections in this document.*

---

*Last Updated: December 2025*
