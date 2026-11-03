# Windows Phoenix experimental preview

Independent integration branch from accepted research commit
`73784949cda9a99e859d3b3a82ac27784a5e7b3b`. It uses the existing Windows
Qt Quick/Vulkan application; the research PRs are dependencies, not merge targets.

## Scope and material identity

`windows_UI/experimental/manifest.json` lists all 23 original IDs, versions,
profile SHA256 values, parent materials and hypotheses: 2 Experimental v1,
9 Round 2 and 12 Round 3. Material JSON bytes are copied without edits.
`build-pack.cjs` verifies them against the accepted Git freeze (or the shipped
manifest when rebuilding the corresponding source archive without Git history).
Formal film/paper resources and the research directories remain unchanged.

The experimental build is selected by `SPEKTRALAB_PHOENIX_EXPERIMENTAL_PREVIEW`;
it is OFF in the engine by default. The Qt experimental entry point enables it.
The normal build retains its original gain calculation. The shared TIFF input
dispatch fix routes TIFF through the existing ICC-aware raster decoder instead
of LibRaw; no material model was changed for this input fix.

The separate Experimental panel groups both generations by v1/Round 2/Round 3.
It shows ID, version, SHA256 and hypothesis, and supports reset to the same
generation's v1. Comparison, zoom, preview and export reuse the existing editor.
Phoenix selection starts with automatic input metering, grain, glare and halation
disabled to match research conditions; existing controls remain available.

## Print modes

| Mode | Available materials | Meaning |
|---|---|---|
| Frozen Print | all 23 | Original frozen filter table and user controls; internal automatic middle-gray gain still recomputes. |
| Recalibrated Neutral Print | 2 v1 + 12 Round 3 | Frozen EV0/0.18 gray Endura/TH-KG3 parameters from Round 3; no optimization on photographs. Round 2 refuses this mode with an explicit message. |
| Fixed Internal Gain — Research Only | all 23 | A separate process/store fixes the three gains to the same-generation parent v1 reference; no diagnostic DLL is loaded. |

Fixed gain uses the accepted numeric gain table immediately before the existing
spectral print-exposure calculation, guarded by the experimental compile flag
and an immutable-at-start process mode. It performs no trace/readback and works
on arbitrary image dimensions. The old diagnostic reference DLL remains limited
to 32×32 validation; it is not distributed or linked into the application.
This mode identifies causal differences and is not a claim of greater realism.
Research trace spectra and per-stage XYZ/Lab are not GUI features.

Recipes include the experiment manifest in global resource identity and a
version-10 explicit mode. Normal/fixed processes use separate settings stores.
The normal product does not interpret version-10 experimental recipes.
Missing profiles, corrupt manifest or mismatched constants/filter tables fail
explicitly instead of skipping parameters or reusing a previous filter pack.

## Build and repeat the gates

Requires the pinned Windows development dependencies: Qt 6.11.3 MinGW,
MinGW 13.1.0, CMake 3.25.0, LibRaw 0.22.2, Vulkan headers and glslang;
Node is used only to prepare the resource pack. See the existing Windows
dependency manifests for SDK/source hashes. From a PowerShell 7 terminal:

```powershell
./windows_UI/experimental/build-preview.ps1 -DependencyDirectory C:/deps -BuildDirectory ./build/phoenix-preview
./windows_UI/experimental/verify-preview.ps1 -BuildDirectory ./build/phoenix-preview `
  -Resources ./build/phoenix-preview/app/resources `
  -NormalReference ./build/review-clean/engine/spk_render_fixture.exe `
  -FixedReference ./build/round3-fixed-gain/spk_render_fixture.exe `
  -OutputDirectory ./build/new-phoenix-evidence -RawInput C:/photos/example.ARW
./windows_UI/experimental/package-preview.ps1 -BuildDirectory ./build/phoenix-preview `
  -QtBin C:/deps/qt-6.11.3/mingw_64/bin `
  -CompilerBin C:/deps/qt-6.11.3/Tools/mingw1310_64/bin `
  -LibrarySourceDirectory C:/distribution-sources -Destination ./output/PhoenixPreview
```

The reference binaries must come from the accepted research builds; their hashes
are recorded with this run. Do not replace them with the preview renderer.
The verifier makes a new resource snapshot and retains every failure log.
Set Qt/compiler runtime directories on PATH when invoking development tests.
The distributed directory uses only its adjacent runtime/resources, with no SDK,
Python or Node requirement at runtime. A Vulkan-capable driver is required.

`photographic_fidelity_validated=false`; `photographic_gate=Not Verified`.
All unsupported material hypotheses remain Inconclusive. No real-film fitting
or fourth-round work is part of this application integration.
