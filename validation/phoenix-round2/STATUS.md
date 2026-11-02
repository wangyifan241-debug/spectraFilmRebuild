# Execution status — partial research, not final acceptance

Infrastructure base: 00405ee8ad5a448b09e64b1bf9195f1bb429f9a9 (pushed PR6 repair). Parent renderer/material commit: ba076b2befe6586ad3095f897ebf40ea7e9ffd6c.

Five spectral candidates passed PR6 category validation. Seven actual native Vulkan fixture executions (two parents plus five candidates) succeeded; complete driver results are in native-execution.json. The driver exercises repeated rendering, cached reprint and result lifetime. These are not GUI preview/export tests. Executable hash is recorded; the prebuilt engine DLL source provenance has not been independently reconstructed, so do not assign a complete engineering Pass.

The synthetic fixture contains a -6..+6 EV neutral ramp and skin-like RGB strips in linear ProPhoto RGB. PNGs are sRGB RGB8 visualizations of native RGBA16 results. output-differences.json contains encoded RGB model differences, not photographic accuracy. It is not a 24-patch ColorChecker and is not a RAW image.

Important discovered interface mismatch: frozen-recipe.v1.json uses film_exposure_ev (a UI setting). C ABI rejects that name. This round explicitly maps it to exposure_compensation_ev and disables auto_exposure. Parent and candidate use identical mapping; no changes were made to PR6 files.

Decision for all five candidates: Inconclusive. Retain v1 for product use. Source calibration, logarithmic ordinate, illuminant correction and relative channel speeds remain unknown.

Outstanding required delivery: D55 spectral table/formula provenance and candidate; II monotone bounded width alternatives; fixed authentic RAW decode/render; 24-patch D50 Lab/Y analysis; full exposure series; native preview/export consistency and unaffected-stock regressions. No claim that round 2 is complete. No PR4/PR6 merge or product registration of research materials.

Scripts currently run from repository root. run-native.cjs uses a local prebuilt binary path and is an execution reproduction aid, not a portable renderer distribution. Candidate plans intentionally omit renderer and recipe bindings until the complete fixed-input validation is configured.

Update: two II width alternatives (1.15 and 1.30) were added and both actually rendered successfully. Nine native executions now include two parents and seven candidates. Width changes preserve monotonicity and density bounds by rescaling the exposure coordinates and Gaussian centers/sigmas together. D55, authentic RAW, 24-patch Lab and GUI regression gates remain outstanding.
