# Automatic colour-negative inversion — Windows, 2026-11-02

Implemented opt-in internal negative development, spectral base removal and
reversal using the existing Digital Intermediate constants, with a Vulkan
port of `spk_di_encode`. Engine pixels retain the Cineon contract. Qt decodes
that log before Post-Dev, applying a neutral-preserving rational highlight
shoulder in linear ProPhoto. Direct negative TIFF export remains separate.
Slides and monochrome films retain their existing paths. Settings persist
in sidecar v10; absent settings in older records default off.

Validation:

- Old Oct 30 build rejects the 5207 grey-ramp DI request; the new build accepts
  the same request and produces bit-exact repeated renders.
- Vulkan LUT/DI: 97 analytic/float64 cases, maximum absolute error
  6.96505159e-7, including bounds guards and the new Cineon encoder.
- Settings store: 411 checks, including automatic inversion save/load.
- Film workflow: 689317 checks including enabling/disabling inversion,
  cached-negative reuse, positive-preview/export agreement, and negative TIFF
  depth, ICC and all pixels against direct scan.
- Full 37-test gate: 36 passed; material workflow initially failed its stale
  v9 assertion. Updated to v10 and reran successfully (37 workflow checks).
- Final rebuilt QML application: standalone QML workflow passed. An earlier
  run timed out while other work was running; it was not counted as passing.
- `git diff --check` passed for the feature changes.

Development executable: `build/auto-negative/app/SpektraLabQt.exe`.
SHA256: `a54ca8f1037ec2fe1749a6e028d75217406c9be3ac28bff9e895032397e71722`.

The build script refused to record a distributable source manifest because
the shared source inventory changed during the build. This executable is a
development build, not a packaged release. Separate ongoing Phoenix material
work was left untouched. No macOS/Metal parity or external Python oracle run
is claimed by these Windows checks.
