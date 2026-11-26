# Third-party software

ScanCleaner source is MIT licensed. Libraries shipped with its binaries have
their own licenses. The Linux packages use libraries provided by the
distribution.

## Qt

The Windows and macOS packages dynamically link to Qt, including Qt PDF.
These libraries are distributed under LGPL-3.0. The license texts are included
in the binary archive. The PDF module incorporates PDFium and other components
distributed under their own licenses. Qt's attribution pages identify those
components and their license texts.

- [Qt 6.8 licensing](https://doc.qt.io/qt-6.8/licensing.html)
- [Qt 6.8 third-party notices](https://doc.qt.io/qt-6.8/licenses-used-in-qt.html)
- [Qt PDF licensing and attributions](https://doc.qt.io/qt-6.8/qtpdf-licensing.html)
- [Corresponding source for the official Qt 6.8.3 binaries](https://download.qt.io/archive/qt/6.8/6.8.3/single/qt-everywhere-src-6.8.3.tar.xz)
- [Legacy Windows SDK and corresponding patched Qt 6.8.4 source](https://github.com/xanasoft/qt-builds/releases/tag/v6.8.4-ssl-lgpl)
- [Win7/Win8 patch source](https://github.com/crystalidea/qt6windows7)

You may replace the Qt shared libraries in a package with ABI-compatible
versions. If macOS requires a signature after replacement, you can rebuild
from the instructions in this repository or re-sign your modified app bundle
locally. LGPL rights, including reverse engineering for debugging modifications
to the library, apply to distributed binaries.

## OpenCV

The Windows and macOS packages include the OpenCV 4.12.0 core and imgproc
modules under the Apache License 2.0. The license is included in the packages.
The build flags are in `.github/workflows/release.yml` and
`packaging/build-opencv.cmake`.

- [OpenCV 4.12.0 source, license, and third-party notices](https://github.com/opencv/opencv/tree/4.12.0)
