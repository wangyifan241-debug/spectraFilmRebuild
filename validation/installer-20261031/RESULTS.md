# Windows Preview 2026.10.31-preview.2

## Actual validation

- Final Windows Release: **32/32 CTests passed, 223.16 s**, no fixture skips.
- New icon test: all nine ICO sizes decode with transparency; every PE icon
  image matches the original artwork. The previous executable failed this gate
  before replacement (`icon-old-red.log`). SVG regeneration is byte-reproducible.
- Final native Windows empty-editor startup: exit 0, stderr 0 bytes. Qt's actual
  window icon must load in both smoke and QML integration gates. Window captures
  exclude the native non-client caption; they are not a visual taskbar/caption
  inspection. Executable resources are checked independently.
- Installer shell icon extracted and visually checked (`installer-icon.png`).
- Isolated installation in a Chinese/spaces path: **1721 installed file hashes**
  match; overwrite is refused; application starts without developer PATH or Qt
  import paths. Exact-file uninstall preserves recipient-added files and does
  not change real uninstall registrations. See `install-test.json`.
- A second isolated install renders an owned copy of a real Sony ARW through
  native Windows/Vulkan, exit 0 and empty stderr, with developer paths removed.
  Uninstallation preserves recipient content. **475 archived source files**
  match their SHA256 inventory; the fixed regression fixture is included and no
  RAW photographs are present in the source ZIP. See `source-render-test.json`.
- Non-UTF8 EOL source validation: old UTF8 implementation red, byte-preserving
  correction green; a substantive byte mutation remains refused. Original SDK
  SPDX inventory: **22517 files = 22125 equivalent files + 392 replacements**.
  No missing-source exemption or hash gate removal was used.

These are developer-machine correctness checks, not a clean-recipient-PC test,
full macOS parity, a sustained interaction/FPS benchmark or a signed release.
The original material data and film physics were not changed in this batch.
Pro Image 100 remains unavailable pending measured-data consistency/calibration.

## Distribution artifact

File: `SpektraLab-2026.10.31-preview.2-windows-x64-setup.exe`

Bytes: **151041898**

SHA256: `e6953fa38f4d81d1a7652ce17668caa3c7ea0104ddcde428a2e6a653f1cb8d2f`

Application SHA256: `83516afde561a8a6a681162aec3560181ea877a434d30cb6dcf3a086f8ec2fb7`

Frozen source inventory SHA256: `03050a9095ddec6beeaa56c1fdab04e4a5f623950bac3bf2c39fe8396344613f`

The installer is under `build/installer-20261031-preview-2/` locally and is not
uploaded as a GitHub Release. It bundles runtimes, profiles, licenses and source
archives. Do not distribute an older installer marked NOT-FOR-DISTRIBUTION.
Requires Windows x64 and a working Vulkan graphics driver. It is unsigned;
verify the checksum/source and do not disable system security protections.

## Corresponding-source provenance

Qt base is reconstructed at fixed Qt revision
`5a1194b2d368e2d72c230205c464b04a2f23eccd`; every source entry was checked against
the SDK source SPDX SHA256
`b2c2ebd3c7ab1ab35d24eba5d6b7c6028c53660c34374234c032525a23464b7f`.
Release archives are pinned by `windows_UI/dependencies/distribution-sources.json`.
Fixed-revision replacements came from Qt's official code.qt.io host and the Qt
GitHub repository API; no alternate Qt version was substituted. Initial raw-host
DNS failures, uncompressed transfer timeouts and incorrect legacy-text hashes
were failures, not successful validation. The completed overlay SHA256 is
`f72e2092c65cd1403fda2cf637fd4fde4b3d56f52416c8872525a6fb686a06ff`.
The NSIS 3.12 compiler archive matches the documented pinned SHA256.

Profile/third-party licensing and redistribution notices remain in the package.
This report and later validation records are outside the existing compiled
source-inventory allowlist; no rule was weakened to avoid rebuilding.
