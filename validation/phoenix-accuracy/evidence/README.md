# Infrastructure v1 execution evidence

- `node-tests.tap`: fresh `node --test tools/phoenix-validation/tests.cjs` run,
  27/27 infrastructure tests; no real-film measurements. Fake-renderer fixtures
  test staging, artifact binding and comparison/report flow only.
- `negative-control.json`: deliberately broken constant-zero ΔE00 rejected by
  all seven selected published numeric vectors. No repository code was replaced.
- `../reports/infrastructure-v1-final/`: versioned HTML/JSON; 64 frozen files checked,
  two frozen Phoenix PNGs identical, current research gate 3,254 checks.
- The native 7/7 evidence is the **historical, hash-pinned PR #4** receipt in
  `validation/phoenix-review-20261102/`, not a new full native suite run.

No engine/kernel/material/UI/output changes were made, so this increment runs
the offline tooling gate and reuses existing native coverage rather than repeating
or extending the gray/exposure/synthetic patch/preview-export implementations.
Windows/macOS builds, new native renders, real scans and photographic fidelity
were not newly validated. The shipped physical manifest contains zero samples;
the photographic gate is **Not Verified**, with null metrics.

CI additionally runs the same offline suite on Windows and Ubuntu and uploads
fresh HTML/JSON. CI results are separate from this local execution receipt.
