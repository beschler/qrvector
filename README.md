# qrvector

Convert a raster QR code into a compact, transparent SVG while preserving its original module pattern.

```sh
./qrvector input.jpg -o output.svg
```

`qrvector` validates the QR code and vectorizes its existing squares. It preserves the original mask, error-correction level, encoding choices, and orientation. JPEG noise, gray edges, and background colors become exact `#000` square geometry on transparency. It never re-encodes the decoded URL or other payload.

[Requirements](#requirements) · [Installation](#installation) · [Usage](#usage) · [Options](#options) · [Supported inputs](#supported-inputs) · [Examples](#examples) · [Development](#development) · [License](#license)

## Requirements

| Requirement | Running a compiled executable | Building from source |
| --- | --- | --- |
| Operating system | macOS 13 Ventura or later | macOS with a compatible Apple development toolchain |
| Architecture | Apple Silicon (`arm64`): M-series Macs | Apple Silicon for the supplied build and tests |
| Compiler / SDK | None | Apple Clang with C++20 support and the macOS SDK, supplied by Xcode or Command Line Tools |
| Build tools | None | `make` and `xcrun`, supplied by Apple's developer tools |
| Extra runtime packages | None | All third-party source is vendored; no package-manager downloads are needed to build |

The executable uses macOS ImageIO, CoreGraphics, CoreFoundation, and the system C++ runtime. Intel Macs, Linux, and Windows are outside the current supported target. The macOS 13 minimum is the binary's deployment target; the current build and tests have been verified locally on Apple Silicon with macOS 26.7.

Conversion runs locally and offline. The tool does not upload images, fetch URLs, or print decoded payloads.

## Installation

### Build and run inside the project folder

1. On GitHub, choose **Code → Download ZIP**, extract the source archive, and name the extracted folder `qrvector`. You can also clone the repository using its URL.
2. If Apple's developer tools are missing, install them once with `xcode-select --install`.
3. Build and run from the extracted project folder:

```sh
cd /path/to/qrvector
make -j4
./qrvector examples/example.jpg -o output.svg
```

The executable is created directly at `./qrvector`. Compiler objects and generated test files stay in `build/`. Building does not install anything outside the project.

### Run a prebuilt release package

When a release provides `qrvector-macos-arm64.zip`, download that asset and extract it:

```sh
unzip qrvector-macos-arm64.zip
cd qrvector-macos-arm64
./qrvector examples/example.jpg -o output.svg
```

This package contains the executable, examples, documentation, and licenses; it needs no compiler or additional runtime packages. GitHub's automatically generated **Source code (zip)** archive contains source, not this prebuilt executable. See [release preparation](docs/RELEASING.md) for distribution details.

### Optional: make `qrvector` available from any folder

For a source checkout, install a copy into your user account's local prefix:

```sh
make install PREFIX="$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
qrvector --version
```

For an extracted prebuilt package, copy the executable directly:

```sh
mkdir -p "$HOME/.local/bin"
install -m 755 ./qrvector "$HOME/.local/bin/qrvector"
export PATH="$HOME/.local/bin:$PATH"
qrvector --version
```

Add the `export PATH=...` line to `~/.zshrc` to keep that location available in new zsh sessions. You can choose another directory already on `PATH`. The installer copies the executable and license notices; it does not edit shell configuration. From a source checkout, rebuild and run `make install` again to update the installed copy. Remove a source installation with `make uninstall PREFIX="$HOME/.local"`; for a manually copied binary, remove that copy.

Using `./qrvector` selects the file in the current project folder. Using `qrvector` selects the installed command found through `PATH`.

## Usage

```text
qrvector [OPTIONS] INPUT
qrvector --help | --version
```

`INPUT` is required for a conversion. All flags are optional; help and version need no input file. The tool processes one file per invocation.

```sh
# Default output: input.svg, alongside input.png
./qrvector input.png

# Choose the output filename
./qrvector input.jpg -o output.svg

# Four-module border for the standard QR quiet zone
./qrvector input.jpg -o output.svg --margin 4

# Explicitly replace an existing output
./qrvector input.jpg -o output.svg --force

# SVG to stdout, conversion report to stderr
./qrvector input.png -o - > output.svg

# Quote paths containing spaces
./qrvector "QR image.jpeg" -o "QR vector.svg"

# End option parsing for an input filename beginning with a dash
./qrvector -o output.svg -- -input.png
```

The same arguments work with an installed `qrvector` command. Option values must be separate arguments: use `--margin 4`, not `--margin=4`. Options may appear before or after the input, until `--`.

Output directories must already exist. Existing output files are protected unless `--force` is supplied. File output is atomic, and the input cannot be overwritten, including through an alias or hard link. Shell redirection (`> output.svg`) is controlled by your shell and can overwrite that file independently of `--force`.

The conversion report goes to stderr and includes the QR version, module count, mask, ECC level, margin, and actual size change. SVG goes to a file or stdout. There is no stdin input, batch mode, configuration file, or application-specific environment variable.

## Options

| Argument / flag | Required? | Default | Description |
| --- | --- | --- | --- |
| `INPUT` | Yes, for conversion | — | Path to one PNG or JPEG file, including `.jpg` / `.jpeg`. |
| `-o PATH`, `--output PATH` | No | Input path with its extension changed to `.svg` | Destination file. Use `-` for stdout. |
| `-m N`, `--margin N` | No | `2` | Exact transparent margin on every side, measured in QR modules. Integer from `0` to `64`. Larger source margins are trimmed; smaller ones are padded. |
| `-f`, `--force` | No | Off | Replace an existing output file. Cannot overwrite the input. |
| `-h`, `--help` | No | — | Print help to stdout and exit successfully. |
| `--version` | No | — | Print the application version to stdout and exit successfully. |
| `--` | No | — | Stop parsing flags; the following input path may begin with `-`. |

### Exit codes

| Code | Meaning |
| --- | --- |
| `0` | Conversion completed, or help/version printed. |
| `1` | Image, QR validation, conversion, or output I/O error. |
| `2` | Invalid command-line arguments. |

## Supported inputs

| Property | Requirement |
| --- | --- |
| Image format | One static PNG or JPEG image. |
| QR format | One conventional Model 2 QR code, versions 1–40 (21×21 through 177×177 modules). |
| Module shape | Square modules on a regular, unstretched square grid. |
| Resolution | At least three source pixels per module; sharper exports are preferred. |
| Orientation | Upright or rotated by 90°, 180°, or 270°; EXIF orientation is applied. |
| Contrast | Clearly dark modules on a uniform, light background. Transparent PNGs are analyzed against white. |
| Image limits | At most 40,000,000 pixels total and 20,000 pixels on either axis. |

Minor antialiasing, JPEG artifacts, off-white backgrounds, and dark colors other than pure black are normalized. Perspective photographs, arbitrary rotation, stretching, mirrored/inverted symbols, logos, decorative dots, extra foreground artwork, ambiguous pixels, multiple codes, animated/multi-image files, Micro QR, rMQR, and legacy Model 1 are outside the supported input scope.

A code that scans on a phone can still be rejected if it needs error correction. `qrvector` requires intact structure, zero codeword repairs, and agreement between the observed source grid and the decoder grid. It rejects uncertainty instead of silently changing modules.

## Output and scanner compatibility

SVG output contains one filled `#000` path. Adjacent squares are combined into the same exact union; light modules and holes remain transparent. Integer module coordinates keep the geometry square and deterministic. There is no embedded raster, background rectangle, stroke, or stored gray edge color.

The default margin is **exactly two modules on each side**. Source margins are normalized rather than preserved. The original inferred pixels per module are retained; the resulting canvas changes to fit the chosen margin. Raster edge artifacts are normalized, so their irregular pixel boundaries are not preserved. SVG viewers may still antialias vector edges at fractional display sizes.

The standard quiet zone is **four modules** on every side. The two-module default alone does not meet that requirement. Clean two-module outputs pass automated ZXing decoding tests, but those tests do not establish compatibility with most phones or dedicated scanners. Use `--margin 4` for the standard border, especially for print or placement near other graphics. A clear light surrounding surface can also supply space beyond the SVG's own canvas. [DENSO margin guidance](https://www.qrcode.com/en/howto/code.html)

Place transparent SVGs on a uniform light background, including the quiet zone. White is the most reliable backing for print. Printed module size depends on the printer and scanner. The tool validates the grid; it does not certify print quality or complete clause-by-clause compliance with ISO/IEC 18004. [DENSO size guidance](https://www.qrcode.com/en/howto/cell.html), [color/transparency guidance](https://www.qrcode.com/en/faq.html?from=20423&from_column=20423)

## Examples

The committed examples encode `https://example.com` using the same original version 3 grid (29×29 modules), mask 4, and ECC M.

| File | Role | Margin | Canvas | Bytes |
| --- | --- | ---: | ---: | ---: |
| [example.jpg](examples/example.jpg) | Raster input | 4 modules | 629×629 px | 50,664 |
| [example.png](examples/example.png) | Raster input, same module pattern | 4 modules | 629×629 px | 10,571 |
| [example.svg](examples/example.svg) | Default SVG output | 2 modules | 561×561 px | 1,573 |
| [example-margin-4.svg](examples/example-margin-4.svg) | SVG with standard quiet zone | 4 modules | 629×629 px | 1,573 |

The JPEG-to-SVG reduction is **96.9%**. Savings depend on source compression and grid complexity; a dense SVG can be larger than a well-compressed PNG. See [example details and reproduction commands](examples/README.md).

## Troubleshooting

| Message / symptom | Action |
| --- | --- |
| `no readable Model 2 QR code found` | Use a sharper export with a single conventional QR code. |
| `source grid and decoder grid disagree` / ambiguous modules | Provide a higher-resolution, aligned image without decorative styling. |
| `requires error correction` / damaged structural modules | Use an intact original. The tool deliberately does not repair the pattern. |
| `output already exists` | Choose another output path, or use `--force` intentionally. |
| `cannot create output` | Check that the output directory exists and is writable. |
| `command not found: qrvector` | Run the local executable as `./qrvector`, or add the installed directory to `PATH`. |
| `no such file: ./qrvector` | Build with `make -j4`, or enter the extracted prebuilt package folder. |
| Output will not scan | Use a light clear background, sufficient display/print size, and `--margin 4`. Test the final placement with the intended scanner. |

## Development

```sh
make -j4          # build ./qrvector
make -j4 test     # reference-grid, SVG geometry, decoder, and CLI tests
make examples    # regenerate the committed SVGs from the committed JPEG
make audit       # inspect architecture and system-only library linkage
make package     # create dist/qrvector-macos-arm64.zip
make clean       # remove build/ and ./qrvector
```

The test suite uses an independent, test-only encoder. It covers all 40 QR versions, all eight masks, all four ECC levels, right-angle rotations, PNG/JPEG/alpha/EXIF, fractional module sizes, artifacts, margins, invalid inputs, deterministic output, and overwrite protection. An independent SVG path parser checks filled geometry at nine points per module and decodes the reconstructed output with its actual margin. The encoder is excluded from the shipping executable.

See [developer documentation](docs/DEVELOPMENT.md) for the pipeline, repository layout, build variables, and fixed algorithm limits; [contribution guidelines](CONTRIBUTING.md) for changes and issue reports; and [release preparation](docs/RELEASING.md) for publishing. Changes are recorded in [CHANGELOG.md](CHANGELOG.md). Maintained by Zachary Beschler.

## License

qrvector's own source, documentation, and supplied examples are licensed under the [MIT License](LICENSE), copyright © 2026 Zachary Beschler. Bundled dependencies retain their Apache-2.0, BSD-3-Clause, and MIT licenses; see [third-party notices](THIRD_PARTY_NOTICES.md). Binary release packages include the applicable license texts.

QR Code is a registered trademark of DENSO WAVE INCORPORATED.
