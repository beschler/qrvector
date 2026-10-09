# Contributing to qrvector

Bug reports, documentation improvements, and focused pull requests are welcome. Contributions to qrvector's own code and documentation are made under its [MIT License](LICENSE). Third-party files keep their original licenses.

## Development setup

Use an Apple Silicon Mac with Apple's C++20 compiler tools and macOS SDK installed.

```sh
make -j4
make -j4 test
```

The executable is `./qrvector`; the default build stays inside the checkout. See [developer documentation](docs/DEVELOPMENT.md) for configuration and architecture.

## Changes

- Preserve the observed module pattern. Do not add a decode-and-re-encode fallback or silently repair damaged modules.
- Keep transparent output, exact black square geometry, and deterministic SVG serialization.
- Update `--help`, the README option/default tables, code comments, and relevant examples when behavior changes.
- Add focused tests when the change affects grid recovery, SVG geometry, validation, command parsing, or output safety. Documentation-only edits need command/link checks rather than unrelated new tests.
- Keep vendored source unchanged unless intentionally updating a dependency; record versions, provenance, and notices.
- Run `make -j4 test`. Run `make examples` if output changes and `make audit` if dependencies or linking change.

## Bug reports

Include `./qrvector --version`, macOS version and architecture, the command used, stderr output, and expected behavior. Attach a reproducible image only when you are comfortable sharing its encoded content. A QR image can contain private data even though it looks abstract. Prefer an example using a public test payload such as `https://example.com`.

Use the repository's issue tracker once it is published. See [release preparation](docs/RELEASING.md) for maintainer checks before publishing assets.
