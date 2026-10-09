# Preparing a release

The source tree and locally built package are ready for review; creating a public GitHub repository, release, or signing identity is a separate publishing action.

## Validate the source

1. Update `AppVersion` in `src/qrvector.hpp`, move release notes from **Unreleased** in `CHANGELOG.md`, and keep documented defaults aligned with `--help`.
2. Build and check on an Apple Silicon Mac:

   ```sh
   make -j4 test
   make examples
   make audit
   make package
   ```

3. Confirm the package contains `qrvector`, the MIT license, all dependency license texts/notices, current documentation, and both margin variants. Extract the ZIP into a fresh directory and run `./qrvector examples/example.jpg -o output.svg` there.
4. Verify the stated minimum macOS version on an actual machine or CI environment before expanding compatibility claims. The current deployment target is macOS 13; local checks have run on Apple Silicon/macOS 26.7.

## Publish on GitHub

- Publish source, tests, examples, documentation, and bundled dependency source/licenses. The generated root executable, `build/`, `dist/`, and `.DS_Store` are ignored by Git.
- Add a concise repository description and relevant topics such as `qr-code`, `svg`, `cli`, and `macos`.
- Create a version tag and a release with the matching changelog entry.
- Upload `dist/qrvector-macos-arm64.zip` as a binary asset. GitHub's automatic source archive does not contain the ignored compiled executable.
- Provide a SHA-256 checksum for the binary archive: `shasum -a 256 dist/qrvector-macos-arm64.zip`.

The GitHub repository URL and release links are intentionally not hard-coded before the repository exists. Add real download/issue links and any CI badge only after their destinations are available.

## Recommended follow-up work

- Add an Apple Silicon CI job that builds and runs `make test`; add a status badge only after the workflow runs successfully.
- Consider testing actual camera/scanner workflows and older supported macOS versions. Clean-image decoding with ZXing does not establish broad scanner compatibility for the two-module default.
- For consumer downloads, decide whether to distribute a Developer ID signed and notarized binary. The current package is a local build, not a notarized distribution; downloaded executables can be subject to macOS security checks. Building from reviewed source remains the documented source-install path. See [Apple's distribution/security guidance](https://support.apple.com/en-us/102445).
- A Homebrew formula can be added after a stable release URL and checksum exist. No formula or package-registry entry currently exists, so documentation should not advertise `brew install qrvector`.

These are publication recommendations, not claims that CI, notarization, or a package registry is already configured.
