# Native tagging

M23 adds opt-in tag libraries inspired by [TagStudio]. A library is rooted at
an existing local folder and stores tags, tag groups, text and date fields and
file entries in one open JSON document. Plasma Opus never moves, renames or
duplicates the files it indexes, and it doesn't need TagStudio.

The implementation is C++ in `src/core/`; see the
[Rust evaluation](rust-prototype.md#m23-tagging-evaluation).

## Library format

A library is the file `.opus-library.json` at the library root. There are no
per-file sidecars, and the format is plain UTF-8 JSON that other tools can
read and write. `TagLibrary::Library` in `src/core/taglibrary.h` implements
it.

```json
{
  "format": "plasma-opus-library",
  "version": 1,
  "tags": [
    { "id": "…", "name": "Animal", "color": "#336699" },
    {
      "id": "…",
      "name": "Cat",
      "aliases": ["Kitty"],
      "shorthand": "c",
      "parents": ["<Animal id>"]
    }
  ],
  "groups": [{ "id": "…", "name": "Subjects", "tags": ["<tag id>"] }],
  "fields": [{ "id": "…", "name": "Shot on", "type": "date" }],
  "entries": [
    {
      "id": "…",
      "path": "photos/cat.jpg",
      "kind": "file",
      "size": 12345,
      "modified": 1764072000000,
      "device": "2049",
      "inode": "1234567",
      "tags": ["<Cat id>"],
      "fields": { "<field id>": "2026-02-28" }
    }
  ]
}
```

| Key       | Meaning                                                                                                                  |
| --------- | ------------------------------------------------------------------------------------------------------------------------ |
| `format`  | Always `plasma-opus-library`.                                                                                            |
| `version` | The format version. Version 1 readers refuse higher versions rather than risk losing data.                               |
| `tags`    | Tags with a stable `id`, a `name`, optional `aliases`, `shorthand`, `color` (`#rrggbb`) and `parents` (tag identifiers). |
| `groups`  | Tag groups with a stable `id`, a `name`, an optional `color` and member `tags` in display order.                         |
| `fields`  | Field definitions with a stable `id`, a `name` and a `type` of `text` or `date`.                                         |
| `entries` | Indexed files and folders.                                                                                               |

Each entry has:

- `id`: a stable identifier that survives renames and relinking.
- `path`: the path relative to the library root, with `/` separators. It is
  never absolute and never contains `.` or `..` components.
- `kind`: `file` or `folder`.
- `size` and `modified`: the size in bytes and the modification time in
  milliseconds since the epoch, recorded at the last refresh.
- `device` and `inode`: decimal strings recorded at the last refresh. They are
  hints for relinking only; metadata is never attached to a file because its
  inode matches.
- `missing`: `true` when the last refresh didn't find the file.
- `tags` and `fields`: assigned tag identifiers, and field values keyed by
  field identifier. Date values use `YYYY-MM-DD`.

Identifiers are opaque strings; Plasma Opus creates UUIDs. Names may repeat,
so other data always refers to tags, groups and fields by identifier.

### Reading and writing

- Unknown top-level keys are preserved when Plasma Opus saves a library.
  Unknown keys inside tags, groups, fields and entries are not.
- Invalid structure, such as duplicate identifiers or paths, unsafe paths or
  wrong types, makes the whole document unreadable instead of being guessed
  at.
- Dangling references, invalid colours or dates and parent cycles are
  repaired when the library is loaded and reported as warnings; the document
  on disk changes only when the library is next saved.
- Saves are atomic: the document is written to a temporary file next to it
  and renamed into place.
- A save is refused when the document changed on disk since it was loaded,
  for example from another window. Edits are applied to a freshly loaded copy
  and retried, so concurrent edits aren't lost.

### Existing metadata

Libraries don't read, convert or replace the per-folder `.mfm.json` files that
hold labels, colours, ratings, notes, pins and view formats. Both kinds of
metadata can exist in the same folder. Refreshes skip `.mfm.json` and the
library document itself.

### Unsupported and read-only locations

`TagLibrary::checkLocation()` reports locations that can't hold a library:

- remote and other non-local locations, and relative paths;
- missing paths, files and symbolic links;
- the file system root;
- folders that can't be read.

A folder that can be read but not written, on a read-only file system or
without write permission, or a library document that can't be written, opens
read-only. Its tags can still be shown and searched, but edits are refused
with a message.

[TagStudio]: https://github.com/TagStudioDev/TagStudio
