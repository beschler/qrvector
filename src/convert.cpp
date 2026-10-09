// Copyright (c) 2026 Zachary Beschler
// SPDX-License-Identifier: MIT
#include "qrvector.hpp"
#include "Barcode.h"
#include "BitMatrix.h"
#include "ByteArray.h"
#include "ReadBarcode.h"
#include "ReaderOptions.h"
#include "ReedSolomon.h"
#include "qrcode/QRBitMatrixParser.h"
#include "qrcode/QRDataBlock.h"
#include "qrcode/QRDataMask.h"
#include "qrcode/QRFormatInformation.h"
#include "qrcode/QRVersion.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace qrvector {
namespace {
[[noreturn]] void invalid(const std::string& reason) { throw std::runtime_error(reason); }

int threshold(const Raster& image) {
    // Otsu thresholding; the midpoint of tied optima is deterministic for flat color clusters.
    std::array<uint64_t, 256> histogram{};
    for (auto v : image.gray) ++histogram[v];
    double totalSum = 0;
    for (int i = 0; i < 256; ++i) totalSum += i * double(histogram[i]);
    uint64_t count = 0;
    double sum = 0, bestScore = -1;
    int first = 0, last = 0;
    for (int t = 0; t < 255; ++t) {
        count += histogram[t]; sum += t * double(histogram[t]);
        auto other = image.gray.size() - count;
        if (!count || !other) continue;
        double delta = sum / count - (totalSum - sum) / other;
        double score = double(count) * other * delta * delta;
        if (score > bestScore) { bestScore = score; first = last = t; }
        else if (score == bestScore) last = t;
    }
    if (bestScore <= 0) invalid("no readable QR code found");
    return (first + last) / 2;
}

int bch(int value, int shift, int polynomial) {
    int remainder = value;
    for (int i = 0; i < shift; ++i)
        remainder = (remainder << 1) ^ ((remainder >> (shift - 1)) * polynomial);
    return (value << shift) | remainder;
}

void validate(const Grid& grid, Result& result) {
    using namespace ZXing;
    namespace QR = ZXing::QRCode;
    int n = grid.size, versionNumber = (n - 17) / 4;
    auto version = QR::Version::Model2(versionNumber);
    if (!version || version->dimension() != n) invalid("unsupported QR grid size");
    BitMatrix bits(n, n);
    for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) if (grid.at(x, y)) bits.set(x, y);
    auto format = QR::ReadFormatInformation(bits);
    if (!format.isValid() || format.type() != QR::Type::Model2 || format.isMirrored)
        invalid("only conventional, non-mirrored Model 2 QR codes are supported");
    std::vector<int> expected(size_t(n) * n, -1);
    auto expect = [&](int x, int y, bool dark) {
        if (x >= 0 && y >= 0 && x < n && y < n) expected[size_t(y) * n + x] = dark;
    };
    for (auto [ox, oy] : {std::pair{0, 0}, {n - 7, 0}, {0, n - 7}}) {
        for (int y = -1; y <= 7; ++y) for (int x = -1; x <= 7; ++x) {
            bool inside = x >= 0 && x <= 6 && y >= 0 && y <= 6;
            bool dark = inside && (x == 0 || x == 6 || y == 0 || y == 6 ||
                (x >= 2 && x <= 4 && y >= 2 && y <= 4));
            expect(ox + x, oy + y, dark);
        }
    }
    for (int i = 8; i < n - 8; ++i) { expect(i, 6, i % 2 == 0); expect(6, i, i % 2 == 0); }
    auto& centers = version->alignmentPatternCenters();
    for (int cy : centers) for (int cx : centers) {
        if ((cx == 6 && (cy == 6 || cy == n - 7)) || (cy == 6 && cx == n - 7)) continue;
        for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx)
            expect(cx + dx, cy + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
    }
    int formatBits = bch((QR::BitsFromECLevel(format.ecLevel) << 3) | format.dataMask, 10, 0x537) ^ 0x5412;
    auto fb = [&](int i) { return ((formatBits >> i) & 1) != 0; };
    for (int i = 0; i <= 5; ++i) expect(8, i, fb(i));
    expect(8, 7, fb(6)); expect(8, 8, fb(7)); expect(7, 8, fb(8));
    for (int i = 9; i < 15; ++i) expect(14 - i, 8, fb(i));
    for (int i = 0; i < 8; ++i) expect(n - 1 - i, 8, fb(i));
    for (int i = 8; i < 15; ++i) expect(8, n - 15 + i, fb(i));
    expect(8, n - 8, true);
    if (versionNumber >= 7) {
        int vbits = bch(versionNumber, 12, 0x1f25);
        for (int i = 0; i < 18; ++i) {
            int a = n - 11 + i % 3, b = i / 3;
            expect(a, b, (vbits >> i) & 1); expect(b, a, (vbits >> i) & 1);
        }
    }
    for (size_t i = 0; i < expected.size(); ++i)
        if (expected[i] != -1 && expected[i] != grid.dark[i])
            invalid("QR structural modules are damaged; refusing to change the original pattern");

    // Decodability alone is insufficient: require zero corrections to the observed codewords.
    auto codewords = QR::ReadCodewords(bits, *version, format);
    auto blocks = QR::DataBlock::GetDataBlocks(codewords, *version, format.ecLevel);
    if (blocks.empty()) invalid("invalid QR data blocks");
    for (auto& block : blocks) {
        auto original = block.codewords();
        auto corrected = original;
        auto unused = ReedSolomonDecode(RSField::QRCode, corrected, int(corrected.size()) - block.numDataCodewords());
        if (!unused || corrected != original)
            invalid("QR data requires error correction; refusing to repair or re-encode the pattern");
    }
    auto functions = version->buildFunctionPattern();
    int dataBits = 0;
    bool up = true;
    for (int x = n - 1; x > 0; x -= 2) {
        if (x == 6) --x;
        for (int row = 0; row < n; ++row) for (int col = 0; col < 2; ++col) {
            int y = up ? n - 1 - row : row, xx = x - col;
            if (!functions.get(xx, y) && dataBits++ >= version->totalCodewords() * 8 &&
                (grid.at(xx, y) != QR::GetDataMaskBit(format.dataMask, xx, y)))
                invalid("QR remainder bits are invalid");
        }
        up = !up;
    }
    result.version = versionNumber;
    result.mask = format.dataMask;
    result.ecLevel = QR::ToString(format.ecLevel);
}

void checkPixels(const Raster& image, const Grid& physical, int left, int top, double pitch, int t) {
    std::vector<int> darkMeans, lightMeans;
    for (int y = 0; y < physical.size; ++y) for (int x = 0; x < physical.size; ++x) {
        // Ignore up to one pixel of raster edge artifacts; validate the remaining square interior.
        double inset = std::min(1.0, pitch * .2);
        int x0 = int(std::ceil(left + x * pitch + inset - .5));
        int x1 = int(std::floor(left + (x + 1) * pitch - inset - .5));
        int y0 = int(std::ceil(top + y * pitch + inset - .5));
        int y1 = int(std::floor(top + (y + 1) * pitch - inset - .5));
        int count = 0, matches = 0, sum = 0;
        for (int yy = y0; yy <= y1; ++yy) for (int xx = x0; xx <= x1; ++xx) {
            int v = image.at(xx, yy); ++count; sum += v;
            if ((v <= t) == physical.at(x, y)) ++matches;
        }
        if (!count || matches < .95 * count)
            invalid("ambiguous or non-square QR modules; use a sharper conventional QR image");
        (physical.at(x, y) ? darkMeans : lightMeans).push_back(sum / count);
    }
    if (darkMeans.empty() || lightMeans.empty()) invalid("invalid QR contrast");
    auto median = [](std::vector<int>& v) { std::sort(v.begin(), v.end()); return v[v.size() / 2]; };
    int dark = median(darkMeans), light = median(lightMeans);
    if (light - dark < 64) invalid("QR contrast is too low to recover the pattern confidently");
    double low = dark + .3 * (light - dark), high = dark + .7 * (light - dark);
    for (int v : darkMeans) if (v > low) invalid("ambiguous dark QR module");
    for (int v : lightMeans) if (v < high) invalid("ambiguous light QR module");
}
} // namespace

Grid rotateClockwise(const Grid& input) {
    Grid result{input.size, std::vector<uint8_t>(input.dark.size())};
    for (int y = 0; y < input.size; ++y) for (int x = 0; x < input.size; ++x)
        result.dark[size_t(x) * input.size + input.size - 1 - y] = input.at(x, y);
    return result;
}

Result convert(const Raster& image, int margin) {
    using namespace ZXing;
    if (margin < 0 || margin > MaxMargin) invalid("margin must be an integer from 0 to 64 modules");
    if (image.width <= 0 || image.height <= 0 || image.gray.size() != size_t(image.width) * image.height)
        invalid("invalid image buffer");
    int t = threshold(image);
    // Padding helps locate cropped finder patterns, but is never sampled or exported as source data.
    int pad = 32, w = image.width + pad * 2, h = image.height + pad * 2;
    std::vector<uint8_t> binary(size_t(w) * h, 255);
    int left = image.width, top = image.height, right = -1, bottom = -1;
    for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) if (image.at(x, y) <= t) {
        binary[size_t(y + pad) * w + x + pad] = 0;
        left = std::min(left, x); top = std::min(top, y);
        right = std::max(right, x); bottom = std::max(bottom, y);
    }
    auto options = ReaderOptions().formats(BarcodeFormat::QRCodeModel2).tryInvert(false).tryDownscale(false)
        .binarizer(Binarizer::BoolCast).maxNumberOfSymbols(2).returnErrors(true);
    auto barcodes = ReadBarcodes(ImageView(binary.data(), w, h, ImageFormat::Lum), options);
    if (barcodes.size() != 1) invalid(barcodes.empty() ? "no readable Model 2 QR code found" : "input contains multiple QR codes");
    auto& barcode = barcodes.front();
    if (!barcode.isValid()) invalid("QR code was detected but could not be decoded");
    if (barcode.isMirrored() || barcode.isInverted()) invalid("mirrored and inverted QR codes are not supported");
    auto symbol = barcode.symbol();
    int n = symbol.width();
    if (n != symbol.height() || n < 21 || n > 177 || n % 4 != 1) invalid("unsupported QR grid size");
    double pitchX = double(right - left + 1) / n, pitchY = double(bottom - top + 1) / n;
    if (std::abs(pitchX - pitchY) * n > 1.1) invalid("QR image is stretched or contains extra foreground artwork");
    double pitch = (pitchX + pitchY) / 2;
    if (pitch < 3) invalid("image resolution is too low; provide at least 3 pixels per QR module");
    auto corners = barcode.position();
    double tolerance = std::max(2.0, pitch * .2);
    for (int i = 0; i < 4; ++i) {
        auto a = corners[i], b = corners[(i + 1) % 4];
        if (std::min(std::abs(b.x - a.x), std::abs(b.y - a.y)) > tolerance)
            invalid("rotated or perspective-distorted photographs are not supported; use an aligned QR image");
    }
    Grid detected{n, std::vector<uint8_t>(size_t(n) * n)};
    Grid physical = detected;
    for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) {
        detected.dark[size_t(y) * n + x] = *symbol.data(x, y) == 0;
        physical.dark[size_t(y) * n + x] = image.at(
            std::min(right, int(left + (x + .5) * pitch)),
            std::min(bottom, int(top + (y + .5) * pitch))) <= t;
    }
    auto rotated = detected;
    bool same = false;
    for (int i = 0; i < 4; ++i) { same |= rotated == physical; rotated = rotateClockwise(rotated); }
    if (!same) invalid("source grid and decoder grid disagree; refusing an uncertain conversion");
    checkPixels(image, physical, left, top, pitch, t);
    Result result;
    validate(detected, result);
    result.grid = std::move(physical);
    result.payload.assign(barcode.bytes().begin(), barcode.bytes().end());
    result.modulePixels = pitch;
    std::array<double, 4> margins{left / pitch, top / pitch,
        (image.width - left) / pitch - n, (image.height - top) / pitch - n};
    // Crop larger source borders and pad smaller ones; never carry over asymmetric source margins.
    result.left = result.top = result.right = result.bottom = margin;
    result.addedMargin = std::any_of(margins.begin(), margins.end(), [margin](double v) { return v < margin - 1e-6; });
    result.svg = makeSVG(result);
    return result;
}
} // namespace qrvector
