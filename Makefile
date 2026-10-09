# Copyright (c) 2026 Zachary Beschler
# SPDX-License-Identifier: MIT
# Default build stays in the project: executable at ./qrvector, intermediate files in build/.
CXX := xcrun clang++
CC := xcrun clang
CPPFLAGS := -Isrc -Iconfig -Ivendor/zxing/src -DZXING_INTERNAL
CXXFLAGS := -std=c++20 -O2 -DNDEBUG -arch arm64 -mmacosx-version-min=13.0 -Wall -Wextra -MMD -MP
CFLAGS := -O2 -arch arm64 -mmacosx-version-min=13.0 -DZUECI_EMBED_NO_TO_ECI -MMD -MP
FRAMEWORKS := -framework ImageIO -framework CoreGraphics -framework CoreFoundation
# Optional explicit installation; make/all never installs outside the project.
PREFIX ?= $(HOME)/.local
DESTDIR ?=
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share/qrvector

ZX_BASE := $(filter-out %/MultiFormatWriter.cpp %/TextEncoder.cpp %/ZXingC.cpp %/ZXingCpp.cpp,$(wildcard vendor/zxing/src/*.cpp))
ZX_QR := $(filter-out %/QREncoder.cpp %/QRMaskUtil.cpp %/QRMatrixUtil.cpp %/QRWriter.cpp,$(wildcard vendor/zxing/src/qrcode/*.cpp))
ZX_OBJ := $(patsubst %.cpp,build/%.o,$(ZX_BASE) $(ZX_QR)) build/vendor/zxing/src/libzueci/zueci.o
APP_OBJ := build/src/image.o build/src/convert.o build/src/svg.o

.PHONY: all test clean audit package examples install uninstall
all: qrvector

qrvector: $(APP_OBJ) build/src/main.o $(ZX_OBJ)
	$(CXX) $(CXXFLAGS) $^ $(FRAMEWORKS) -o $@

build/test-qrvector: $(APP_OBJ) build/tests/test.o build/tests/vendor/qrcodegen.o $(ZX_OBJ)
	$(CXX) $(CXXFLAGS) $^ $(FRAMEWORKS) -o $@

build/%.o: %.cpp config/Version.h Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/vendor/%.o: CXXFLAGS += -Wno-missing-braces

build/%.o: %.c Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test: build/test-qrvector qrvector
	./build/test-qrvector
	./tests/cli.sh

audit: qrvector
	file qrvector
	otool -L qrvector

examples: qrvector
	./qrvector examples/example.jpg -o examples/example.svg --force
	./qrvector examples/example.jpg -o examples/example-margin-4.svg --margin 4 --force

install: qrvector
	install -d "$(DESTDIR)$(BINDIR)" "$(DESTDIR)$(DATADIR)/licenses"
	install -m 755 qrvector "$(DESTDIR)$(BINDIR)/qrvector"
	install -m 644 LICENSE THIRD_PARTY_NOTICES.md "$(DESTDIR)$(DATADIR)/"
	install -m 644 vendor/zxing/LICENSE "$(DESTDIR)$(DATADIR)/licenses/ZXing-Apache-2.0.txt"
	install -m 644 vendor/zxing/src/libzueci/LICENSE "$(DESTDIR)$(DATADIR)/licenses/libzueci-BSD-3-Clause.txt"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/qrvector"
	rm -f "$(DESTDIR)$(DATADIR)/LICENSE" "$(DESTDIR)$(DATADIR)/THIRD_PARTY_NOTICES.md"
	rm -f "$(DESTDIR)$(DATADIR)/licenses/ZXing-Apache-2.0.txt" "$(DESTDIR)$(DATADIR)/licenses/libzueci-BSD-3-Clause.txt"
	@rmdir "$(DESTDIR)$(DATADIR)/licenses" "$(DESTDIR)$(DATADIR)" 2>/dev/null || true

package: qrvector
	@mkdir -p dist/qrvector-macos-arm64/licenses
	cp qrvector LICENSE README.md THIRD_PARTY_NOTICES.md CHANGELOG.md CONTRIBUTING.md dist/qrvector-macos-arm64/
	cp vendor/zxing/LICENSE dist/qrvector-macos-arm64/licenses/ZXing-Apache-2.0.txt
	cp vendor/zxing/src/libzueci/LICENSE dist/qrvector-macos-arm64/licenses/libzueci-BSD-3-Clause.txt
	cp -R examples dist/qrvector-macos-arm64/
	cp -R docs dist/qrvector-macos-arm64/
	ditto -c -k --norsrc --noextattr --keepParent dist/qrvector-macos-arm64 dist/qrvector-macos-arm64.zip

clean:
	rm -rf build
	rm -f qrvector

-include $(APP_OBJ:.o=.d) build/src/main.d $(ZX_OBJ:.o=.d) build/tests/test.d build/tests/vendor/qrcodegen.d
