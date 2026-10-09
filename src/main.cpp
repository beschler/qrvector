// Copyright (c) 2026 Zachary Beschler
// SPDX-License-Identifier: MIT
#include "qrvector.hpp"
#include <charconv>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
const char* help = R"(Usage: qrvector [OPTIONS] INPUT
       qrvector --help | --version

Arguments:
  INPUT             Required for conversion: one PNG or JPEG file.

Options (all optional):

  -o, --output PATH  Output file (default: input filename with .svg extension)
                    Use - to write SVG to standard output.
  -f, --force        Replace an existing output file
  -m, --margin N     Exact transparent margin per side, in modules (0–64;
                    default: 2). Use 4 for the standard QR quiet zone.
  -h, --help         Show this help
      --version      Show version
      --             End option parsing (for input names beginning with -)

Accepts one aligned, square-module Model 2 QR code in a PNG or JPEG.
Requires at least 3 pixels per module. Never repairs or re-encodes a QR.
Exports black squares on transparency. Margins below four modules reduce
scanner compatibility. Place the SVG on a light, uniform background.

No input from stdin, URL fetching, batch mode, or environment configuration.
Exit codes: 0 = success, 1 = conversion/I/O error, 2 = argument error.
)";
void printHelp() {
    std::cout << "qrvector " << qrvector::AppVersion << " — preserve a raster QR code's modules as a compact SVG\n\n" << help;
}
}

int main(int argc, char** argv) {
    std::filesystem::path input, output;
    bool force = false, positional = false;
    int margin = qrvector::DefaultMargin;
    try {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (!positional && (arg == "-h" || arg == "--help")) { printHelp(); return 0; }
            if (!positional && arg == "--version") { std::cout << "qrvector " << qrvector::AppVersion << '\n'; return 0; }
            if (!positional && arg == "--") { positional = true; continue; }
            if (!positional && (arg == "-f" || arg == "--force")) { force = true; continue; }
            if (!positional && (arg == "-m" || arg == "--margin")) {
                if (++i == argc) throw std::invalid_argument("specify a module count after --margin");
                std::string value = argv[i];
                auto parsed = std::from_chars(value.data(), value.data() + value.size(), margin);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || margin < 0 || margin > qrvector::MaxMargin)
                    throw std::invalid_argument("margin must be an integer from 0 to 64 modules");
                continue;
            }
            if (!positional && (arg == "-o" || arg == "--output")) {
                if (++i == argc || !output.empty()) throw std::invalid_argument("specify one output path after -o");
                output = argv[i]; continue;
            }
            if (!positional && arg.starts_with('-')) throw std::invalid_argument("unknown option: " + arg);
            if (!input.empty()) throw std::invalid_argument("specify exactly one input image");
            input = arg;
        }
        if (input.empty()) throw std::invalid_argument("specify an input image (see --help)");
        if (output.empty()) { output = input; output.replace_extension(".svg"); }
        if (output != "-" && std::filesystem::weakly_canonical(input) == std::filesystem::weakly_canonical(output))
            throw std::invalid_argument("output must differ from input");
        if (output != "-" && std::filesystem::exists(output) && std::filesystem::equivalent(input, output))
            throw std::invalid_argument("output must differ from input");
    } catch (const std::exception& e) {
        std::cerr << "qrvector: " << e.what() << '\n'; return 2;
    }
    try {
        auto sourceBytes = std::filesystem::file_size(input);
        auto result = qrvector::convert(qrvector::loadImage(input), margin);
        if (output == "-") { std::cout << result.svg; if (!std::cout) throw std::runtime_error("cannot write standard output"); }
        else qrvector::writeSVG(output, result.svg, force);
        double reduction = 100 * (1 - double(result.svg.size()) / sourceBytes);
        std::cerr << "QR version " << result.version << ", " << result.grid.size << 'x' << result.grid.size
            << " modules, mask " << result.mask << ", ECC " << result.ecLevel << ", margin " << margin << '\n'
            << sourceBytes << " → " << result.svg.size() << " bytes (" << std::fixed << std::setprecision(1)
            << std::abs(reduction) << (reduction >= 0 ? "% smaller) → " : "% larger) → ") << output.string() << '\n';
        if (result.addedMargin) std::cerr << "Added transparent padding to provide a " << margin << "-module margin.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "qrvector: " << e.what() << '\n'; return 1;
    }
}
