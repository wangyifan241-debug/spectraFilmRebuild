# Windows application icon

`film.svg` is original vector artwork for this Windows port, licensed under
the repository's GPL-3.0-or-later license. It uses no manufacturer logo or stock
art. `film.ico` is generated from that SVG at 16, 20, 24, 32, 40, 48, 64, 128 and
256 pixels, with antialiasing and transparency. It is shared by the PE executable,
Qt window/taskbar and NSIS installer/uninstaller.

Regenerate (Qt Core, Gui and Svg development files required, not at runtime):

```powershell
cmake -S windows_UI/tools/icon -B build/icon-tool -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=<Qt SDK>
cmake --build build/icon-tool
build/icon-tool/spk_generate_icon.exe windows_UI/assets/film.svg windows_UI/assets/film.ico
```

The generated ICO is committed, so ordinary application builds do not need the
icon generator or Qt Svg. Windows Explorer may cache an old icon for a previously
used filename; this does not change the icon embedded in a newly built executable.
