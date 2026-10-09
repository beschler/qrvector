// Copyright (c) 2026 Zachary Beschler
// SPDX-License-Identifier: MIT
#include "qrvector.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

namespace qrvector {
namespace {
std::string number(double value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(6) << value;
    auto s = out.str();
    while (s.back() == '0') s.pop_back();
    if (s.back() == '.') s.pop_back();
    return s;
}

struct Point { int x, y; bool operator==(const Point&) const = default; };
std::string start(Point p, Point previous) {
    auto absolute = "M" + std::to_string(p.x) + " " + std::to_string(p.y);
    auto relative = "m" + std::to_string(p.x - previous.x) + " " + std::to_string(p.y - previous.y);
    return absolute.size() <= relative.size() ? absolute : relative;
}

std::string rectangles(const Grid& grid) {
    struct Rect { int x, y, w, h; };
    std::vector<Rect> rects;
    std::map<std::pair<int, int>, size_t> previous;
    for (int y = 0; y < grid.size; ++y) {
        std::map<std::pair<int, int>, size_t> current;
        for (int x = 0; x < grid.size;) {
            if (!grid.at(x, y)) { ++x; continue; }
            int begin = x;
            while (x < grid.size && grid.at(x, y)) ++x;
            auto key = std::pair{begin, x - begin};
            auto found = previous.find(key);
            size_t index;
            if (found != previous.end()) { index = found->second; ++rects[index].h; }
            else { index = rects.size(); rects.push_back({begin, y, x - begin, 1}); }
            current[key] = index;
        }
        previous = std::move(current);
    }
    std::string path;
    Point previousPoint{0, 0};
    for (auto r : rects) {
        Point p{r.x, r.y};
        path += start(p, previousPoint) + "h" + std::to_string(r.w) + "v" + std::to_string(r.h) +
            "h" + std::to_string(-r.w) + "z";
        previousPoint = p;
    }
    return path;
}

std::string contours(const Grid& grid) {
    // Trace exact integer boundaries with black on the right. Opposite winding keeps holes clear.
    struct Edge { Point a, b; int direction; bool used = false; };
    std::vector<Edge> edges;
    int stride = grid.size + 1;
    std::vector<std::array<int, 4>> outgoing(size_t(stride) * stride, {-1, -1, -1, -1});
    auto add = [&](Point a, Point b, int direction) {
        outgoing[size_t(a.y) * stride + a.x][direction] = int(edges.size());
        edges.push_back({a, b, direction});
    };
    auto black = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < grid.size && y < grid.size && grid.at(x, y);
    };
    for (int y = 0; y < grid.size; ++y) for (int x = 0; x < grid.size; ++x) if (grid.at(x, y)) {
        if (!black(x, y - 1)) add({x, y}, {x + 1, y}, 0);
        if (!black(x + 1, y)) add({x + 1, y}, {x + 1, y + 1}, 1);
        if (!black(x, y + 1)) add({x + 1, y + 1}, {x, y + 1}, 2);
        if (!black(x - 1, y)) add({x, y + 1}, {x, y}, 3);
    }
    std::string path;
    Point previous{0, 0};
    for (size_t first = 0; first < edges.size(); ++first) {
        if (edges[first].used) continue;
        std::vector<Point> loop;
        int index = int(first);
        do {
            auto& edge = edges[index];
            if (edge.used) throw std::runtime_error("invalid SVG contour");
            edge.used = true;
            loop.push_back(edge.a);
            const auto& next = outgoing[size_t(edge.b.y) * stride + edge.b.x];
            index = -1;
            // At diagonal contacts, turn right to keep the two squares separate.
            for (int turn : {1, 0, 3, 2}) {
                int candidate = next[(edge.direction + turn) % 4];
                if (candidate >= 0 && (!edges[candidate].used || candidate == int(first))) {
                    index = candidate; break;
                }
            }
            if (index < 0) throw std::runtime_error("open SVG contour");
        } while (index != int(first));
        std::vector<Point> corners;
        for (size_t i = 0; i < loop.size(); ++i) {
            auto a = loop[(i + loop.size() - 1) % loop.size()], b = loop[i], c = loop[(i + 1) % loop.size()];
            if ((a.x == b.x && b.x == c.x) || (a.y == b.y && b.y == c.y)) continue;
            corners.push_back(b);
        }
        path += start(corners.front(), previous);
        for (size_t i = 1; i < corners.size(); ++i) {
            auto a = corners[i - 1], b = corners[i];
            path += a.x == b.x ? "v" + std::to_string(b.y - a.y) : "h" + std::to_string(b.x - a.x);
        }
        path += 'z';
        previous = corners.front();
    }
    return path;
}
} // namespace

std::string makeSVG(const Result& result) {
    // Both candidates represent the exact union of black squares; choose the shorter byte string.
    auto path = rectangles(result.grid), outline = contours(result.grid);
    if (outline.size() < path.size()) path = std::move(outline);
    double width = result.left + result.grid.size + result.right;
    double height = result.top + result.grid.size + result.bottom;
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + number(width * result.modulePixels) +
        "\" height=\"" + number(height * result.modulePixels) + "\" viewBox=\"0 0 " + number(width) + " " + number(height) +
        "\"><path fill=\"#000\" transform=\"translate(" + number(result.left) + " " + number(result.top) +
        ")\" d=\"" + path + "\"/></svg>\n";
}

void writeSVG(const std::filesystem::path& path, const std::string& svg, bool overwrite) {
    auto pattern = (path.parent_path() / ("." + path.filename().string() + ".XXXXXX")).string();
    std::vector<char> name(pattern.begin(), pattern.end()); name.push_back('\0');
    int fd = mkstemp(name.data());
    if (fd < 0) throw std::runtime_error("cannot create output: " + std::string(std::strerror(errno)));
    struct Cleanup {
        int fd; const char* name;
        ~Cleanup() { if (fd >= 0) close(fd); unlink(name); }
    } cleanup{fd, name.data()};
    size_t offset = 0;
    while (offset < svg.size()) {
        auto count = write(fd, svg.data() + offset, svg.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) throw std::runtime_error("cannot write output: " + std::string(std::strerror(errno)));
        offset += size_t(count);
    }
    if (fsync(fd) != 0) throw std::runtime_error("cannot flush output");
    if (close(fd) != 0) { cleanup.fd = -1; throw std::runtime_error("cannot close output"); }
    cleanup.fd = -1;
    // An exclusive hard link avoids an exists-check race; rename atomically replaces when requested.
    int status = overwrite ? rename(name.data(), path.c_str()) : link(name.data(), path.c_str());
    if (status != 0) {
        if (errno == EEXIST) throw std::runtime_error("output already exists; use --force to replace it");
        throw std::runtime_error("cannot save output: " + std::string(std::strerror(errno)));
    }
}
} // namespace qrvector
