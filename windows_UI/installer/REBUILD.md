# Rebuilding this Windows preview

`SpektraLab-source.zip` contains the working-tree source used for the packaged
executable, including uncommitted changes, engine resources and build scripts.
`source-files.json` pins every copied file. It excludes build outputs, Git
metadata, private notes and photographs. The package manifest records the
executable hash, build record and source archive hash; a green Windows test run
is not a Mac parity certification.

Use a short ASCII checkout path, e.g. `C:\src\SpektraLab`. Install the pinned
Qt 6.11.3 / MinGW 13.1 SDK with `windows_UI/setup-qt.ps1`; see that script and
`windows_UI/dependencies/qt-packages.json` for official URLs and hashes. CMake,
Vulkan-Headers, glslang and LibRaw setup are documented in the root `README.md`
and `windows_UI/README.md`. No Python is needed to build/run the Windows app.

```powershell
./windows_UI/build.ps1 -SourceDirectory C:/src/SpektraLab `
  -BuildDirectory C:/src/SpektraLab/build/windows `
  -DependencyDirectory C:/src/deps -Jobs 6 -RunTests
```

The adjacent LibRaw source ZIP corresponds to the statically linked 0.22.2
release. Qt base/declarative/SVG revisions come from the SDK's SPDX metadata;
translations use the 6.11.3 release sources. The release archive for Qt base
differs from the prebuilt SDK: extract it, then overlay
`qtbase-sdk-source-overlay.zip` at the Qt base root. The overlay holds original
files from the SDK's fixed Qt commit, each checked against its source SPDX
hash (allowing only byte-preserving LF/CRLF representation, never UTF8
decode/re-encode of legacy-encoded data). The complete SDK inventory has 22517
files: 22125 equivalent release files and 392 fixed-revision replacements.
It is not a SpektraLab patch to Qt.
`qtbase-sdk-source-overlay.json` records every replacement and the source
inventory hash. `prepare-qt-sdk-overlay.ps1` reconstructs it. No SPDX validation
failure is waived as a successful source match.
Qt build configuration and bundled third-party metadata are preserved
in `licenses/Qt-SBOM`. Qt sources include their bundled component notices.
The compiler SDK/runtime sources and exception notices are identified in
`licenses/MinGW` and `licenses/THIRD_PARTY.md`.

The installed app dynamically loads adjacent Qt DLLs and plugins. A recipient
can rebuild compatible Qt libraries and replace those files while the app is
closed; preserve the MinGW ABI, module layout and matching plugins. There is no
Qt signature/relink lock, activation or DRM. Our EXE can also be rebuilt from
the supplied source. Microsoft DXC's license texts are under `licenses/DXC`.
NSIS's runtime license is under `licenses/NSIS-COPYING`.

To rebuild the installer use `windows_UI/package.ps1` and NSIS 3.12. The
compiler ZIP is pinned to SHA256
`56581f90db321581c5381193d796fffcf2d24b2f8fed2160a6c6a3baa67f2c4f`
from the MacPorts nsis Portfile. The original upstream is
https://nsis.sourceforge.io/ and the mirrored archive is
https://distfiles.macports.org/nsis/nsis-3.12.zip .

This is build/relink information, not a legal compliance certification or a
promise that another GPU/Windows configuration has been tested.
