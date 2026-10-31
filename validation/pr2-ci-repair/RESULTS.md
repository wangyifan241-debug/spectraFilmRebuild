# PR #2 CI contract repair — 2026-10-31

## Source identity

PR #2 import `b1151beaf98f9f07d8529586013db8fd23261b05` has one parent,
`5e9568cad4bd64f7e720d04563639a071c701950` (main). Unlike PR #1, it does not
connect unrelated histories through a two-parent commit. Its tree is
`3c6b88a4a303b4cd4cb405ee9f116013546f037c`, exactly equal to the locally tested
Windows baseline `bee84bfb743899676c446444dddd6bb476bf92ed` tree. Tree equality
establishes source identity, not a claim that every historical test was rerun.

Original Actions run: 41195894652, Linux job 124521854989, Wine job 124521854800.
Both logs checked out GitHub's synthetic merge `24f7ace4dc0cc2c953fba390a82cc0e43ca357aa`.
The separate macOS Build run 41195894671 passed; frontend checks passed.

## Failure classification

* Linux CTest: 15/16 passed. `spk_windows_film_edge` unconditionally expected
  Windows coverage/date rasterisation on Linux, although capabilities explicitly
  advertise overscan/date as unsupported there. The test now executes Linux
  refusal, no-partial-mutation and subsequent successful rendering assertions;
  Windows retains all existing pixel/date/format/pair tests. No skip or test
  registration was removed.
* Wine host smoke: exactly 21 failures = 7 per synthetic PNG/Unicode PNG/TIFF.
  Three stale refusals (scene latitude, overscan, date) actually enable supported
  Windows features. Four later checks (LUT, DI package, racing cancel, render
  after cancel) inherit that state. Unicode PNG has an incompatible 135 gate
  aspect; other fixtures hit Wine's unavailable Windows font rasteriser.
* Native Windows reproduces 13 failures with the original test: all nine stale
  refusals plus four Unicode gate-aspect cascades. The native font path works.
  Thus no independent cancellation/session runtime defect was demonstrated.
* Font availability under Wine remains an environment coverage limitation:
  fixing fixture isolation does not certify Wine typography. Native C ABI
  film-edge/date tests exercise the actual Windows rasteriser and stay required.

## Changes and checks

`engine/tests/host_smoke.py` pins the expected unsupported set independently by
executable platform, checks capability agreement, tests supported acceptance and
unsupported rejection with unchanged params, and restores each toggle before
later methods. It still fails on bad announcements or requests; it does not
accept arbitrary capabilities as truth or swallow render errors.

`engine/tests/windows_film_edge.cpp` adds Linux refusal/session recovery checks.
No engine, shader, C API, measured profile or Qt product implementation changed.

Fresh native Release build: MinGW GCC 13.1.0, CMake 3.25.0, LibRaw 0.22.2,
NVIDIA GeForce RTX 3050 Laptop GPU, driver 595.95. Build directory:
`build/pr2-native-ascii` through the existing ASCII workspace junction.
Initial MinGW build through the Chinese absolute workspace path failed in
`VerifyGlobs.cmake`; the equivalent ASCII junction build succeeded. This is a
local build-tool/path limitation, not evidence of an image-loading Unicode bug.

`ctest -N`: 17 tests. Actual CTest: 17/17 passed, zero skipped, 11.46 seconds.
Includes schema/setup, writer, RAW synthetic/errors, Vulkan math/pointwise/grain/
geometry/transfer/OOM/LUT, film edge/date/pair, feature boundaries and C ABI.
Original host smoke: 13 failures. Updated host smoke: zero failures.
Synthetic inputs are created by the test; no unavailable RAW fixture was skipped
to obtain these results. This repair gate does not test real-camera RAW or Qt UI.

Reproduction (configure/build using the pinned local dependencies first):

```powershell
ctest --test-dir build/pr2-native-ascii -N
ctest --test-dir build/pr2-native-ascii --output-on-failure
python engine/tests/host_smoke.py --host build/pr2-native-ascii/engine/host/spektralab-host.exe --resources build/pr2-native-ascii/engine/resources
```

Linux/Wine reproduce on Actions through unchanged `build-linux.sh --test` and
the cross-build + Wine smoke steps. Local WSL has no configured Linux distro;
Linux and Wine execution must be reported from the new Actions run, not from
native Windows results.

## Import/distribution inspection

Tracked extension inventory found no EXE/DLL/ZIP/DMG, camera RAW, private-key or
credential-named files. A content scan for common GitHub/AWS token formats and
private-key headers found no matches outside vendor/build/docs directories;
this is a bounded scan, not proof of absence of every possible secret.
Large tracked binary data are baked spectral constants, DI cube tables, icons
and product screenshots; these are intentional inputs/evidence, not compiled
application executables. No profile directory differs from the import baseline.

`git ls-files -ci --exclude-standard` exposed `PRD/PRD-v1.4.0.md` despite the
local-only `/PRD/` rule. Removed it from tracking while keeping the local file.
It remains in the existing import commit's history; no force-push or historical
erasure was performed. Six ignored `.log` files are intentionally tracked
red/green validation evidence under `validation/installer-20261031`, retained
with their context rather than silently deleting test results.

License/attribution inputs inspected: `license/`, vendor stb/volk/metal-cpp
licenses, profile notices in `resources_open`/`resources_product`, Qt dependency
license manifests, `windows_UI/THIRD_PARTY.md`, and `engine/stage-host.sh`.
Host staging copies GPL, CC BY-SA changes/attribution, LibRaw LGPL/copyright,
stb/volk/Vulkan-Headers notices and all staged engine resources. Qt packaging
has a separate source/relink inventory. This inspection is not a legal
certification or a clean-recipient-machine installation test; final packaging
jobs and corresponding source availability remain separate checks.

The three untracked `docs/windows-*` audit documents are deliberately excluded
from this repair commit. Final repair tree differs from the frozen import only
by the listed test corrections, this report and the local-only PRD removal.
CI results must be associated with the pushed head and its merge ref; native
results apply to these test edits over the identical imported engine tree.
