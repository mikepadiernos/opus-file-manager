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

## Libraries

Libraries are opt-in. **Tags › Create Library from Folder…** asks for
confirmation and then registers the active folder as a library root:

- Registered roots are stored in the `Tag Libraries` group of the Plasma Opus
  configuration. `LibraryRegistry` in `src/core/libraryregistry.h` manages
  them.
- When the folder has no library document, an empty one is created. A valid
  existing document is adopted, so tags survive removing and creating a
  library again; an unreadable one is reported and left unchanged.
- Libraries can't be nested: a folder inside a library, or a folder that
  contains one, can't become a library.
- Creating a library runs a first refresh, described below.

**Tags › Remove Library…** unregisters the library that contains the active
folder after confirmation. Its document is kept and no files are deleted.

### Indexing

A refresh walks the library root in the background with progress and can be
canceled. `LibraryScan` in `src/core/libraryscan.h` implements it. Refreshing
only reads the file system: nothing is moved, renamed, copied or written
except the library document.

- Files and folders of every type are indexed, including hidden ones.
- Symbolic links are not indexed, and linked folders are not followed, so a
  library never reaches outside its root or loops.
- `.mfm.json` files and the library document are skipped.
- Names that aren't valid in the system encoding are skipped and counted in
  the summary, because they can't be stored as UTF-8 paths.
- New paths become entries, and known paths have their size, modification
  time and identity hints updated.
- Entries that a complete refresh doesn't find are marked `missing` and keep
  their tags and fields. A canceled refresh, or one where some folders
  couldn't be read, records what it found but marks nothing missing.
- A missing entry found again at its recorded path is no longer missing.

The refresh summary offers **Review Missing Entries…** when entries are
missing. **Tags › Refresh Library** and **Tags › Review Missing Entries…**
act on the library containing the active folder.

### Moves and renames in Plasma Opus

Moves and renames that Plasma Opus starts keep their tags and fields: moving
or cutting and pasting, dropping, renaming in the rename dialog, renaming
inline in the cards view, and batch renaming. `LibraryMoveTracker` follows
them and `LibraryMoves` in `src/core/librarymoves.h` applies them.

- A move is applied only once it is confirmed on disk: the destination has
  the renamed item's device and inode, or, for KIO moves, the source is gone
  and the destination exists. Skipped, failed and canceled items aren't
  applied.
- Entries below a moved folder move with it, and swapped names are applied
  together.
- An entry at an overwritten destination is replaced. When a folder is merged
  into an existing folder, neither folder's tags are replaced: the moved
  folder's entry is kept as missing if both have tags or fields.
- Moving an item out of a library marks its entries missing, and moving an
  item into a library indexes it without tags. Tags aren't copied between
  libraries.
- Copies, and moves and renames made by other programs, aren't followed.
  The next refresh marks the old paths missing for review.

### Reviewing missing entries

**Review Missing Entries** lists the entries the last refresh didn't find,
with their tag and field counts. For the selected entry it suggests indexed
items of the same kind that have no tags or fields and either the same device
and inode, or the same name and, for files, the same size. Suggestions are
only shown; metadata moves only when you choose one:

- **Relink to Suggestion** attaches the entry to the selected suggestion.
- **Choose Item…** attaches it to a file or folder you pick inside the
  library. Symbolic links and items outside the library are refused.
- **Forget Entry…** removes the entry and its tags and fields after
  confirmation. No files are deleted.

Relinking never overwrites an entry that has tags or fields. Relinking a
folder also relinks the missing entries inside it.

## Tags

**Tags › Manage Tags…** lists the tags of the library containing the active
folder. The list can be searched by name, alias and shorthand, and shows each
tag's colour, parents and the number of items it is assigned to. Edits are
saved to the library document straight away; a read-only library can be
browsed but not changed.

Each tag has:

- a stable identifier, so renaming a tag keeps its assignments, parents and
  group memberships;
- a name, which is required and may repeat;
- aliases, one per line, that also find the tag. Repeated aliases and aliases
  that match the tag's name are dropped;
- an optional shorthand without spaces;
- an optional colour;
- parent tags. Items with a tag also match searches for its parents and their
  parents.

### Duplicate names

Tags may share a name, ignoring case, such as _Jaguar_ the animal and _Jaguar_
the car. Plasma Opus always refers to tags by identifier, and shows a shared
name followed by the first of these that tells the tags apart: the tag's
parents, its shorthand or the start of its identifier. The tag editor shows
how a new or renamed tag will appear before it is saved.

### Hierarchy

A tag can have several parents, but the hierarchy can't contain cycles. In
the tag editor, the tag itself and the tags below it can't be chosen as
parents. Libraries written by other tools that contain cycles are repaired
when they are loaded, as described in [Reading and writing](#reading-and-writing).

Deleting a tag asks for confirmation and says how many items and child tags
it affects. The tag is removed from those items, from its child tags' parents
and from tag groups; child tags keep their other parents. No files are
changed.

[TagStudio]: https://github.com/TagStudioDev/TagStudio
