# 净页 / ScanCleaner

一个独立的 Qt 6 / C++ / QML 扫描件处理工具。支持 PDF 及 PNG、JPG、TIFF、BMP、WebP 图片。处理在本机运行。

## 下载

在 [GitHub Releases](https://github.com/duhbbx/ScanCleaner/releases) 选择对应的安装包，并使用 `SHA256SUMS.txt` 核对下载。

| 系统 | 发布包 |
| --- | --- |
| Windows 10 1809+ / Windows 11 | `windows-10-11-x64.zip`，解压后打开 `bin/ScanCleaner.exe` |
| Windows 7 SP1 / Windows 8 / 8.1 | `windows-7-8-x64-experimental.zip`，社区 Qt 补丁构建，旧系统上的实际运行仍待验证 |
| macOS | `macos-x86_64.zip`（Intel）、`macos-arm64.zip`（Apple 芯片）、`macos-universal.zip`（两种芯片） |
| Ubuntu 24.04 / Debian 13 | 对应发行版的 `.deb`，使用 `sudo apt install ./文件名.deb` 安装 |
| Arch Linux | `.pkg.tar.zst`，使用 `sudo pacman -U ./文件名.pkg.tar.zst` 安装 |

Windows ZIP 需要与系统相容的 Microsoft Visual C++ 运行库。macOS 构建使用本地签名，未经 Apple 公证。Linux 安装后可从应用菜单启动 **净页**，也可以运行 `ScanCleaner`。

## 使用

1. 打开或拖入扫描 PDF 或单张图片，左右查看原稿与处理效果。
2. 默认“柔和去底”保留字边灰度；需要仅黑白两色时选“纯黑白”。
3. 调整去底色和文字深浅。放大检查细字、小数点、标点和公式。
4. 在左侧选择导出格式。PNG 无损保存，JPG 以高质量压缩。图片导出保留像素尺寸，也可生成单页 PDF，此时 DPI 决定页面的打印尺寸。
5. PDF 导出为 PDF 时可选择页码范围和 DPI。导出为 PNG 或 JPG 时保存当前预览页。可取消导出。

原稿不覆盖。取消或失败时，已存在的目标文件也不会被半成品替换。页面按原尺寸导出，但输出为图像 PDF，不保留原有文字层、书签、链接及附件；不包含 OCR、自动纠偏、照片区域自动识别或复杂曲面校正。提高 DPI 不等于恢复原扫描细节。

PDF 预览最高 300 DPI，长边一般不超过 3200 像素。图片预览按需缩小，导出使用原始像素。单张图片或 PDF 渲染页上限为 2400 万像素。PDF 导出可选 150/200/300/400 DPI，最终效果可能有细微差异。多页任务逐页处理，取消会在当前页面处理阶段结束后生效。

## 编译

依赖：CMake 3.24+、C++17、Qt 6.4+（Core、Gui、Quick、QuickControls2、Pdf、Concurrent，测试需要 Test）和 OpenCV 4 或 5（core、imgproc）。TIFF 和 WebP 需要 Qt Image Formats。发行版构建使用的依赖和命令见 [构建工作流](.github/workflows/release.yml)。

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

macOS 开发运行：`build/ScanCleaner.app/Contents/MacOS/ScanCleaner`。也可给出 `--input /path/book.pdf` 自动打开。

命令行处理：

```sh
ScanCleaner --input book.pdf --output book-cleaned.pdf --mode bw --dpi 300
ScanCleaner --input book.pdf --output sample.pdf --mode gray --first 1 --last 3
ScanCleaner --input scan.jpg --output scan-cleaned.png --mode gray
ScanCleaner --input book.pdf --output page-4.png --first 4 --dpi 300
```

可选参数：`--background 0..100`、`--darkness -20..20`、`--sharpness 0..100`。输出路径必须与输入不同。已有目标文件在命令行模式下会被成功结果替换，请使用新文件名。

Linux 无桌面批处理可设置 `QT_QPA_PLATFORM=offscreen`。

Windows 7/8 的实验构建需要使用社区补丁 Qt SDK，配置时加上 `-DSCAN_LEGACY_WINDOWS=ON`。这个选项会启用 Qt Quick 的软件渲染，并设置相应的 Windows 子系统版本。SDK 的下载地址、校验值和对应源码见 [构建脚本](packaging/windows/install-legacy-qt.ps1)和[第三方声明](THIRD_PARTY_NOTICES.md)。

## 处理原理

PDF 渲染为白底图像，转灰度；用形态学闭运算估计背景并平滑，进行除法光照补偿；纯黑白模式使用 Sauvola 局部阈值，灰度模式进行黑白点映射。默认采用柔和去底和轻度锐化，保留抗锯齿边缘；PDF 图像使用无损压缩，纯黑白页面以 1 位图像写出。不使用生成式图像模型，不重绘文字。参数窗大小随渲染 DPI 调整。

算法基于 OpenCV 官方的形态学、局部均值与平方均值运算自行组合实现，仅链接 core / imgproc。

## 许可

本项目源码采用 [MIT](LICENSE)。预编译包内库的许可与对应源码见 [第三方声明](THIRD_PARTY_NOTICES.md)。
