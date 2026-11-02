# Phoenix frozen Draft PR review

No additional color tuning, parameter changes or new Phoenix CSV package was
introduced while preparing this PR. Physical-film fidelity remains unvalidated.

## Scope and source identity

The PR is based on `codex/pr2-stability-colour-review-20261031` at
`1bc1066c683e1b281c7eb888096d5b6b8f3ebe78` (Draft PR #3), not `main`.
Phoenix implementation commit: `f7e994fd`; exact-byte LF export fix: `7119ab4e`.
The latter only adds `.gitattributes` for hash-bearing Phoenix data. It changes
no numeric data, fit coefficients, effects, exposure anchors or print packs.

The archived PR tree excludes the other pending automatic negative inversion,
negative TIFF and Cineon output work. In particular, this PR does not modify
`FrameOutput.cpp/.hpp`, `PreviewController.cpp`, `EditStore.cpp`, `qml/Main.qml`,
`desktop/preview_host.cpp/.hpp`, Vulkan DI code or their unrelated tests.
They are baseline/reused paths, not Phoenix changes.

Original-source PDFs and source chart PNGs remain local ignored research
artifacts with their URL/hash records; the PR includes digitized data and
reproducible extraction scripts, not a new third-party CSV data pack.

## Isolated PR-tree results

An archive of the PR tree was expanded into `build/phoenix-pr-source`, built
separately into `build/phoenix-pr-clean` with Qt 6.11.3 / GCC 13.1 / Vulkan.
The following seven selected tests passed with **0 failures**:

- `spk_qt_phoenix` — 3,281 checks.
- `spk_phoenix_model_data` — 3,254 checks.
- `spk_phoenix_model_counterfactual` — intentionally injects NaN; expected rejection.
- `spk_qt_frame_output`.
- `spk_qt_material_workflow`.
- `spk_core_schema`.
- `spk_core_setup`.

Run the normal Windows CMake configuration/build, including targets
`spk_qt_phoenix_test`, `spk_qt_frame_output_test`,
`spk_qt_material_workflow_test`, `spk_dump_json`, `spk_dump_setup`, then:

```powershell
ctest --test-dir <build> --output-on-failure -R '^(spk_qt_phoenix|spk_phoenix_model_data|spk_phoenix_model_counterfactual|spk_core_schema|spk_core_setup|spk_qt_frame_output|spk_qt_material_workflow)$'
```

Actual output is preserved in [ctest-summary.txt](ctest-summary.txt) and
[ctest-details.txt](ctest-details.txt). This is **7 selected tests**, not a
claim that all 40 were rerun on the isolated PR tree. The earlier integration
workspace had 40/40 passing but contained unrelated pending changes; its
separate historical report is [phoenix-20261102.md](../phoenix-20261102.md).

## Frozen-image and output evidence

These are owned synthetic gray/color/highlight fixtures, not photographed film
or measured ColorChecker patches. The four PNGs from the isolated PR were
compared byte for byte with the original integration build's evidence and all
were identical. SHA256:

| Image | SHA256 |
|---|---|
| [Input](images/input.png) | `ca8d89ab09b11257609a326ade302d4c73cd0f3a7c30604ecafc22fa95496783` |
| [Portra control](images/portra.png) | `86e3738dc8ca846a2d3fec556e0918aa7c45567d8b6cf999448d423d9b25f61b` |
| [Phoenix 200](images/harman_phoenix_200_exp_v1.png) | `aeb23dfae33c34efd221ee134815075810760949f2b3c4ec87e9ed271a82a96f` |
| [Phoenix II](images/harman_phoenix_ii_200_exp_v1.png) | `96f34d81bb2bea76c4d30cbac8125949230c7fe4192885a080701e53036cef8f` |

The native test writes TIFF/PNG8/PNG16/JPEG through the controller. Its separate
preview/export comparison requires equal dimensions and PNG8 mean absolute RGB
error below 0.6 code values; it does not assert exact pixels at different
resolutions or with stochastic effects. Both saved/reopened previews report
`preview difference=0` and equal settings. Portra round-trip restoration is exact.

## Accuracy boundary

The gray-ramp folding and inherited print-pack bugs have reproducible software
fixes. This does not establish the correctness of sensitivity-scale interpretation,
absolute exposure, first-generation shadow completion, the second-generation
candidate curve, Gold dye spectra, modeled purple base, synthetic densitometer,
DIR coupling, grain statistics or halation PSF. Actual negative reversal and
scanner behavior, real scenes and physical-film comparisons remain unverified.
See [the model record](../../engine/resources_research/PHOENIX_MODELS.md).

This is a frozen experimental implementation for independent review. No PR
merge, automatic merge, new CSV acquisition or email contact is authorized here.
