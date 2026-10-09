// Copyright (c) 2026 Zachary Beschler
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace qrvector {
inline constexpr const char* AppVersion = "0.1.1";
// Output margins are normalized per side; four modules is the standard QR quiet zone.
inline constexpr int DefaultMargin = 2;
inline constexpr int MaxMargin = 64;
struct Raster {
    int width = 0, height = 0;
    std::vector<uint8_t> gray; // Row-major analysis pixels: 0 = black, 255 = white; EXIF already applied.
    uint8_t at(int x, int y) const { return gray.at(size_t(y) * width + x); }
};
struct Grid {
    int size = 0;
    std::vector<uint8_t> dark; // Row-major observed modules: 1 = black, 0 = light (exported as transparent).
    bool at(int x, int y) const { return dark.at(size_t(y) * size + x) != 0; }
    bool operator==(const Grid&) const = default;
};
struct Result {
    Grid grid; // In the same physical orientation as the source image.
    int version = 0, mask = 0; // QR version 1–40 and original mask 0–7, separate from AppVersion.
    std::string ecLevel; // Original QR error-correction level: L, M, Q, or H.
    std::vector<uint8_t> payload; // Validation only; never used to generate a new QR or printed.
    double left = DefaultMargin, top = DefaultMargin, right = DefaultMargin, bottom = DefaultMargin; // Module units.
    double modulePixels = 1; // Inferred source pixels per square module; can be fractional.
    bool addedMargin = false; // At least one source margin was narrower than the requested output margin.
    std::string svg;
};

// Single PNG/JPEG, EXIF-normalized, with transparency composited onto white for analysis.
Raster loadImage(const std::filesystem::path& path);
Grid rotateClockwise(const Grid& grid);
// Preserve the observed grid and normalize all margins to an integer in [0, MaxMargin].
Result convert(const Raster& image, int margin = DefaultMargin);
std::string makeSVG(const Result& result);
// Atomic file output; refuses to replace an existing destination unless overwrite is true.
void writeSVG(const std::filesystem::path& path, const std::string& svg, bool overwrite);
} // namespace qrvector
