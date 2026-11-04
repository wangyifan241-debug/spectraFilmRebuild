# Phoenix round 4 — diagnosis (2026-11-03)

Status: NOT VERIFIED; source audit only. No running Windows binary, resolved render capture, fixed RAW experiment or real-film measurement has been verified in this report.

Baseline HEAD: 00405ee8ad5a448b09e64b1bf9195f1bb429f9a9. Working tree contains pre-existing uncommitted rendering/export changes; HEAD does not identify its executable.

## Evidence and classification

- Material/model uncertainty: experimental Phoenix v1 metadata explicitly identifies Gold 200 proxy dyes, unmeasured purple base, assumed exposure anchor and synthetic densitometer. Phoenix II characteristic generation attribution is unresolved. These are hypotheses, not measured Phoenix capabilities. Neither material was changed.
- Output-path interpretation: pipeline.cpp node_scan_spectral integrates transmission; node_bw_correction bypasses negative direct scans. Direct scan is therefore not a normalized positive scan. This is a path-contract distinction, not proof of a channel bug.
- Existing digital-intermediate path separately removes base spectrally, reverses neutral curves and applies colour/log encoding. Its Cineon output cannot be interpreted as sRGB. Audit the consuming surface before blaming a material.
- Fixed Internal Gain: third-round results were not located in the initial inventory. No conclusion or default is inferred from that unavailable evidence.
- Runtime/resource provenance, ICC conversion, channel ordering, curve interpretation and actual dominant cause: NOT VERIFIED. No measured root cause or colour error is claimed.

## Required reproducible experiment

Capture executable, DLL, resource and profile absolute paths and SHA256; full resolved parameters after defaults/sidecars; input RAW and decoded linear ProPhoto hash, decoder settings and ICC. Freeze one RAW, a documented chart, neutral ramp and exposure bracket. Disable stochastic effects for comparisons. Preserve unclipped float buffers and per-stage min/max, finite counts, quantiles and patch means at decode, exposure, developed layer density, spectral transmission, print exposure/density, scanner XYZ, output linear RGB, unsharp, gamut mapping and encoded output. Hash every buffer and recipe.

Run paired ablations of automatic print compensation, Endura, scanner correction, unsharp, gamut mapping and ICC, changing one stage each time. Keep original Optical Print and v1 as controls. Compare raw transmission, reference inversion and DI with explicitly different encodings. Report D50 Lab CIEDE2000 only after known source-white adaptation and matched patch correspondence; include luminance, hue, chroma, gray balance and pre-clamp clipping ratios. Software differences do not validate film fidelity.

Pending: instrument running app, reproduce fixed-frame failure, collect intermediate buffers, prove preview/export agreement, and identify dominant cause. This report is an initial diagnostic protocol, not completed phase-one acceptance.
