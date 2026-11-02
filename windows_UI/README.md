# Windows Qt Quick frontend

## Windows preview installer

`package.ps1` packages an already deployed/tested build as a per-user NSIS
installer. This is a **preview**, not a completed Mac port. It includes Qt and
compiler runtimes, engine resources, license texts, the exact working-tree
Windows source snapshot and corresponding Qt/LibRaw sources. It never includes
test photographs or SDK binaries as development tools.

Prepare `dependencies/distribution-sources.json` with
`setup-distribution-sources.ps1`. The Qt base SDK predates its release tarball;
see `installer/REBUILD.md` for its hash-verified fixed-revision source overlay.
Use NSIS 3.12 (compiler ZIP SHA256
`56581f90db321581c5381193d796fffcf2d24b2f8fed2160a6c6a3baa67f2c4f`).

```powershell
./windows_UI/package.ps1 -BuildDirectory ./build/windows `
  -DependencyDirectory C:/src/deps -LibrarySourceDirectory ./build/distribution-sources `
  -NsisDirectory ./build/tools/nsis-3.12 -OutputDirectory ./build/preview-installer
./windows_UI/tests/package.ps1 -PackageDirectory ./build/preview-installer
```

The package output must be new and inside the workspace; the build's executable
must match `qt-build.json` and have run the Windows CTest gate. The final EXE is
also tied to `build-source-files.json`: changed source or an absent/unverified
Qt SDK source overlay is a hard packaging failure. A previously generated
installer is not automatically approved for redistribution by a newer test.
The final EXE is
under `OutputDirectory`, alongside `SHA256SUMS.txt`. Installation requires an
empty directory, uses no administrator elevation or RAW association, and has
Start menu and uninstall entries. Uninstall removes only enumerated package
files and empty directories; user photos, edits and extra files are preserved.
The test uses `/TESTONLY` to avoid altering real uninstall/shortcut registration
and checks deployed hashes, isolated startup, overwrite refusal and uninstall.
These unsigned previews still require Vulkan drivers and an independent clean
recipient-machine check before claiming release readiness.

## Scene fitting (first Windows panel)

The Pre-Dev **场景拟合** panel calls the engine's real `spk_scene_latitude`
measurement/fit. It shows a fixed −8…+8 EV plot (square-root histogram heights),
measured neutral-film/paper range, below/within/above shares, the neutral chain's
CIE L* separation band and automatic pull-back suggestions. The original and
proposed histograms are labelled separately; a refused fit has no proposed curve.
Measurement does not change the picture. Manual sliders require **拟合并应用**;
**自动建议并应用** requests the engine's suggested fit. A refusal or a suggestion
outside the panel's 0…8-stop range retains the previous curve. These resolved
shoot-layer curves precede the film; no measured material profile is modified.

Placement travels with per-photo settings, history, reset, synchronization and
native single/batch export. Edit format v4 adds it; v1–v3 load with neutral
placement, preserving their existing post-develop interpretation. The meter
holds the exact decoded snapshot and never rereads a replaced source path.
Changed input/film/paper settings make its readout stale until remeasured.

This is not full Mac panel parity: no automatic refresh, local feasible-window
slider solve, per-recipient refit on synchronization, or EDR implementation yet.
The first panel explicitly measures and applies on demand; it does not pretend
that a fixed 0…8 slider range is the Mac's locally constrained live slider.
`spk_qt_scene_placement` and the real QML workflow test exercise actual fitting,
refusal, rendering, immutable source ownership, history, migration and export.

The Windows migration execution order, remaining scope and acceptance gates
are tracked in [the port plan](WINDOWS_PORT_PLAN.md). Consult that plan before
starting the next migration stage; implementation is not equivalent to Mac parity.

This is a native Qt Quick/C++ application using the existing synchronous
`spk_desktop_host`. There is no Python, Node, browser or subprocess renderer at
runtime. The upstream SwiftUI theme supplied the grey rails, centre canvas,
warm accent and bottom strip; unsupported controls are omitted deliberately.

### JPEG and PNG input

Use **导入照片** or drag files into the editor. JPEG (`.jpg`/`.jpeg`) and
8/16-bit PNG share the existing host reader: encoded samples are decoded
through the embedded RGB matrix/TRC ICC profile into scene-linear ProPhoto
before entering the unchanged film pipeline. Untagged JPEG/PNG, including
16-bit PNG, assume sRGB. Unsupported or malformed embedded ICC profiles are
rejected with an error, preserving the previous successful frame.

PNG transparency is composited onto white in linear light before film
exposure; this editor does not preserve transparent output. JPEG EXIF
orientation is applied once. The RAW decode selector is disabled for raster
photos. Edits, comparison, geometry, half-frame pairs and TIFF/PNG/JPEG export
use the same controller and persistence path. Importing a JPEG does not undo
the camera/developer's existing tone mapping, so it is not equivalent to RAW.
Sources are never rewritten. The historical C++/Qt `open_raw`/`openRaw` names
remain compatible; no C API or material-profile fields changed.

Implemented: multi-file RAW/JPEG/PNG import and drag/drop; independent per-photo
film/paper, decode, exposure, grain, halation, glare, film format and filter
settings; a navigator and selected-photo strip; free/ratio cropping; and
TIFF16, PNG8/16, JPEG8 export with JPEG quality and optional downsize. Batch
export processes selected photos serially, supports cancellation between files,
and skips existing files. Single export refuses overwrites. Output stays sRGB
with the bundled ICC profile. Select-all and synchronization copy supported
settings to selected photos, preserving their own crops by default.

Ctrl+O imports, Ctrl+E opens export options, Ctrl+0 fits and Ctrl+1 selects
physical 100%. Double-click also switches fit/100%. Changes update the preview
automatically after a 180 ms settling delay. The filmstrip restores each
photo's wanted edits, including edits pending when the user switches away.
Edits automatically save under the application's local data directory (`Edits`)
and restore when the same photo is reopened. RAW files and their directories
are not modified. Save failures, incompatible records, changed sources and
conflicting writes from another app instance remain visible and preserve the
existing record. After a concurrent-write conflict, restart the application and
reopen the photo to load the other instance's saved settings. Undo/redo is per
photo, bounded to 64 in-memory steps, and a
slider drag is one step. Undo history itself is not persisted. Only the current
photo is fully decoded/developed; unvisited
photos have an explicit placeholder until opened. Small thumbnails are retained,
not one full RAW/GPU session per imported photo.

Crop is post-render geometry and does not re-run the emulsion. Preview,
navigator and exports share the same rounded source-pixel edges. The first
Windows crop supports free drag, 1:1, 3:2, 4:3 and 16:9, clockwise/counterclockwise
quarter turns, horizontal/vertical flips and -45 to +45 degree straightening.
Straightening applies on release, uses original 16-bit samples and zooms to
fill the crop; quarter turns/flips are lossless. The crop editor uses the
unrotated source explicitly, while presets describe the final output ratio.
Masked full-photo strip thumbnails remain pending. The strip currently
shows the cropped output. See [DESIGN_ALIGNMENT.md](DESIGN_ALIGNMENT.md) for the
author's drawing transcription and upstream comparison.

Slide film is scanned directly, matching the upstream macOS film-stage rule.
The host resolves this from each profile's `info.type`, including Provia 100F,
Velvia 100, Ektachrome 100 and Kodachrome 64. Paper and paper-brightness controls
are disabled for slides; their previous settings return with negative film.
The colour model, profile data and shader maths are unchanged.

All engine work runs on one serial worker. Edits during a render are coalesced
into the newest settings; superseded results never replace the displayed frame
or newer controls. File dialogs pause pending updates until they close; opening
and exporting lock conflicting commands. A failed
current render retains the displayed frame and restores its controls; a retry
is available. Export captures that displayed frame's immutable decoded input
and film settings, including when a newer preview is pending, and renders
native-size pixels before geometry/optional downsize. It does not upscale the
preview or re-read a replaced source file. Pending edits resume after export
succeeds or fails. The preview GPU session is released for a native render;
the first subsequent edit rebuilds its negative cache.
Closing during work waits for the current operation. A failed image-publication
allocation is reconciled against the displayed source/decode mode on the next
render.

Ctrl+Z undoes the current photo; Ctrl+Shift+Z / Ctrl+Y redo. Text editors retain
their own undo shortcuts. Synchronization is undoable on each recipient photo.
Batch export and file dialogs lock history commands along with other edits.
`--edit-store <absolute-directory>` overrides the persistent directory; all
automated tests inject an isolated directory, including `--self-test` and
`--snapshot`, so no test reads or writes the user's real edit store. Persistent
photo identity includes canonical path, size, modification time and first/last
64 KiB hash. Renamed/moved source files are not automatically matched yet.

## Build

Use the official Qt 6.11.3 Windows MinGW SDK with its MinGW-w64 13.1 compiler.
Do not link these Qt binaries with the earlier GCC 15 Windows build. Build into
an independent directory; the script changes only its own process PATH.
Use `-Fresh` to discard an existing CMake configuration before rebuilding.

```powershell
# From the repository root. The bootstrap uses only PowerShell and Windows
# bsdtar, downloads into ../deps and checks pinned SHA256 for every file.
./windows_UI/setup-qt.ps1 -DependencyDirectory ../deps
./windows_UI/build.ps1 -SourceDirectory . -BuildDirectory ./build/qt -RunTests
```

Local dependencies are in the workspace's `deps/qt-6.11.3`, alongside Vulkan
headers, glslang and LibRaw used by the engine. `-DependencyDirectory` overrides
that location. The output `app/` is self-contained for this Windows machine;
keep the complete directory with its DLLs, QML plugins, resources and notices.
The Vulkan driver is supplied by Windows/the installed GPU driver.

`dependencies/qt-packages.json` lists the exact official archives and hashes;
`dependencies/qt-licenses.json` lists each official source license and hash.
`setup-qt.ps1 -VerifyOnly` verifies the existing archive/license cache and tool
versions without network or extraction. The initial download is about 278 MB.
This script does not install Vulkan headers, glslang, CMake or LibRaw; set those
up using the repository's Windows engine instructions first. It neither
downloads an unpinned "latest" SDK nor depends on a private local manifest.

For a parent build, enable the native RAW and desktop host targets and call
`add_subdirectory(windows_UI)` after `add_subdirectory(engine)`. The Qt frontend
links the existing `spk_desktop_host`; the macOS build is untouched. Resources
are synchronized on every build, including builds where the executable itself
does not need relinking. Deployment is an explicit build-script step.

## Validation

The first validated build used Qt 6.11.3 / MinGW 13.1 on an RTX 5070 Ti:
16 engine CTests passed, followed by the real 4688 x 7028 ARW scene test below.
The same decoded input produced byte-identical deterministic RGBA16 full and
print-edit outputs under this compiler and the existing GCC 15.2 baseline.
The Vulkan startup gate uses an exact product-residual reference because the
legacy MSVCRT `fmaf` returns incorrect residuals for some of the probe inputs;
the GPU comparison remains exact and also has fixed-bit regression vectors.

```powershell
$env:QT_QPA_PLATFORM='offscreen'
$env:QT_QUICK_BACKEND='software'
./build/qt/app/SpektraLabQt.exe --open C:/photos/supported.ARW `
  --reject C:/photos/unsupported-HE.NEF --self-test C:/SpektraLab-validation/new-qt-test
```

The output directory must not exist. The test drives actual QML/controller
operations and the native Vulkan engine, saves fit/100%/final PNG captures,
exports the displayed frame, checks negative-cache use, film/paper changes,
failed RAW retention, overwrite refusal with pending settings, single flight,
panning and physical-pixel scale, old-frame immutability, and injected display
publication failure recovery. It also checks automatic selection/brightness,
rapid edits before and during rendering, returning to the displayed settings,
all four slide films, negative/slide transitions, automatic decode changes and
reset. It exits nonzero on failure and writes JSON and scene captures.

`spk_qt_frame_output` is an additional CTest for shared crop geometry, actual
PNG/JPEG/TIFF pixels/depth/ICC, JPEG quality, resize, extension agreement,
atomic no-overwrite publication (including concurrent writers), and cleanup.
The explicit full-RAW library gate uses isolated copies of both photo inputs:

```powershell
# Use the same Qt/MinGW runtime PATH as build.ps1 and the SDK's plugin directory.
./build/qt/spk_qt_library_workflow_test.exe ./build/qt/app/resources `
  C:/photos/supported.ARW C:/photos/unsupported-HE.NEF `
  C:/SpektraLab-validation/new-library-test
```

It checks pending edits while switching photos, independent crops, synchronization,
preview/export pixel agreement, PNG16 and JPEG size, batch source/filename
agreement, selection locks, no-overwrite, cancellation and failed RAW retention.
The output directory must be new; missing fixtures are a hard failure.

`spk_qt_edit_store` adds strict document/version validation, atomic save failure,
source replacement (including before first save), concurrent-instance conflicts
and preservation of damaged/future records to CTest. `spk_qt_editing_workflow_test`
uses the same four arguments as the library gate and verifies history during
in-flight renders, geometry/PNG agreement, mirrored rotation direction,
destructor flushing of pending changes, recreation/reopening, per-photo history,
batch locks and visible persistence warnings. It owns copies of all inputs.

The captures are **offscreen scene captures**, not a live monitor screenshot.
Software scene display does not make the engine a CPU renderer. This gate does
not automate native file dialogs or validate a physical monitor. The Qt host
now renders encoded ProPhoto, grades/resamples there, then uses the shared
`spk_output_transform` arithmetic (including CAM16 compression) to reach sRGB.
FrameCanvas reads the active Windows monitor ICC via GetICMProfileW and
converts the final sRGB image with Qt before uploading its SDR texture; it
reports an unavailable/invalid profile instead of claiming calibration.
Wide-gamut output selection, EDR/HDR, physical display/ACM validation and
perspective geometry remain future work. Original/split comparison and the
enabled global Layer 2 arithmetic are implemented, but are not yet
whole-image Mac or interactive-performance parity. TIFF and PNG16 retain 16-bit
pixels; PNG8/JPEG8 use 8-bit samples, and resizing is explicitly requested.

Nikon HE/HE* RAW remains explicitly unsupported by this LibRaw build. A failed
NEF open must leave the previous ARW and its session usable.

### Incremental stock and post-development gates

`spk_qt_stock_policy` runs without external photographs: it writes a small
owned Bayer DNG in a temporary fixture directory, uses a separate edit store,
and exercises the actual LibRaw -> desktop host -> Vulkan -> Qt controller
path. It verifies CineStill selection with halation enabled, post-adjustment
undo/redo, bypass/reset, cropped/turned PNG and batch pixel agreement, saved
state and reopening. Independent C ABI charts cover four silver films and
CineStill -> Portra -> CineStill transitions. This is not a substitute for
real-camera RAW and large-frame memory validation.

The CineStill path now includes `spk_highlight_headroom`, directly ported from
the Metal node. `spk_vulkan_pointwise` checks that kernel against an analytic
reference, including zero/negative input, transition thresholds, common RGB
gain, workgroup boundaries and output guards. The material data and default
halation are not disabled to make it render.

The right-rail **显影后** tab supplies temperature/tint, exposure, contrast,
brightness, saturation, highlights/shadows, black/white points, five curves
(RGB/luma/R/G/B), four colour-balance zones and vignette. The curve editor adds,
moves and removes points; colour zones currently use hue/saturation/luminance
sliders rather than Mac's circular wheel widget. Arithmetic follows Mac's
Layer 2 order, cosine colour offsets, 256-sample Fritsch–Carlson curve tables,
texture lookup centres and radial vignette. The working pivot is ROMM 0.3857.
This is frontend CPU image processing, not a fallback for the Vulkan film
engine; GPU grading and real-window interaction timings remain outstanding.
Neutral/bypassed adjustments
preserve the unmodified simulation. Non-neutral adjustments are applied before
geometry/resampling; destination conversion follows geometry and precedes
output-depth conversion for all frame output surfaces and exports.

Editor documents now use version 6. Version 1 reads with neutral post controls;
version 2 retains a legacy-sRGB adjustment marker, rather than silently
reinterpreting existing controls in ProPhoto. Migration occurs on save;
invalid/future records are preserved and block replacement. Curves, zones,
vignette and the working-space marker participate in history, synchronization,
single/batch export and persistence. Reset starts a new ProPhoto grade.
`spk_qt_edit_store` covers both migrations and strict point/zone validation.

The toolbar's **原图** shows the retained scene-linear RAW decode (bilinear
resampled before encoding), not the film result. **分屏** has a draggable
divider; holding Space on the focused canvas temporarily shows the original,
then restores the split. Both sides use the same crop/turn/straighten, zoom/pan
and monitor conversion. Comparison is a viewing mode: exports retain the
processed image. Persistent “Don't Apply Film Simulation” remains separate,
not implemented by this temporary comparison switch.

Scene latitude's Vulkan kernel and on-demand Qt fitting panel use the measured
analysis API. Mac's local feasible-window live sliders remain outstanding.
Changing paper does not enable a border.

### Film borders, date backs and half-frame pairs (2026-10-30)

The right rail's film/date section exposes ten film formats, strip/filed
carriers, picture gates, perforation backing, stock lettering, frame number,
registration seeds, fog and light leaks. It uses the shared overscan pipeline
and four Vulkan kernels, not a PNG frame pasted onto a finished print.
Windows GDI+ rasterises lettering masks; the measured film/paper profiles are
unchanged. Font metrics need Mac visual comparison and are not pixel-identical.
Crop/straighten/turn/flip cut scene-linear input before film when border/date
is active. Comparison uses the engine's actual picture quad(s); export does
not crop the resulting film canvas a second time.

Date backs expose LCD/dot-matrix/data lettering, position, size and exposure.
Text accepts digits and date separators. The default is **today, not EXIF**;
enter the capture date manually. Panoramic formats do not offer date backs.

Select exactly two RAW/JPEG/PNG photos in the strip and choose **半格双拼**. A new
`.spektrapair` project is created under the app's Edits/Pairs directory. It
references two absolute source paths, full SHA-256 fingerprints, decode modes
and their initial source crops; original files are neither changed nor moved.
Open this project through **导入照片** to restore it. Missing/changed sources
are rejected, preserving existing edits and the previous canvas. These Windows
project files are not the Mac pair-file format and are not portable packages:
moving the originals currently requires recreating the pair.

Each hole has independent balanced metering, exposure, scale, pan and quarter
turns. The zero-radiance gap passes through one shared film development.
Horizontal/vertical arrangements work; spacing is 0.5–2 mm without a border,
and physically fixed to 1 mm with a border. Film, paper, global grading and
date text are shared in this first version. Per-hole printing/grading, Mac
edit scopes, source replacement/relinking, automatic EXIF dates and cached
per-hole interactive recomposition remain outstanding. Ordinary crop controls
and decode changes are disabled for a pair; edit its hole placement instead.

All new settings participate in undo/redo, restore, single/batch output and
strict v6 edit documents; v1–v5 migrate with neutral newly introduced fields.
Synchronizing looks preserves each recipient's source type and pair recipe.
`spk_windows_film_edge`, `spk_qt_film_frame` and `spk_qt_half_pair` exercise real
Vulkan rendering with owned inputs, not fixture-dependent skips. Native film
and pair export checks include a 3600×2400 sensor fixture with stochastic
effects off; this is not a 45 MP/default-effects border memory acceptance.
For an empty-editor scene capture, use `--ui-smoke <new.png>`; add
`--ui-smoke-post` to show the post-development rail. These are offscreen
captures, not proof of physical monitor colour management.

### Native-size export and memory gate (2026-10-30)

The interactive canvas remains a 3400-pixel preview; its 100% is one preview
pixel per physical display pixel, not native RAW detail. Metadata now reports
decoded original dimensions separately. Single and batch GUI exports render
the original decoded dimensions, apply post-development and geometry, then
optionally downsize. Their native-size grain/detail need not equal the smaller
interactive preview pixel for pixel. The low-level `exportFrame`/host
`export_tiff` writers still write exactly the explicit frame supplied to them.

Without a film border, the desktop host uses actual 128-row strip plans rather than the old
`striped=true`/zero-rows whole-plane plan. The Vulkan pool bounds oversized
reuse so small parameter blocks cannot pin full image planes; allocation
pressure reclaims only idle zero-reference pooled buffers and retries once.
Film-canvas renders explicitly disable stripes: upstream overscan requires a
whole canvas and therefore uses more VRAM. Native-size export can fail on a
small GPU; it must not silently substitute an enlarged preview.
Source and cached-negative holdings prefer host-visible compute memory outside
the device-local heap. Shaders still execute on the Vulkan GPU, at float32;
these holdings are not Windows file-backed mappings or a CPU renderer.

`spk_qt_stock_policy` checks a >3400-pixel owned DNG, native single/batch export,
crop/turn dimensions, post-export editing, and exact decoded-snapshot ownership
after its source is moved/replaced and another photo is open. The transfer gate
checks size-class reuse and bit-exact GPU access to host-visible holdings.
An explicit large gate (not a silently skipped CTest) runs with:

```powershell
./build/qt/spk_qt_stock_policy_test.exe ./build/qt/app/resources `
  C:/photos/supported.ARW C:/SpektraLab-validation/new-native-export-test
# Or owned synthetic sensor fixtures, explicitly not real-camera validation:
./build/qt/spk_qt_stock_policy_test.exe ./build/qt/app/resources `
  --synthetic-45mp C:/SpektraLab-validation/new-synthetic-test
```

It keeps default grain/halation/glare enabled, writes native PNG16 plus a
cropped/turned batch result and a JSON report in a NEW directory. It owns a
copy of the input and its edit store. Recorded results and remaining limits
are in [the validation record](VALIDATION_2026-10-30.md); these are not Mac
whole-image parity or live-monitor acceptance.

## 16-bit negative scan export

For an entirely internal workflow, enable **自动负片翻转 · 去色罩** under
film format/effects. It generates the negative, removes the orange base using
the film data and reverses it without writing or reimporting a file. The
canvas and normal exports show the same positive, with a soft output
highlight shoulder. Post-Dev adjustments still apply. Disable it to restore
the existing optical print. Slides and monochrome films retain their existing
paths; the option is saved
per photograph (sidecar version 10, older edits default off).

In the export dialog select **负片扫描 TIFF · 16 位** for a negative film.
The TIFF contains a simulated negative scan with the colour mask retained,
encoded as ProPhoto RGB with an embedded ICC profile. Invert and remove the
mask in an external negative conversion workflow. Printing and Post-Dev
grading are bypassed; film exposure/effects and crop geometry remain.
The size selector still applies; choose original cropped size for grading.
This option is available for both single and batch export. Positive film
stocks are rejected rather than exported under a misleading negative label.
It does not change existing print rendering or the measured film profiles.
