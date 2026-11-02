# SpektraLab — THIRD_PARTY_BENCHMARK

**Independent pre-research report | 2026-11-02 | Scope: Phoenix 200 (I) and Phoenix II 200 | Status: research/design only**

## 0. Executive decision

- **DxO FilmPack 8:** confirmed *Phoenix 200 (I)* stock in official release information. **Phoenix II not confirmed**. Primary commercial cross-engine reference for first generation, not a color ground truth.
- **Ashigrain:** confirmed *Phoenix 200 (I)*, a reconstruction openly combining HARMAN chart information with proxy/assumed data. **Phoenix II not found in reviewed stock list**. Useful methodological reference and independent rendering for first generation.
- **The Grain Archive:** confirmed presets for *both* Phoenix 200 and Phoenix II 200 in darktable, Photomator and Lightroom. Creator explicitly calls the Phoenix II darktable product **an interpretation, not a physical emulation**. Best current second-generation external visual-style reference, not a ground truth.
- **Open-source tooling:** `darktable` (image processing, native module styles), `spektrafilm` (spectral simulation architecture), `OpenColorIO`, `colour-science`, `ArgyllCMS` and `rawpy/LibRaw` (experiment and color-management infrastructure). No publicly verified *measured Phoenix II film profile* was established in this investigation.
- **No third-party image exports were generated.** No trial installed or purchased; no commercial LUT, program asset or protected sample downloaded. Permissions for commercial use of derived output / benchmarking require checking the applicable EULA and, when unclear, rights-holder confirmation.

Evidence levels in this report: **A** = developer/manufacturer first-party statement or documentation; **B** = third-party documented implementation with method caveats; **C** = community example or proposed test, without validated original-film ground truth. `confirmed` means the feature was found in a cited current product page; it does **not** mean that its model is physically accurate.

## 1. Software matrix (research date: 2026-11-02)

| Software | I | II | Model / evidence | Input/output and reproducibility | Availability / permission | Confidence / role |
|---|---|---|---|---|---|---|
| **DxO FilmPack 8** | **Confirmed** by DxO's V8 release note | **Not confirmed**; absence from highlights is not proof it cannot be added | DxO describes real film profiling using calibrated charts, exposed subjects, development, high-resolution scans and film response/grain measurements; no Phoenix-specific measured data or downloadable curves disclosed | Official FilmPack guide confirms standalone RAW, DNG, JPEG, TIFF 8/16-bit RGB input. Output 16-bit TIFF and ICC configuration for this precise application/version must be established in a live trial (PhotoLab export documentation is not equivalent). Manual control of automatic corrections and extra effects must be audited in UI | Official unrestricted 30-day trial, Windows/macOS; price/license on official shop. Public EULA details governing competitors' output comparison not confirmed here | **A** as product and method claim; recommended independent **I** output benchmark, never truth |
| **Ashigrain** | **Confirmed** | Not found in reviewed stock/parameter list; not established | Manufacturer curves and named proxies; published documentation exposes approximation limits, light model, and film workflow. Specific substitutes need to be checked against relevant version of the per-film technical page before restating | DaVinci Resolve film plugin; parameter docs cover film stock, print stock, illumination, grain, printer lights, halation, film gauge. Exact image export / free-trial limitations not experimentally checked | Site advertises Try free. License on its EULA page must govern use; do not copy internals | **A/B** independent physical-style reconstruction for **I** |
| **The Grain Archive** | **Confirmed** | **Confirmed** | darktable style uses native modules; II derived from HARMAN datasheet and reviews, author explicitly says *interpretation, not emulation*. Photomator/Lightroom styles may bake look to 3D LUT | darktable 5.6+ `.dtstyle`; Photomator `.adjustments`; Lightroom `.xmp`. Example before/after publicly viewable. Same-input tests possible if licensed styles installed; darktable baseline exposure and tone mapping must be locked | Commercial preset pack listed at $20 at time of research. Site images and preset files not assumed reusable | **A/B** **I + II visual-look reference**, not material-spectrum benchmark |
| **Shaka1277 ECN-2 Phoenix preset** | **Confirmed** first-generation project | Not confirmed | Author describes mild color-correction Lightroom preset specifically for Phoenix developed in ECN-2; **not** comparable directly to normal C-41 production output | Published linked preset; Lightroom host and process limitation | Public page, permission for redistribution/reuse not established | **C** special-process example only; not a baseline |
| **spektrafilm (open source)** | Engine/material infrastructure; **not claiming Phoenix profile** | No verified Phoenix II stock established | Spectral engine and documented profile architecture | Relevant for testing physics & provenance rather than independent implementation of Phoenix | Follow repository license and profile data licenses separately | Algorithmic reference, not independent true color |
| **darktable (open source)** | Hosts licensed third-party Phoenix styles | Hosts licensed third-party Phoenix II styles | Open-source processor; Phoenix behavior comes from external style, not stock darktable | `darktable-cli` batch rendering and pinned versions useful | Separate program license from third-party preset copyright | Reproducible host only |

### Primary source URLs

1. DxO 8 release, explicit Phoenix I, method claim: https://www.dxo.com/en/news/introducing-dxo-filmpack-8/
2. DxO product + unrestricted trial: https://www.dxo.com/en/dxo-filmpack/
3. DxO user guide (file formats): https://userguides.dxo.com/en/filmpack/system-requirements-installation-file-formats
4. DxO guide, laboratory profiling process: https://userguides.dxo.com/en/filmpack
5. DxO store / system requirements: https://shop.dxo.com/en/dxo-filmpack-8.html?gv_os=1&product=FPES
6. Ashigrain Phoenix I page: https://ashigrain.com/films/?film=harman_phoenix_200
7. Ashigrain stock guide / stated proxies: https://ashigrain.com/docs/stocks/
8. Ashigrain parameters: https://ashigrain.com/docs/parameters/
9. Grain Archive presets for both models: https://thegrainarchive.com/presets/
10. Grain Archive Phoenix II film overview: https://thegrainarchive.com/films/harman-phoenix-ii-200/
11. Grain Archive II style method, sample measurements and limitations: https://thegrainarchive.com/styles/harman-phoenix-ii-200/
12. Shaka1277 special-process Phoenix first-generation note: https://www.shaka1277.com/phoenix
13. Open engine: https://github.com/andreavolpato/spektrafilm
14. darktable: https://github.com/darktable-org/darktable
15. Color science library: https://github.com/colour-science/colour
16. OCIO: https://opencolorio.org/
17. ArgyllCMS: https://www.argyllcms.com/

**Claim limitations:** Website listing/advertising does not establish color accuracy. A 16-bit TIFF *input* spec does not establish output capability or wide-gamut ICC preservation. DxO's PhotoLab TIFF output guide does not prove FilmPack standalone exports the same way. An available trial does not constitute authorization to reverse engineer or redistribute profiles. No direct third-party source has been licensed or downloaded for model fitting.

## 2. Comparison target and identity

**Three distinct questions must never be conflated**:

1. **Software–software difference:** SpektraLab vs DxO/Ashigrain/Grain Archive for the *same processed scene*. Measurable but not equal to truth.
2. **Independent implementations' agreement:** if programs resemble each other, that only creates a corroboration clue (their assumptions might also be shared).
3. **Real-film fidelity:** simulation compared to an independently acquired, consented and documented Phoenix negative/scanned positive under a specified C-41/scanning/rendering target. Not measurable from software agreement alone.

**Render target contract:** choose whether the output emulates (a) physical negative transmittance, (b) defined lab scanner/negative conversion, or (c) print-paper projection. DxO/Ashigrain/Grain Archive outputs are creative positives and may represent different print/scan conditions. Record this mismatch; do not subtract their RGB as if physically equivalent without qualification.

## 3. Experiment design — controlled identical inputs

### 3.1 Fixture selection

Use a lawful fixed Sony ARW, plus generated neutral ramp, RGB primaries, color-chart-like 24 synthetic patches (do **not** call synthetic patches a physical ColorChecker), controlled +/−2 EV variants, skin/foliage/sky/white fabric scenes and nighttime point lights.

**Canonical conversion**: decode ARW once in a documented LibRaw/darktable version, fixed demosaic, WB, exposure, noise reduction, color matrix, highlight treatment and no creative adjustments. Export a 16-bit integer TIFF with embedded, known ICC (e.g., wide gamut linear TIFF if every tool correctly accepts its ICC; otherwise a common display-referred 16-bit TIFF with explicit transfer function). Save both variants if needed. Store source ARW SHA256, demosaic software + commit/version, TIFF SHA256, color primaries, whitepoint, transfer function, ICC hash.

Third parties must ingest the same TIFF whenever supported. If one cannot safely interpret a linear TIFF, use a common tagged non-linear TIFF and record the compromise. Do **not** assume the same ARW opened separately in each application yields identical source colors.

### 3.2 Locked settings / run evidence

Capture per product/version:
- installation version, platform/build, license/trial type, preset and stock/version;
- input TIFF checksum, ICC and transfer; scene exposure/WB;
- disabled/controlled automatic corrections, grain, bloom/halation, sharpening, noise reduction, vignetting, tone curve;
- print/scan mode and paper where exposed, printer light/white balance and output color settings;
- output bit depth, embedded ICC, dimensions, file SHA256, GPU/render quality, reproducibility notes;
- legal right or permission to retain/analyze the exported image; no source preset or binary included in repository.

Run two experiment tracks:
- **Color-only:** spatial effects disabled if possible, identical calibration intent and output color space. Separate *fixed-print* comparison from *each tool's default-look* comparison. Do not pretend proprietary tools expose identical print settings.
- **Full-look:** native/declared grain and halo effects; spatial metrics at native resolution and calibrated physical scan scale only if appropriate. No pixelwise equality expectation for random grain.

Replicate at minimum 3 times for nondeterministic output to measure rendering variance. For exposures, use input variations constructed upstream, not each tool's own EV slider unless transfer function matching has been verified.

### 3.3 Metrics

- Color: same registered ROIs → ICC conversion → D50/2° Lab → ΔE00 mean/median/P95/max; report pairwise third-party distance **not film-fidelity error**.
- Tone: L* and Y vs source, gray-ramp monotonicity and mean L* delta, shoulder/toe trend over EV.
- Hue: Lab LCh shifts for skin, green foliage, sky blue, reds, and near neutrals; preserve output color/gamut clipping flags.
- Pixel/image: after exact alignment and matched spatial extent use per-channel RMSE/MAE and image difference; this is descriptive and will overreact to spatial effects.
- Grain: linear luminance RMS, skew, autocorrelation and PSD on flat regions with equal physical/effective sampling; a scan also includes scanner noise and optics.
- Halo: spatial point-source profile and radial background-subtracted energy, control exposure and highlight clipping.
- Quality control: output clipping %, chromatic aberration from decode, NaN/Inf, repeated-run variance and ICC presence.

These are *measurements of other simulators' outputs*, not evidence of real Phoenix measurements. Pre-register baseline, fixtures, ROIs and measurement conditions before looking at comparative results.

## 4. Integration contract with PR #6

**Repo baseline (reviewed earlier):** PR #6 introduced `tools/phoenix-validation/` with `baseline.cjs`, `references.cjs`, `observations.cjs`, `metrics.cjs`, `candidates.cjs`, `report.cjs`, `cli.cjs`; versioned HTML/JSON reports, isolated `physical` reference manifest and engineering vs photographic gates. Do **not** revise or claim PR #6 tests were run as part of this report.

Proposed *future extension* without code change now:

```text
validation/phoenix-accuracy/third-party/
  benchmark-manifest.v1.json
  fixture-manifest.v1.json
  experiment-contract.v1.json
  authorized-outputs/      # local / gitignored, permission-dependent
  reports/                 # generated, versioned
```

`benchmark-manifest.v1.json` minimum fields:

```json
{
  "format": "SpektraLab.PhoenixThirdPartyBenchmark",
  "version": 1,
  "experiment_id": "phoenix-coloronly-D50-001",
  "film_generation": "phoenix_200",
  "engine": {"name": "DxO FilmPack", "version": null, "preset": "Harman Phoenix 200"},
  "fixture": {"input_sha256": null, "input_icc_sha256": null, "pixel_encoding": null},
  "render": {"output_sha256": null, "output_icc_sha256": null, "settings_record": null},
  "rights": {"basis": "not_verified", "evidence_uri": null},
  "review": {"operator": null, "reviewer": null, "pairing_status": "unreviewed"},
  "classification": "third_party_simulation_not_real_film"
}
```

Use PR #6 `observations.extract` after common color-tagged PNG conversion with audited ICC chain, or extend ingestion with validated TIFF/ICC handling. Use existing `pairedMetrics` for *descriptive between-simulator ΔE00*; **do not feed third-party simulation records into `physical` references, `evaluateSample` or the photographic gate**. New report section: `third_party_benchmarks`, separate from `engineering_gate` and `photographic_gate`, with `status=Not Run` while outputs are missing. Ensure test/validation splits of physical film are not used to tune to a commercial simulator. Keep sidecar manifests with rights and checksums; exclude proprietary output from public repo unless explicitly licensed.

## 5. Copyright and permission constraints

- Reference or describe publicly available material; no automated collection of rights-restricted photographs.
- Do not reproduce proprietary presets, LUTs, profiles, embedded physical data, commercial code or protected binary resources.
- Only make software outputs through authorized, licensed software where license terms permit; investigate relevant EULAs first (comparison as research vs distribution are different).
- A screenshot on a website is not a licensed training image.
- Document images from real film must receive permissions separately from simulator outputs.
- Maintain no protected data in Git, public CI artifacts or shared HTML without explicit permission.

## 6. Blockers and ordered follow-through

**Available today:** official product claims; descriptions of simulation method; published example results; PR #6 verification architecture. **Unavailable/unverified:** identical third-party output rendered from our fixture; per-tool all-effects-off controls; DxO FilmPack 8 *standalone* 16-bit export + output ICC behavior; commercial EULA approval for redistribution or training; Phoenix II DxO/Ashigrain support; true Phoenix physical/scanned reference pairs.

**Execution order:**
1. Freeze the second-round model work separately; keep this study as docs only.
2. Independently verify third-party software installation and rights, selecting one stock and one shared tagged input TIFF for an end-to-end pilot; start with DxO for I and Grain Archive for II.
3. Audit each tool's input interpretation, no-extra-effects settings and ICC output by render checks; reject uncontrolled pairs.
4. Run fixed color-only fixtures, then exposure series, then spatial/full-look fixtures. Record manifests and immutable outputs.
5. Integrate non-photographic benchmark reports into a future PR #6 extension with a distinct status field. Maintain physical fidelity `Not Verified` until independently paired real-film evidence is available.
6. For any real-accuracy acceptance, obtain lawful paired physical Phoenix C-41 scans and independent scene references; measure repeatability before setting thresholds.

## 7. Scope attestation

Research and design only. This report did **not** install/run commercial software, procure a license, download restricted film images, modify engine, PR #4, PR #6, UI, render output, materials, or any GitHub branch. No PR was created because the deliverable is a local independent research document rather than a user-authorized repository write operation. The exact requested destination is `docs/research/phoenix/THIRD_PARTY_BENCHMARK.md` if later copied into the repository.
