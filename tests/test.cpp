// Copyright (c) 2026 Zachary Beschler
// SPDX-License-Identifier: MIT
#include "qrvector.hpp"
#include "vendor/qrcodegen.hpp"
#include "Barcode.h"
#include "ReadBarcode.h"
#include "ReaderOptions.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace qrvector;
namespace {
int checks = 0;
const std::vector<uint8_t> payload{'q', 'r', 0, 255, 'v'};
void require(bool condition, const std::string& message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F action, const std::string& message) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected, "did not reject: " + message);
}
Grid reference(int version, int ecc, int mask) {
    using namespace qrcodegen;
    auto code = QrCode::encodeSegments({QrSegment::makeBytes(payload)}, QrCode::Ecc(ecc), version, version, mask, false);
    Grid grid{code.getSize(), std::vector<uint8_t>(size_t(code.getSize()) * code.getSize())};
    for (int y = 0; y < grid.size; ++y) for (int x = 0; x < grid.size; ++x)
        grid.dark[size_t(y) * grid.size + x] = code.getModule(x, y);
    return grid;
}
Raster raster(const Grid& grid, int pitch = 5, int margin = 4, int dark = 0, int light = 255) {
    int width = (grid.size + 2 * margin) * pitch;
    Raster image{width, width, std::vector<uint8_t>(size_t(width) * width, uint8_t(light))};
    for (int y = 0; y < grid.size * pitch; ++y) for (int x = 0; x < grid.size * pitch; ++x)
        image.gray[size_t(y + margin * pitch) * width + x + margin * pitch] = grid.at(x / pitch, y / pitch) ? dark : light;
    return image;
}
Raster fractionalRaster(const Grid& grid) {
    double pitch = 5.37, left = 29.4, top = 23.7;
    Raster image{250, 235, std::vector<uint8_t>(250 * 235, 255)};
    for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
        int black = 0;
        for (int sy = 0; sy < 8; ++sy) for (int sx = 0; sx < 8; ++sx) {
            int gx = int(std::floor((x + (sx + .5) / 8 - left) / pitch));
            int gy = int(std::floor((y + (sy + .5) / 8 - top) / pitch));
            black += gx >= 0 && gy >= 0 && gx < grid.size && gy < grid.size && grid.at(gx, gy);
        }
        image.gray[size_t(y) * image.width + x] = 255 * (64 - black) / 64;
    }
    return image;
}
struct Point { int x, y; };
std::vector<std::vector<Point>> parseSVG(const std::string& svg) {
    require(svg.find("fill=\"#000\"") != std::string::npos, "black fill");
    require(svg.find("<image") == std::string::npos && svg.find("<rect") == std::string::npos, "no raster or background");
    auto begin = svg.find(" d=\"");
    require(begin != std::string::npos, "SVG path exists");
    auto d = svg.substr(begin + 4, svg.find('"', begin + 4) - begin - 4);
    const char* p = d.c_str();
    auto integer = [&]() {
        char* end;
        long value = std::strtol(p, &end, 10);
        require(end != p, "integer SVG coordinate"); p = end;
        return int(value);
    };
    Point current{0, 0};
    std::vector<std::vector<Point>> loops;
    while (*p) {
        char command = *p++;
        if (command == 'M' || command == 'm') {
            int x = integer(), y = integer();
            if (command == 'm') { x += current.x; y += current.y; }
            current = {x, y}; loops.push_back({current});
        } else if (command == 'h' || command == 'v') {
            int distance = integer();
            (command == 'h' ? current.x : current.y) += distance;
            loops.back().push_back(current);
        } else if (command == 'z') {
            auto first = loops.back().front();
            require(current.x == first.x || current.y == first.y, "orthogonal closure");
            loops.back().push_back(first); current = first;
        } else throw std::runtime_error("unexpected SVG command");
    }
    return loops;
}
void verifySVG(const Result& result, const Grid& expected) {
    int margin = int(result.left), extent = expected.size + 2 * margin;
    require(result.left == margin && result.top == margin && result.right == margin && result.bottom == margin, "uniform integer margin");
    require(result.svg.find("viewBox=\"0 0 " + std::to_string(extent) + " " + std::to_string(extent) + "\"") != std::string::npos, "SVG canvas includes exact margin");
    require(result.svg.find("transform=\"translate(" + std::to_string(margin) + " " + std::to_string(margin) + ")\"") != std::string::npos, "SVG translation matches margin");
    auto loops = parseSVG(result.svg);
    for (auto& loop : loops) for (auto point : loop)
        require(point.x >= 0 && point.y >= 0 && point.x <= expected.size && point.y <= expected.size, "path stays within QR grid");
    Grid reconstructed{expected.size, std::vector<uint8_t>(expected.dark.size())};
    // Independently evaluate nonzero SVG winding at nine points in every module.
    for (int y = 0; y < expected.size; ++y) for (double offsetY : {.125, .5, .875}) {
        std::vector<std::pair<int, int>> events;
        double scan = y + offsetY;
        for (auto& loop : loops) for (size_t i = 1; i < loop.size(); ++i) {
            auto a = loop[i - 1], b = loop[i];
            if (a.x == b.x && scan >= std::min(a.y, b.y) && scan < std::max(a.y, b.y))
                events.emplace_back(a.x, b.y > a.y ? 1 : -1);
        }
        std::sort(events.begin(), events.end());
        for (double offsetX : {.125, .5, .875}) {
            size_t next = 0; int winding = 0;
            for (int x = 0; x < expected.size; ++x) {
                while (next < events.size() && events[next].first < x + offsetX) winding += events[next++].second;
                require((winding != 0) == expected.at(x, y), "SVG fill differs from reference at " + std::to_string(x) + "," + std::to_string(y));
                reconstructed.dark[size_t(y) * expected.size + x] = winding != 0;
            }
        }
    }
    // Decode with the actual exported margin, rather than silently padding it to four modules.
    auto rendered = raster(reconstructed, 5, margin);
    auto decoded = ZXing::ReadBarcode(ZXing::ImageView(rendered.gray.data(), rendered.width, rendered.height, ZXing::ImageFormat::Lum),
        ZXing::ReaderOptions().formats(ZXing::BarcodeFormat::QRCodeModel2));
    require(decoded.isValid(), "rendered SVG decodes");
    require(std::vector<uint8_t>(decoded.bytes().begin(), decoded.bytes().end()) == payload, "rendered SVG payload unchanged");
}
Result verify(const Raster& image, const Grid& expected, int version, int ecc, int mask, bool svg = true) {
    auto result = convert(image);
    require(result.grid == expected, "original grid changed (version " + std::to_string(version) + ")");
    require(result.version == version && result.mask == mask, "version or mask changed");
    require(result.ecLevel == std::vector<std::string>{"L", "M", "Q", "H"}.at(ecc), "ECC changed");
    require(result.payload == payload, "payload changed");
    if (svg) verifySVG(result, expected);
    return result;
}
void saveImage(const Raster& raster, const std::filesystem::path& path, bool jpeg, bool transparent = false, int orientation = 1) {
    std::vector<uint8_t> rgba(raster.gray.size() * 4);
    for (size_t i = 0; i < raster.gray.size(); ++i) {
        auto v = raster.gray[i];
        rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = v;
        rgba[i * 4 + 3] = transparent && v == 255 ? 0 : 255;
        if (rgba[i * 4 + 3] == 0) rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 0;
    }
    auto color = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    auto provider = CGDataProviderCreateWithData(nullptr, rgba.data(), rgba.size(), nullptr);
    auto image = CGImageCreate(raster.width, raster.height, 8, 32, raster.width * 4, color,
        uint32_t(kCGImageAlphaPremultipliedLast) | uint32_t(kCGBitmapByteOrder32Big), provider, nullptr, false, kCGRenderingIntentDefault);
    auto string = path.string();
    auto url = CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8*>(string.data()), string.size(), false);
    auto destination = CGImageDestinationCreateWithURL(url, jpeg ? CFSTR("public.jpeg") : CFSTR("public.png"), 1, nullptr);
    require(destination != nullptr, "create fixture destination");
    float quality = .8f;
    auto q = CFNumberCreate(nullptr, kCFNumberFloatType, &quality);
    auto o = CFNumberCreate(nullptr, kCFNumberIntType, &orientation);
    const void* keys[]{kCGImageDestinationLossyCompressionQuality, kCGImagePropertyOrientation};
    const void* values[]{q, o};
    auto props = CFDictionaryCreate(nullptr, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CGImageDestinationAddImage(destination, image, props);
    require(CGImageDestinationFinalize(destination), "save fixture");
    CFRelease(props); CFRelease(o); CFRelease(q); CFRelease(destination); CFRelease(url);
    CGImageRelease(image); CGDataProviderRelease(provider); CGColorSpaceRelease(color);
}
} // namespace

int main() {
    try {
        std::filesystem::create_directories("build/fixtures");
        for (int v = 1; v <= 40; ++v) {
            int ecc = (v - 1) % 4, mask = (v - 1) % 8;
            auto grid = reference(v, ecc, mask);
            verify(raster(grid), grid, v, ecc, mask);
        }
        for (int ecc = 0; ecc < 4; ++ecc) for (int mask = 0; mask < 8; ++mask) {
            auto grid = reference(3, ecc, mask);
            verify(raster(grid), grid, 3, ecc, mask);
        }
        std::cout << "Passed all 40 versions, all 8 masks and all 4 ECC levels.\n";
        auto base = reference(4, 2, 5), rotated = base;
        for (int angle = 0; angle < 4; ++angle) {
            verify(raster(rotated), rotated, 4, 2, 5);
            rotated = rotateClockwise(rotated);
        }
        verify(raster(base, 3), base, 4, 2, 5);
        auto tinted = raster(base, 7, 4, 25, 232);
        verify(tinted, base, 4, 2, 5);
        auto normal = raster(base, 8);
        auto aa = normal;
        for (int y = 1; y < normal.height - 1; ++y) for (int x = 1; x < normal.width - 1; ++x) {
            int sum = normal.at(x, y) * 6 + normal.at(x - 1, y) + normal.at(x + 1, y) + normal.at(x, y - 1) + normal.at(x, y + 1);
            aa.gray[size_t(y) * normal.width + x] = sum / 10;
        }
        verify(aa, base, 4, 2, 5);
        auto fractional = fractionalRaster(base);
        auto fractionalResult = verify(fractional, base, 4, 2, 5);
        require(!fractionalResult.addedMargin && fractionalResult.svg.find("viewBox=\"0 0 37 37\"") != std::string::npos,
            "fractional modules normalize asymmetric source margins");
        for (int margin = 0; margin < 8; ++margin) {
            auto result = verify(raster(base, 5, margin), base, 4, 2, 5, false);
            require(result.left == 2 && result.top == 2 && result.right == 2 && result.bottom == 2, "exact two-module output margin");
            require(result.addedMargin == (margin < 2), "margin addition flag");
        }
        for (int outputMargin : {0, 1, 2, 4, 8, 64}) {
            auto result = convert(normal, outputMargin);
            require(result.grid == base && result.payload == payload, "custom margin preserves original QR");
            require(result.left == outputMargin, "custom margin applied");
            verifySVG(result, base);
        }
        auto standard = convert(normal, 4);
        writeSVG("build/fixtures/standard-margin.svg", standard.svg, true);
        rejects([&] { convert(normal, -1); }, "negative margin");
        rejects([&] { convert(normal, 65); }, "excessive margin");
        saveImage(normal, "build/fixtures/sample.png", false);
        saveImage(normal, "build/fixtures/sample.jpg", true);
        saveImage(normal, "build/fixtures/transparent.png", false, true);
        for (auto name : {"sample.png", "sample.jpg", "transparent.png"})
            verify(loadImage(std::filesystem::path("build/fixtures") / name), base, 4, 2, 5);
        for (auto [orientation, turns] : {std::pair{3, 2}, {6, 1}, {8, 3}}) {
            saveImage(normal, "build/fixtures/oriented.jpg", true, false, orientation);
            auto expected = base;
            for (int i = 0; i < turns; ++i) expected = rotateClockwise(expected);
            verify(loadImage("build/fixtures/oriented.jpg"), expected, 4, 2, 5);
        }
        auto deterministic = convert(normal);
        require(convert(normal).svg == deterministic.svg, "deterministic output");
        writeSVG("build/fixtures/sample.svg", deterministic.svg, true);
        rejects([&] { writeSVG("build/fixtures/sample.svg", "corruption", false); }, "overwriting output");
        std::ifstream file("build/fixtures/sample.svg");
        require(std::string(std::istreambuf_iterator<char>(file), {}) == deterministic.svg, "existing output unchanged");
        rejects([&] { convert(raster(base, 2)); }, "low resolution");
        rejects([&] { convert(raster(base, 5, 4, 120, 150)); }, "low contrast");
        rejects([&] { convert(Raster{100, 100, std::vector<uint8_t>(10000, 255)}); }, "non QR");
        auto damaged = base;
        damaged.dark[size_t(base.size - 1) * base.size + base.size - 1] ^= 1;
        auto correctable = raster(damaged);
        auto corrected = ZXing::ReadBarcode(ZXing::ImageView(correctable.gray.data(), correctable.width, correctable.height, ZXing::ImageFormat::Lum),
            ZXing::ReaderOptions().formats(ZXing::BarcodeFormat::QRCodeModel2));
        require(corrected.isValid() && std::vector<uint8_t>(corrected.bytes().begin(), corrected.bytes().end()) == payload,
            "damaged test code is still decodable through correction");
        rejects([&] { convert(raster(damaged)); }, "correctable data damage");
        damaged = base; damaged.dark[8] ^= 1;
        rejects([&] { convert(raster(damaged)); }, "correctable format damage");
        damaged = base; damaged.dark[size_t(6) * base.size + 10] ^= 1;
        rejects([&] { convert(raster(damaged)); }, "timing damage");
        damaged = reference(7, 2, 5); damaged.dark[damaged.size - 11] ^= 1;
        rejects([&] { convert(raster(damaged)); }, "version information damage");
        damaged = reference(2, 2, 5); damaged.dark[size_t(16) * damaged.size] ^= 1;
        rejects([&] { convert(raster(damaged)); }, "remainder bit damage");
        auto blurred = raster(base, 8);
        int mid = (4 + 16) * 8;
        for (int y = mid; y < mid + 8; ++y) for (int x = mid; x < mid + 8; ++x)
            blurred.gray[size_t(y) * blurred.width + x] = 128;
        rejects([&] { convert(blurred); }, "ambiguous module");
        auto rounded = raster(base, 16);
        for (int y = 4 * 16; y < (4 + base.size) * 16; ++y) for (int x = 4 * 16; x < (4 + base.size) * 16; ++x) {
            double dx = (x % 16 + .5) - 8, dy = (y % 16 + .5) - 8;
            if (dx * dx + dy * dy > 64) rounded.gray[size_t(y) * rounded.width + x] = 255;
        }
        rejects([&] { convert(rounded); }, "circular dots");
        auto inverted = normal;
        for (auto& v : inverted.gray) v = 255 - v;
        rejects([&] { convert(inverted); }, "inverted code");
        auto stretched = normal;
        stretched.width += 20; stretched.gray.assign(size_t(stretched.width) * stretched.height, 255);
        for (int y = 0; y < stretched.height; ++y) for (int x = 0; x < stretched.width; ++x)
            stretched.gray[size_t(y) * stretched.width + x] = normal.at(x * normal.width / stretched.width, y);
        rejects([&] { convert(stretched); }, "stretched image");
        auto mirrored = normal;
        for (int y = 0; y < mirrored.height; ++y)
            std::reverse(mirrored.gray.begin() + size_t(y) * mirrored.width, mirrored.gray.begin() + size_t(y + 1) * mirrored.width);
        rejects([&] { convert(mirrored); }, "mirrored code");
        auto two = normal;
        two.width = normal.width * 2 + 20;
        two.gray.assign(size_t(two.width) * two.height, 255);
        for (int y = 0; y < normal.height; ++y) for (int x = 0; x < normal.width; ++x) {
            two.gray[size_t(y) * two.width + x] = normal.at(x, y);
            two.gray[size_t(y) * two.width + x + normal.width + 20] = normal.at(x, y);
        }
        rejects([&] { convert(two); }, "multiple QR codes");
        std::ofstream("build/fixtures/not-image.png") << "not an image";
        rejects([&] { loadImage("build/fixtures/not-image.png"); }, "invalid image file");
        // Reference assets are encoded only by the test helper; the shipping CLI never encodes a QR.
        std::string url = "https://example.com";
        auto demoCode = qrcodegen::QrCode::encodeSegments({qrcodegen::QrSegment::makeBytes({url.begin(), url.end()})},
            qrcodegen::QrCode::Ecc::MEDIUM, 3, 3, 4, false);
        Grid demo{demoCode.getSize(), std::vector<uint8_t>(size_t(demoCode.getSize()) * demoCode.getSize())};
        for (int y = 0; y < demo.size; ++y) for (int x = 0; x < demo.size; ++x)
            demo.dark[size_t(y) * demo.size + x] = demoCode.getModule(x, y);
        saveImage(raster(demo, 17), "build/fixtures/example.jpg", true);
        saveImage(raster(demo, 17), "build/fixtures/example.png", false);
        auto demoResult = convert(loadImage("build/fixtures/example.jpg"));
        require(demoResult.grid == demo && demoResult.payload == std::vector<uint8_t>(url.begin(), url.end()), "example URL and grid preserved");
        writeSVG("build/fixtures/example.svg", demoResult.svg, true);
        auto pngResult = convert(loadImage("build/fixtures/example.png"));
        require(pngResult.grid == demo && pngResult.svg == demoResult.svg, "PNG and JPEG example conversions match");
        std::cout << "Passed exact SVG fill and payload checks, rotation, PNG/JPEG, alpha, EXIF, artifacts, margins, rejection and output safety ("
            << checks << " checks).\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n'; return 1;
    }
}
