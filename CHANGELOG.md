# Changelog

Changes are listed by application version. The CLI's version is defined in `src/qrvector.hpp`; dependency versions are recorded separately in `THIRD_PARTY_NOTICES.md`.

## Unreleased

- Expanded CLI help and public documentation for requirements, every argument/flag, local execution, optional global installation, build variables, and limitations.
- Added the MIT license under Zachary Beschler's name while retaining dependency licenses.
- Added PNG and standard-margin SVG reference assets, contribution guidance, and release preparation instructions.
- Added optional install/uninstall targets and included documentation/license files in binary packages.

## 0.1.1 — 2026-10-09

- Normalize SVG margins to exactly two modules per side by default; trim larger input borders and pad smaller ones.
- Add `-m` / `--margin` for an exact margin of 0–64 modules, including the standard four-module quiet zone.
- Build the executable as `./qrvector` in the project root; keep intermediate and test outputs in `build/`.
- Preserve the original QR grid, mask, ECC level, payload, and right-angle orientation while normalizing raster artifacts.
- Validate structure, codewords without repairs, remainder bits, and agreement between independent source sampling and the decoder grid.
- Cover QR versions 1–40, masks 0–7, ECC levels L/M/Q/H, SVG geometry, formats/orientation, image artifacts, input rejection, and output safety.
