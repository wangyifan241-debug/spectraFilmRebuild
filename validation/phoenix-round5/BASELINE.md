# Round five baseline, 2026-11-03

PR #11 is OPEN / DRAFT, not merged. Head 34498abe26aa2624ebf5499d4d17ca47fbdb159b; base codex/phoenix-round4-frozen-source at 00405ee8. This branch builds on that head and does not alter PR #11. The original workspace has unrelated dirty files and was left intact.

Both generations already have independently digitized sensitivity charts. Their printed relative ordinate has unresolved linear/log interpretation; v1 assumes log sensitivity. Both use Kodak Gold 200 proxy dyes, a synthetic purple base and synthetic Gaussian densitometer. Phoenix II characteristic data is a secondary candidate whose caption says Phoenix I: generation remains unresolved. No public sensitivity curve is a measured dye absorption curve.

Optical Print couples negative density, Endura response and automatic print gain. Round-four scanning separates these through spectral base removal, a fixed neutral inverse and a local neutral Jacobian. It is an idealized instrument, not actual hardware. A local derivative need not describe saturated colours or multiple exposure levels. Round five investigates that output conversion without changing material data.

Official current first-generation PDF is September 2024 (7 pages), spectral chart page 2, tungsten 2856 K. Existing first-generation digitization instead pins November 2023 (6 pages): do not relabel its hash or CSV as 2024. Second generation PDF is July 2025 (5 pages), spectral chart page 2, D55. It contains no characteristic curve. Existing CSVs and digitization metadata remain in engine/resources_research/phoenix200 and phoenix2. Pixel quantization, stroke width and crossings are uncertain; no certified material-measurement uncertainty exists.

Baseline run-tests.cjs requires executable, resource directory and temporary directory. Initial missing-argument failure was corrected. The actual GPU run passed spectral oracle, deliberately broken integration rejection, neutral reversal, C ABI Optical Print control, PNG/TIFF16 agreement and ICC checks. Log: baseline-tests.txt. This is software evidence, not film fidelity.

Round-four and round-five executables are independent research viewers. Main Windows editor integration must be reported separately. Neither monitor calibration nor real-film physical accuracy has been independently verified.
