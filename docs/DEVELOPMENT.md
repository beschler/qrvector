# Developer documentation

## Repository layout

```text
qrvector                   Generated executable in the project root
src/                       Application source and public internal types
config/Version.h           Bundled ZXing reader configuration/version
vendor/zxing/              Vendored ZXing-C++ source and dependency licenses
tests/                     Grid, SVG, image, and CLI verification
tests/vendor/              Independent encoder used only for test fixtures
examples/                  Committed JPEG/PNG and two SVG margin variants
docs/                      Developer and release documentation
build/                     Object/dependency files, test binary, generated fixtures
dist/                      Binary release folder and ZIP
LICENSE                    qrvector's MIT license
THIRD_PARTY_NOTICES.md      Dependency versions, provenance, and notices
```

The root executable, `build/`, and `dist/` are generated and ignored by Git. Source archives therefore require a build. `make clean` removes only the root executable and `build/`; it does not remove committed examples, release packages, or optional installed copies.

## Processing pipeline

1. ImageIO decodes one PNG/JPEG. CoreGraphics composites transparency onto white for analysis and converts sRGB pixels to grayscale. EXIF orientation is applied.
2. Otsu thresholding classifies pixels. A temporary analysis border helps locate finder patterns in tightly cropped input; this border is neither sampled as source nor exported.
3. Bundled ZXing-C++ locates and decodes a Model 2 QR code, exposing its observed module matrix separately from the decoded payload.
4. qrvector independently samples the aligned source lattice, compares it with the decoder matrix under right-angle rotation, and checks module pixel confidence. The raw source orientation is retained in `Result::grid`.
5. Fixed patterns, both metadata copies, data blocks without Reed–Solomon repairs, and remainder bits are checked. Successful decoding alone is insufficient.
6. Every output margin is normalized to the requested module count. Merged rectangles and exact orthogonal contours are serialized as candidate paths; the shorter string is used.
7. A single black SVG path is saved with a same-directory temporary file. Exclusive linking protects existing destinations; explicit overwrite uses atomic rename.

The shipping target excludes QR encoder sources and the independent test encoder. `Result::payload` supports validation; SVG geometry comes from `Result::grid`, never from re-encoding the payload.

## Units and defaults

`Grid::dark` is a row-major array of observed values, where `1` is black and `0` is light. Light modules are exported as transparent space. The matrix is square with `N = 17 + 4 × version` modules. One SVG viewBox unit is one module.

`Result::modulePixels` is the inferred source pixel pitch, possibly fractional. With normalized margin `M`, the SVG viewBox is `(N + 2M) × (N + 2M)` and its intrinsic pixel dimensions are `(N + 2M) × modulePixels`. `left`, `top`, `right`, and `bottom` are module units, all equal after `convert`. `addedMargin` records whether any source margin was smaller than the requested width.

| Setting | Current value | Runtime configurable? |
| --- | ---: | --- |
| `AppVersion` | `0.1.1` | No; application release identifier. |
| `DefaultMargin` | `2` modules | Override with `--margin`. |
| `MaxMargin` | `64` modules | No; accepted range is 0–64. |
| Minimum source pitch | `3` pixels/module | No. |
| Maximum decoded image | `40,000,000` pixels total | No. |
| Maximum image axis | `20,000` pixels | No. |
| Analysis padding | `32` pixels per side | No; temporary detector aid only. |
| Module edge exclusion | At most `1` pixel, also limited to 20% of pitch | No. |
| Minimum interior classification agreement | `95%` | No. |
| Minimum median grayscale contrast | `64` on the 0–255 scale | No. |
| Per-module mean confidence | Dark at or below 30%, light at or above 70% of the median contrast range | No. |
| Grid width/height difference | At most `1.1` source pixels over the full QR body | No. |
| Detector edge alignment tolerance | The larger of `2` pixels and 20% of pitch | No. |
| SVG numeric precision | Up to `6` decimal places for dimensions | No; module path coordinates remain integers. |

These fixed recovery thresholds are implementation settings, not CLI flags, formal QR print-quality thresholds, or application environment variables. Change them only with targeted reference-grid and rejection tests.

## Make targets

| Target | Purpose |
| --- | --- |
| `make` / `make all` | Build `./qrvector` without installation. |
| `make test` | Build/run the independent reference, image, SVG, and CLI tests. |
| `make examples` | Rebuild both committed SVGs from `examples/example.jpg`. |
| `make audit` | Show executable architecture and dynamic library dependencies. |
| `make package` | Build the release folder and `dist/qrvector-macos-arm64.zip`. |
| `make install` | Explicitly copy the executable and license notices under the selected prefix. |
| `make uninstall` | Remove the install target's known files under the same prefix. |
| `make clean` | Remove `build/` and `./qrvector`. |

### Optional build and installation variables

All listed variables are optional; none is required for the normal local build. Supply overrides on the make command line.

| Variable | Default | Meaning |
| --- | --- | --- |
| `CXX` | `xcrun clang++` | C++ compiler invocation. |
| `CC` | `xcrun clang` | C compiler invocation for bundled libzueci. |
| `CXXFLAGS` | C++20, `-O2`, `-DNDEBUG`, `arm64`, macOS 13 target, warnings/dependency generation | C++ compiler flags; see the Makefile for the exact complete value. |
| `CFLAGS` | `-O2`, `arm64`, macOS 13 target, decoder-only libzueci, dependency generation | C compiler flags; see the Makefile for the exact complete value. |
| `CPPFLAGS` | Project/vendor include paths and `ZXING_INTERNAL` | Preprocessor configuration required by the bundled decoder integration. |
| `FRAMEWORKS` | ImageIO, CoreGraphics, CoreFoundation | System framework linker flags. |
| `PREFIX` | `$HOME/.local` | Optional installation root. |
| `BINDIR` | `$(PREFIX)/bin` | Directory receiving the installed executable. |
| `DATADIR` | `$(PREFIX)/share/qrvector` | Directory receiving license notices. |
| `DESTDIR` | Empty | Optional staging root prepended to installation paths; useful for package tests. |

For example, `make install PREFIX=/opt/homebrew` selects an existing prefix, while `make install PREFIX=/usr/local DESTDIR=/path/to/stage` stages files beneath that directory. Build/install commands do not add a prefix to `PATH` or edit shell profiles. The executable is copied, not symlinked to the checkout; rebuild and reinstall to update it. `make uninstall` removes known files and only removes empty notice directories, leaving unrelated files intact.

Changing compiler/linker flags does not by itself add Intel, Linux, or Windows support. `config/Version.h` refers to ZXing-C++ 3.1.1, not the application's version. Keep its metadata aligned with the vendored source.

`config/Version.h` is compile-time decoder integration, not a user configuration file. It defines `ZXING_READERS`, enables `ZXING_ENABLE_QRCODE`, and disables `ZXING_ENABLE_1D`, `ZXING_ENABLE_AZTEC`, `ZXING_ENABLE_DATAMATRIX`, `ZXING_ENABLE_MAXICODE`, and `ZXING_ENABLE_PDF417`. Its `ZXING_VERSION_*` macros record the vendored dependency version. The Makefile's derived `ZX_BASE`, `ZX_QR`, `ZX_OBJ`, and `APP_OBJ` lists select decoder sources and build objects; they are not supported user settings.

## Verification

The independent Nayuki encoder is compiled only into `build/test-qrvector`. Tests cover all 40 versions and combinations exercising every ECC level and mask, validate original grid equality, evaluate SVG winding at nine points per module, and decode the reconstructed SVG with its actual output margin.

Image cases cover JPEG/PNG, transparency, EXIF, right-angle rotation, color/edge normalization, fractional pitch, and asymmetric source borders. Rejection cases cover damaged structure/data/remainder bits, insufficient resolution/contrast, ambiguity, unsupported styles, inversion/mirroring/stretching, multiple codes, and invalid files. CLI checks cover documented aliases, defaults, stdout, optional margins, argument errors, exit codes, and file protection.

Run `make -j4 test` after relevant changes. Regenerate SVG examples with `make examples` and inspect their byte counts before editing documentation claims. Run `make audit` when changing dependencies. The linked libraries should remain macOS system libraries only.

## Updating dependencies

Retain original copyright/license headers in vendored files. Record the exact upstream tag, archive checksum, and any local modifications in `THIRD_PARTY_NOTICES.md`. The added libzueci license file supplies the full BSD-3-Clause text corresponding to its SPDX notices. Never replace dependency licenses with qrvector's MIT license.
