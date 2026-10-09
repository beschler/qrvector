// Copyright (c) 2026 Zachary Beschler
// SPDX-License-Identifier: MIT
#include "qrvector.hpp"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>

namespace qrvector {
namespace {
template <typename T> class OwnedCF {
    T value;
public:
    explicit OwnedCF(T v) : value(v) {}
    ~OwnedCF() { if (value) CFRelease(value); }
    OwnedCF(const OwnedCF&) = delete;
    operator T() const { return value; }
    explicit operator bool() const { return value != nullptr; }
};

int property(CFDictionaryRef dict, CFStringRef key, int fallback) {
    if (!dict) return fallback;
    auto value = static_cast<CFNumberRef>(CFDictionaryGetValue(dict, key));
    int result = fallback;
    if (value && CFGetTypeID(value) == CFNumberGetTypeID())
        CFNumberGetValue(value, kCFNumberIntType, &result);
    return result;
}

Raster orient(Raster input, int orientation) {
    if (orientation == 1) return input;
    if (orientation < 1 || orientation > 8)
        throw std::runtime_error("unsupported image orientation metadata");
    Raster out;
    out.width = orientation >= 5 ? input.height : input.width;
    out.height = orientation >= 5 ? input.width : input.height;
    out.gray.resize(input.gray.size());
    for (int y = 0; y < input.height; ++y) for (int x = 0; x < input.width; ++x) {
        int ox = x, oy = y;
        switch (orientation) {
        case 2: ox = input.width - 1 - x; break;
        case 3: ox = input.width - 1 - x; oy = input.height - 1 - y; break;
        case 4: oy = input.height - 1 - y; break;
        case 5: ox = y; oy = x; break;
        case 6: ox = input.height - 1 - y; oy = x; break;
        case 7: ox = input.height - 1 - y; oy = input.width - 1 - x; break;
        case 8: ox = y; oy = input.width - 1 - x; break;
        }
        out.gray[size_t(oy) * out.width + ox] = input.at(x, y);
    }
    return out;
}
} // namespace

Raster loadImage(const std::filesystem::path& path) {
    auto bytes = path.string();
    OwnedCF url(CFURLCreateFromFileSystemRepresentation(nullptr,
        reinterpret_cast<const UInt8*>(bytes.data()), bytes.size(), false));
    if (!url) throw std::runtime_error("cannot open input path");
    OwnedCF source(CGImageSourceCreateWithURL(url, nullptr));
    if (!source) throw std::runtime_error("cannot read image: " + path.string());
    auto type = CGImageSourceGetType(source);
    if (!type || (!CFEqual(type, CFSTR("public.png")) && !CFEqual(type, CFSTR("public.jpeg"))))
        throw std::runtime_error("input must be a PNG or JPEG image");
    if (CGImageSourceGetCount(source) != 1)
        throw std::runtime_error("animated or multi-image inputs are not supported");
    OwnedCF props(CGImageSourceCopyPropertiesAtIndex(source, 0, nullptr));
    int width = property(props, kCGImagePropertyPixelWidth, 0);
    int height = property(props, kCGImagePropertyPixelHeight, 0);
    // Bound decoded image buffers before allocating them: 40 MP and 20,000 px per axis.
    constexpr uint64_t maxPixels = 40000000;
    if (width <= 0 || height <= 0 || uint64_t(width) * height > maxPixels || width > 20000 || height > 20000)
        throw std::runtime_error("invalid image dimensions or image exceeds the 40-megapixel limit");
    OwnedCF image(CGImageSourceCreateImageAtIndex(source, 0, nullptr));
    if (!image) throw std::runtime_error("cannot decode input image");
    std::vector<uint8_t> rgba(size_t(width) * height * 4);
    OwnedCF color(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    OwnedCF context(CGBitmapContextCreate(rgba.data(), width, height, 8, size_t(width) * 4, color,
        uint32_t(kCGImageAlphaPremultipliedLast) | uint32_t(kCGBitmapByteOrder32Big)));
    if (!context) throw std::runtime_error("cannot allocate image buffer");
    // Composite transparency onto white only for analysis; output remains transparent.
    CGContextSetRGBFillColor(context, 1, 1, 1, 1);
    CGContextFillRect(context, CGRectMake(0, 0, width, height));
    CGContextSetInterpolationQuality(context, kCGInterpolationNone);
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
    Raster result{width, height, std::vector<uint8_t>(size_t(width) * height)};
    for (size_t i = 0; i < result.gray.size(); ++i)
        result.gray[i] = (299 * rgba[i * 4] + 587 * rgba[i * 4 + 1] + 114 * rgba[i * 4 + 2] + 500) / 1000;
    return orient(std::move(result), property(props, kCGImagePropertyOrientation, 1));
}
} // namespace qrvector
