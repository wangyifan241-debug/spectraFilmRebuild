# Windows Qt frontend dependencies

SpektraLab code remains GPL-3.0-or-later. The film/paper profiles retain the
upstream CC BY-SA 4.0 notices. This development build dynamically links the
open-source Qt 6.11.3 Windows MinGW SDK; it does not use a Qt commercial license.

The adjacent `Qt/` directory contains official Qt license texts. The exact SDK
source revisions are recorded in `Qt-SBOM/*.spdx.json`. The Qt base, declarative,
SVG and tools license texts were retrieved at those source revisions. The
shader-tools and translations SBOMs do not identify a commit, so their license
texts were retrieved from separately pinned commits on the Qt 6.11 branch;
`Qt/manifest.json` records every source URL and SHA256. SDK archive URLs and
verified hashes are in `Qt-packages.json`.

- Qt source: <https://code.qt.io/cgit/qt/> (also mirrored at <https://github.com/qt>)
- Qt licensing: <https://doc.qt.io/qt-6/licensing.html>
- Qt module and bundled third-party notices: `Qt-SBOM/` and `Qt/` in this folder
- LibRaw 0.22.2 source: <https://github.com/LibRaw/LibRaw/tree/0.22.2>
- LibRaw license choices and copyright: `LibRaw-LICENSE.LGPL`,
  `LibRaw-LICENSE.CDDL`, `LibRaw-COPYRIGHT`
- MinGW-w64 compiler/runtime source and licenses:
  `MinGW/` and <https://github.com/niXman/mingw-builds-binaries/releases/tag/13.1.0-rt_v11-rev1>

The SDK and compiler are local development dependencies outside the Git source
tree. `package.ps1` includes the current application's source snapshot, LibRaw,
the Qt runtime module sources, license texts, SDK SPDX metadata and build/relink
information under `Source/` and `licenses/`. Source download identities are
pinned in `Source/distribution-sources.json`. Base/declarative source archives
are checked against the SDK's source SPDX inventory (the SDK uses CRLF).

The binary also contains the MIT-licensed volk Vulkan loader and public-domain
stb code; their source/license headers are in the application source archive.
Microsoft DXC's LLVM/Microsoft and third-party notices are under `licenses/DXC`.
NSIS's copying notice is `licenses/NSIS-COPYING`, with its unmodified source
archive under `Source/` (including the CPL LZMA module).

Keep the complete installed payload and accompanying source/notice material
when redistributing. The preview is not code-signed and has not been verified
on a clean recipient machine. Packaging is not a release-compliance
certification or evidence that the Windows port has full macOS parity.
