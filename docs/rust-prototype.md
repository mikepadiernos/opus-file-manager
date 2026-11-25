# Rust query-parser prototype

M21 evaluated Rust for self-contained core logic by reimplementing the
advanced search query parser ([#242]) behind a narrow C ABI. This document
records what was built, what the boundary tests found and the recommendation
for later milestones. The C++ parser in `src/core/searchquery.cpp` remains the
only parser the application uses.

## What was built

- `rust/searchquery/`: the `opus_searchquery` crate, with no dependencies.
  `src/parser.rs` mirrors the C++ tokenizer, parser, term validation and
  normalized output; `src/lib.rs` exposes the C ABI.
- `rust/searchquery/include/opus_searchquery.h`: the C declarations and a
  header-only C++ wrapper, `OpusSearchQuery::check()`, that takes a `QString`
  or UTF-8 bytes and returns the status and a `QString`.
- `OPUSFM_RUST_PROTOTYPE`: a CMake option, off by default. When on, CMake
  imports the crate with [Corrosion] and adds two tests:
  - `opusfm-rustsearchquerytest` compares the C++ and Rust parsers.
  - `opusfm-rust-searchquery-cargo` runs the crate's own `cargo test`.

The ABI is two functions:

```c
int32_t opus_query_check(const uint8_t *text, size_t len, uint8_t **out, size_t *out_len);
void opus_query_free(uint8_t *ptr, size_t len);
```

`opus_query_check()` returns `OPUS_QUERY_VALID` with the normalized query, or
`OPUS_QUERY_INVALID` with an English error message. It returns
`OPUS_QUERY_BAD_ENCODING`, `OPUS_QUERY_BAD_ARGUMENT` or
`OPUS_QUERY_INTERNAL_ERROR` without output.

To build and test it in the dev container:

```bash
tools/dev-container/run.sh sh -c 'cmake -S . -B build-container -G Ninja -DCMAKE_BUILD_TYPE=Debug -DOPUSFM_RUST_PROTOTYPE=ON && cmake --build build-container'
tools/dev-container/run.sh ctest --test-dir build-container -R rust --output-on-failure
```

## Equivalence

The comparison test checks that both parsers give the same normalized query
or the same English error for:

- the valid and invalid queries from `searchquerytest`, plus sizes, dates,
  leap years, non-ASCII words and quoted values with newlines;
- 3,000 queries generated from operators, parentheses, every field and
  comparison, and invalid values, sometimes without spaces between them;
- embedded NUL characters and unpaired UTF-16 surrogates.

All of them match. Two differences are known and asserted in
`documentsKnownDivergences()`:

- **MIME types:** Rust has no MIME database, so it accepts any well-formed
  MIME type, such as `type:nothing/here`, which C++ rejects. Production use
  would need a C++ check after parsing or a callback into `QMimeDatabase`.
- **Letters:** C++ uses `QChar::isLetter()` on UTF-16 code units; Rust uses
  the Unicode Alphabetic property on code points. Alphabetic includes letter
  numbers and some combining marks, so `Ⅻ:x` is an unknown field in Rust but
  a plain word in C++. The Rust parser also treats characters outside the
  Basic Multilingual Plane as non-letters to match C++, which sees them as
  surrogate pairs.

## Boundary findings

- **Ownership:** Rust allocates output and must free it, so every result
  crosses back through `opus_query_free()`. The C++ wrapper copies the bytes
  into a `QString` and releases the buffer through a `std::unique_ptr`
  deleter, so callers never handle raw buffers.
- **Errors and panics:** parsing runs under `catch_unwind`, and a panic
  becomes `OPUS_QUERY_INTERNAL_ERROR` instead of unwinding into C++, which
  would be undefined behaviour. Null pointers and invalid UTF-8 return status
  codes. Both sides have tests for these cases.
- **Text encoding:** `QString::toUtf8()` silently drops unpaired surrogates,
  which can change a query's meaning; for example, `name:"\uDC00"` becomes
  `name:""`, which is an error. The wrapper replaces them with U+FFFD before
  encoding. Like an unpaired surrogate, U+FFFD is neither a letter nor a
  space, so tokenization is unchanged. Any future boundary that passes
  `QString` data needs the same care.
- **Paths:** queries treat paths as Unicode text, which suits parsing.
  Matching against real file names, which can be arbitrary bytes on Linux,
  stays in C++ with `QFileInfo` and KIO. A Rust component that handles file
  names would need a byte-oriented ABI rather than UTF-8 strings.
- **Localization:** Rust messages are English only. KI18n has no Rust
  binding, so production use would need error codes and arguments that C++
  turns into `i18nc()` messages, which doubles the error catalogue.
- **Threading:** the parser is stateless and reentrant. The test runs 2,000
  queries through `QtConcurrent::blockingMapped()` and gets the same results
  as running them in sequence.

## Costs

Measured in the dev container (Fedora 44, aarch64, rustc 1.100, Corrosion
0.5.2):

| Item           | Cost                                                                                                                                    |
| -------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| Toolchain      | `rust`, `cargo`, `rustfmt`, `clippy` and `corrosion` packages in the dev image; distributions need the same to build with the option on |
| Crate build    | under a second from clean, debug or release; no third-party crates                                                                      |
| Static library | about 45 MB, almost all of it the Rust standard library, which the linker mostly discards                                               |
| Linked size    | the debug comparison test is 1.3 MB stripped, against 0.2 MB for `searchquerytest`; about 0.6 MB is Rust code                           |
| Source         | 650 lines of Rust for the parser and 150 for the ABI, against 670 lines of C++ for the parser and evaluator                             |
| Tooling        | `cargo fmt` and `cargo clippy -D warnings` alongside clang-format; a second test runner under ctest                                     |

The larger maintenance cost is keeping two implementations equivalent:
every change to the query syntax or its messages must be made twice and kept
in step by the comparison test.

## Recommendation

Keep the C++ parser and don't adopt Rust broadly yet. The boundary was easy to
make safe, and the build cost is small with distribution packages. The parser,
however, gains little from Rust: it is small, already covered by tests, and
depends on Qt for MIME types, Unicode properties, dates and localization, so a
Rust version needs callbacks or post-checks for each.

Reconsider Rust for components that:

- are CPU-bound or parse untrusted data, and are profiled as hotspots;
- need few Qt or KDE facilities, so the ABI stays small;
- can work on bytes rather than `QString`, or define a single, tested
  conversion at the boundary;
- report errors as codes, so C++ owns localized messages.

The tagging work in M23 and the performance work in later milestones should
apply these criteria. The `OPUSFM_RUST_PROTOTYPE` option and its tests remain
as a reference for the boundary patterns: status codes, Rust-owned buffers
with an explicit free function, panic containment, surrogate-safe encoding
and comparison testing.

## M23 tagging evaluation

M23 ([#250]) applied these criteria to the three candidate areas for native
tagging. The decision is to implement the tagging core in C++, in
`src/core/`, and to keep Rust limited to the M21 prototype.

| Area                                                                              | CPU-bound or untrusted input                                                                        | Qt or KDE needs                                                                 | Text and errors                                                                                                                                                               | Fit  |
| --------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---- |
| Tag relationships: identities, aliases, parents, cycle checks, inherited matching | No; a library holds hundreds to a few thousand tags, and ancestor sets are computed once per search | Colours as `QColor` names, localized names in the UI                            | Duplicate-name and cycle errors are user-visible, so they need codes and a second catalogue                                                                                   | Poor |
| Library metadata: the open JSON format, entries, fields, export and restore       | No; loading and saving are dominated by disk I/O                                                    | `QJsonDocument`, `QSaveFile` for atomic writes, `QStandardPaths` test isolation | Paths are arbitrary bytes on Linux, so the ABI would need byte strings; a JSON parser in Rust means a third-party crate such as `serde_json`, which distributions must vendor | Poor |
| Search logic: tag, untagged and metadata terms in advanced queries                | Matching is per entry and cheap; refresh and search are I/O-bound                                   | `QFileInfo`, `QMimeDatabase`, and `KFileMetaData` for EXIF and ID3              | The query parser stays in C++ after M21, so tag terms extend it                                                                                                               | Poor |

None of the areas is a profiled hotspot, and each depends on Qt or KDE for
most of its work. Moving them to Rust would add callbacks into Qt, a
byte-oriented path ABI and a second error catalogue, while the C++ versions
reuse existing tested facilities. A library refresh walks folders and calls
`stat()`, so its cost is the file system, not the language.

Tag search terms extend the C++ query parser, so the Rust prototype must
mirror them to keep its comparison test passing. That is the dual-maintenance
cost M21 identified, and another reason not to split the search logic.

Revisit this decision if profiling shows that loading, matching or saving very
large libraries, with hundreds of thousands of entries, is CPU-bound. Because
the [library format](tagging.md#library-format) is open and documented, a
later implementation in any language can read and write it.

[#242]: https://github.com/mikepadiernos/plasma-opus/issues/242
[#250]: https://github.com/mikepadiernos/plasma-opus/issues/250
[Corrosion]: https://github.com/corrosion-rs/corrosion
