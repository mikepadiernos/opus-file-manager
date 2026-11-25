# Native tagging

M23 adds opt-in tag libraries inspired by [TagStudio]. A library is rooted at
an existing local folder and stores tags, tag groups, text and date fields and
file entries in one open JSON document. Plasma Opus never moves, renames or
duplicates the files it indexes, and it doesn't need TagStudio.

The implementation is C++ in `src/core/`; see the
[Rust evaluation](rust-prototype.md#m23-tagging-evaluation).

## Library format

A library is one document. By default it is the file `.opus-library.json` at
the library root. A library can instead keep it privately in Plasma Opus's
data folder, so nothing is written to the library folder, which suits shared
drives and read-only folders; see [Where libraries are kept](#where-libraries-are-kept).
There are no per-file sidecars, and the format is plain UTF-8 JSON that other
tools can read and write. `TagLibrary::Library` in `src/core/taglibrary.h`
implements it.

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
| `root`    | Private documents only: the library folder the document belongs to. It is ignored when the document is read.             |

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

### Where libraries are kept

`TagLibrary::documentPath()` finds the document of a library:

- A private document is
  `~/.local/share/opus-file-manager/libraries/<hash>.json` (below
  `$XDG_DATA_HOME` when that is set). `<hash>` is the hexadecimal SHA-1 of
  the library folder's cleaned path in UTF-8. The `libraries` folder is
  created readable only by you.
- When a private document exists for a folder, it is used; otherwise the
  library is `.opus-library.json` in the folder.

**Store metadata of new libraries** in
[Preferences › Tagging › Privacy](#privacy) chooses where
**Create Library from Folder…** creates the document. An existing document is
always adopted where it is, so a library never ends up with two: a private
document first, then one in the folder. `TagLibrary::moveDocument()` moves a
document between the two places. It checks the document, writes it
atomically to the target, reads it back and only then deletes the original.
It refuses to replace an existing document and touches no other file.

A private document is keyed by the folder's path. When a library folder is
moved or renamed, its private document isn't found at the new path. Create the
library again at the new path and use **Restore Library Metadata…** with an
export or with the old private document; copies of library documents are
accepted. The old private document is left in place.

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
  and renamed into place. This applies to private documents too.
- A private document also records the library folder under `root`. The key
  is dropped when the document is read, so it never reaches exports.
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

A library whose document can't be written opens read-only. For a document in
the folder, that is a folder on a read-only file system or without write
permission, or a document that can't be written. For a private document, only
the private folder and the document count, so a folder you can read but not
write can still hold a writable private library. A read-only library's tags
can still be shown and searched, but edits are refused with a message.
Creating a library in a read-only folder is refused unless its metadata is
stored privately.

## Libraries

Libraries are opt-in. **Tags › Create Library from Folder…** asks for
confirmation and then registers the active folder as a library root:

- Registered roots are stored in the `Tag Libraries` group of the Plasma Opus
  configuration. `LibraryRegistry` in `src/core/libraryregistry.h` manages
  them.
- When the folder has no library document, an empty one is created in the
  place chosen in [Preferences › Tagging › Privacy](#privacy). A valid
  existing document is adopted where it is, so tags survive removing and
  creating a library again; an unreadable one is reported and left unchanged.
  When an existing document isn't where the setting asks, the confirmation
  and the status bar say so.
- Libraries can't be nested: a folder inside a library, or a folder that
  contains one, can't become a library.
- Creating a library runs a first refresh, described below.

**Tags › Remove Library…** unregisters the library that contains the active
folder after confirmation. Its document is kept and no files are deleted.

[Preferences › Tagging › Libraries](#libraries-1) lists every library and
can add and remove libraries or move a library's metadata between the
library folder and private storage. The Tags menu and Preferences ask the
same questions, from `LibraryConfirmations` in
`src/app/libraryconfirmations.h`.

### Indexing

A refresh walks the library root in the background with progress and can be
canceled. `LibraryScan` in `src/core/libraryscan.h` implements it. Refreshing
only reads the file system: nothing is moved, renamed, copied or written
except the library document.

- Files and folders of every type are indexed, including hidden ones unless
  [**Index hidden files and folders**](#libraries-1) is off. An item is
  hidden when any part of its path below the root starts with `.`.
- Items matching an exclusion pattern, and items deeper than the folder depth
  limit, are neither indexed nor looked into.
- A refresh indexes at most the item limit set in Preferences. A refresh that
  reaches it stops, records what it found and marks nothing missing, and its
  summary says so.
- Symbolic links are not indexed, and linked folders are not followed, so a
  library never reaches outside its root or loops.
- `.mfm.json` files and the library document are skipped.
- Names that aren't valid in the system encoding are skipped and counted in
  the summary, because they can't be stored as UTF-8 paths.
- New paths become entries, and known paths have their size, modification
  time and identity hints updated.
- Entries that a complete refresh doesn't find are marked `missing` and keep
  their tags and fields. A canceled refresh, one stopped by the item limit,
  or one where some folders couldn't be read, records what it found but marks
  nothing missing.
- Entries that the exclusions, hidden items setting or depth limit put out of
  scope are left as they are: they keep their tags and fields and aren't
  marked missing. Tagging an excluded item explicitly, or moving it in Plasma
  Opus, still indexes it.
- A missing entry found again at its recorded path is no longer missing.

The refresh summary offers **Review Missing Entries…** when entries are
missing. **Tags › Refresh Library** and **Tags › Review Missing Entries…**
act on the library containing the active folder.

Libraries can also be refreshed automatically, as set in
[Preferences › Tagging › Libraries](#libraries-1). `LibraryAutoRefresh` in
`src/app/libraryautorefresh.h` refreshes one library at a time in the
background for all windows, with the same scope and limits, and reports
added and missing entries and errors briefly in the status bar of the active
window. It records each library's last refresh in the `LibraryRefresh` group
of the state configuration, skips a library refreshed within the interval,
and gives way to a manual refresh of the same library. Refreshes in progress
are canceled when Plasma Opus quits.

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

### Export and restore

**Export Library Metadata…** and **Restore Library Metadata…**, in the Tags
menu, save a library's tags, groups, fields and entries to a file and bring
them back later. Both only read or write the export and the library document;
the files in the library are never changed, copied, moved or deleted.

The export dialog summarizes what will be exported and suggests a file such as
`Photos library 2026-11-25.json` in your Documents folder. It only replaces an
earlier export: other files, symbolic links and files named
`.opus-library.json` are refused. If the library changes while the dialog is
open, the summary is updated and you export again.

An export is plain JSON that wraps the [library document](#library-format):

```json
{
  "format": "plasma-opus-library-export",
  "version": 1,
  "exported": "2026-11-25T10:00:00Z",
  "root": "/home/user/Photos",
  "library": { "format": "plasma-opus-library", "version": 1, "…": "…" }
}
```

Restoring replaces the library document as a whole:

1. Choose an export, or a copy of a library document, and **Preview** it. The
   preview lists the tags, groups and fields that are restored, removed or
   changed, and the items whose tags or field values change, with the
   library's counts now and after restoring. Nothing is written yet.
2. Check **Replace the library's current tags, groups, fields and item
   metadata**, then **Restore**. Choosing another file clears the preview and
   the confirmation.

Items are matched by their path inside the library, so an export can be
restored into a library that has moved. Entries whose file or folder isn't at
its path any more are restored as missing, so **Review Missing Entries** can
relink them. A library document that is missing or can't be read can be
restored too.

Before writing, the previous library document is copied to the backup folder
set in [Preferences › Tagging › Privacy](#privacy). By default this is
`~/.local/share/opus-file-manager/library-backups/` (below `$XDG_DATA_HOME`
when that is set). The status bar names the copy; restore it like an export
to undo. Copies are named after the folder, the time and the first eight
characters of the library's hash, such as
`Photos 2026-11-25 100000 1a2b3c4d.json`. After a restore, the oldest copies
of that library beyond **Keep at most … backups per library** are deleted.
Only regular files named like that library's copies are considered. Copies
that record another library folder, copies made before Plasma Opus added the
hash to the name, and any other file are never deleted.

The restore is refused, without writing, when the library changed after the
preview or its document can't be written. A private library is restored in
place, so nothing is written to its folder. Exports from newer versions of
Plasma Opus are refused rather than restored partially.

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

### Tag groups

Tag groups organize how a library's tags are listed, for example _People_,
_Places_ and _Projects_. They are part of the library, so they are shared by
every window and workspace, and they don't depend on the current location or
workspace. Groups don't change what items are tagged with or what searches
match.

The **Groups** tab of **Manage Tags…** lists the groups in display order:

- **New Group…** and **Edit…** set a group's name, an optional colour and its
  tags. The tag list can be filtered by name, alias or shorthand. A tag can be
  in several groups, and group names may repeat.
- **Move Up** and **Move Down** change the order of the groups.
- **Delete…** asks for confirmation and removes only the group; its tags and
  their assignments are kept.

Groups have stable identifiers, so renaming a group or its tags keeps its
members. The **Tags** tab shows each tag's groups.

When a library has groups, the tag panel lists every group in order with its
tags below it, followed by **Other Tags** for tags that aren't in a group. A
tag in several groups appears under each, and checking any copy assigns it.
No group is hidden unless a search in the panel matches neither its name nor
any of its tags; searching for a group's name shows all of its tags. Libraries
without groups show a flat list.

## Assigning tags

**Tags › Tag Panel** shows the tag panel to the right of or below the panes;
its options menu, or dragging its title bar, moves it, and the choice is
remembered. **Assign Tags…**, in the Tags menu and the context menu of items
in a library, shows the panel and moves the keyboard focus to its search
field.

The panel lists the tags of the library that contains the selected items.
Each tag's check box shows whether it is assigned to all of them, some of
them (partly checked) or none. Checking a tag assigns it to every selected
item, and clearing it removes it from every selected item. Items that aren't
indexed yet, such as files added since the last refresh, are indexed when they
are tagged.

The panel can be used from the keyboard:

- Type in the search field to filter tags by name, alias or shorthand.
- Press <kbd>Enter</kbd> to assign or remove the tag whose name, alias or
  shorthand matches exactly, or to create a tag with the typed name and
  assign it.
- Press <kbd>Down</kbd> to move to the list, then <kbd>Space</kbd> or
  <kbd>Enter</kbd> to assign or remove the current tag.

Tags can only be assigned within one library. The panel explains why when
the selection includes items in different libraries, items outside any
library, the library folder itself, symbolic links or remote items, and shows
the current folder's tags without check boxes. With nothing selected it shows
the tags of the library containing the current folder.

The Information panel shows a **Tags** row for items in a library. It lists
the tags assigned directly; its tooltip lists the tags they inherit from
their parents.

## Fields

Fields store text or a date on items in a library alongside their tags, for
example a _Title_ or the date a photo was _Shot on_. They are kept in the
library document; files and their labels, ratings and notes aren't changed.

The **Fields** tab of **Manage Tags…** lists the library's fields with their
type and the number of items that have a value:

- **New Field…** and **Edit…** set a field's name and type, **Text** or
  **Date**. Field names must be unique, ignoring case.
- A field's type can't be changed while any item has a value for it, because
  values aren't converted between types.
- **Delete…** asks for confirmation, says how many items have a value and
  removes the field with its values.

**Edit Fields…**, in the Tags menu and the context menu of items in a
library, edits the values of the selected items. It has one row per field:

- Text fields accept any text up to 10,000 characters.
- Date fields are written as `YYYY-MM-DD`; the calendar button picks a date.
  Invalid dates, such as `2026-02-30`, are refused before anything is saved.
- When the selected items have different values, the row shows **Several
  values**. Only the rows you edit are applied, so the other values are kept.
- Clearing a row removes that value from every selected item.

Items that aren't indexed yet are indexed when a value is set. Like tags,
fields can only be edited within one library; the status bar explains why
when the selection can't be edited. When a library has no fields, the editor
offers **Manage Fields…**.

The Information panel shows a **Fields** row for items in a library that has
fields, with one `Name: value` line per value. Dates are shown in the regional
format.

## Embedded metadata

Images and audio files carry metadata of their own, such as the camera and
date of a photo or the artist and genre of a song. Plasma Opus reads it with
KDE's metadata extractors, the same ones that feed Baloo and the Information
panel:

- From images: date taken, date, camera make and model, title, description,
  subject, keywords, artist, copyright, ISO, f-number, exposure time and focal
  length.
- From audio files: title, artist, album artist, album, genre, composer, year,
  track and disc number and comment.

Reading never changes the file, and a file that can't be read is reported
rather than treated as empty.

### Importing into a library

**Import Embedded Metadata…**, in the Tags menu and the context menu of items
in a library, copies embedded properties of the selected files into the
library:

1. The files are read in the background. The dialog then lists the properties
   that were found, with example values, under **Import As**.
2. Choose **Tags** or a field for each property, or leave it at **Don't
   import**. A property is suggested for a field with the same name. Lists,
   such as keywords, become one tag per value, and dates can go to date
   fields.
3. **Preview** lists every change per item: tags to assign, tags to create,
   values to set or replace, values already imported, and values that are
   skipped because they're invalid or conflict. Nothing is written yet.
4. **Import** applies the preview. Tags with a matching name, alias or
   shorthand are reused rather than created again, and importing the same
   files twice changes nothing.

By default, a field that already has a different value keeps it, and the
preview shows it as a conflict. Check **Replace existing field values** to
overwrite them. If a value changes in the library between the preview and the
import, the newer value is kept and the status bar says so. A read-only
library can be previewed but not imported into.

The import only changes the library document. To change the metadata stored
in a file itself, use **Edit Metadata…**, or **Edit Embedded Metadata…** in the
import dialog when a single JPEG, TIFF or MP3 file is selected. It previews
the new values and writes the file only after you confirm. The import
dialog's button is disabled until you allow writing to files in
[Preferences › Tagging › Privacy](#privacy); **Tools › Edit Metadata…** isn't
affected.

### Searching embedded metadata

`meta:` criteria match the embedded properties listed above, in any library
or folder:

| Criterion    | Matches files with an embedded property that |
| ------------ | -------------------------------------------- |
| `meta:canon` | contains `canon`, ignoring case              |
| `meta=Jazz`  | is exactly `Jazz`, ignoring case             |
| `meta:20*`   | matches the wildcard pattern                 |

Each value of a list, such as one keyword, is matched on its own, and dates
are also matched as `YYYY-MM-DD`, so `meta:2024-05*` finds photos taken in
May 2024. `metadata:` is accepted for `meta:`. Metadata criteria can't use
`<`, `<=`, `>` or `>=`. Like `content:`, they read each candidate file, so
they're checked after cheaper criteria; combine them with a name or type
criterion, as in `type:image/* meta:canon`, to keep searches fast.

## Settings

**Preferences › Tagging** holds the tagging settings, which are stored in the
`Tagging` group of the Plasma Opus configuration.

### Privacy

The tab starts by explaining what Plasma Opus stores:

- A library's metadata holds its tags, aliases, shorthand, colours and groups;
  its field definitions and values; the indexed items' paths relative to the
  library folder, with size, modification time and file system identity
  numbers; and which items have which tags.
- The list of library folders is kept in the configuration, and backups are
  kept in the backup folder.
- Nothing is sent over the network.
- Files themselves only change when you write embedded metadata.

| Setting                                                        | Default               | Effect                                                                                                                                                                                |
| -------------------------------------------------------------- | --------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Store metadata of new libraries**                            | In the library folder | Where **Create Library from Folder…** creates the document of a new library: in the folder, or privately in Plasma Opus's data folder. Existing documents are adopted where they are. |
| **Backup folder**                                              | Empty                 | Where restores keep a copy of the replaced library document. Empty uses `library-backups` in Plasma Opus's data folder, shown as the placeholder.                                     |
| **Keep at most … backups per library**                         | 20                    | How many copies of each library a restore keeps, from 0 (**All**) to 1000. See [Export and restore](#export-and-restore).                                                             |
| **Allow writing library values into files' embedded metadata** | Off                   | Enables **Edit Embedded Metadata…** in the import dialog. Plasma Opus always previews the changes and asks before changing a file.                                                    |

### Libraries

The tab lists the registered libraries with their folder, where their
metadata is kept (**Library folder** or **Private**) and their state:
**Ready**, **Read-only**, or the reason the library can't be used, such as a
missing folder or an unreadable document.

- **Add Library…** picks a local folder and asks the same question as
  **Tags › Create Library from Folder…**. A new document follows **Store
  metadata of new libraries** as currently shown in the Privacy tab, and an
  existing one is adopted where it is. The new library then gets its first
  refresh in the background.
- **Remove…** unregisters the selected library after confirmation. Its
  document is kept and no files are deleted.
- **Keep Metadata Privately** or **Keep Metadata in Library Folder**, as fits
  the selected library, moves its document after confirmation. See
  [Where libraries are kept](#where-libraries-are-kept).

These changes take effect immediately, even if Preferences is canceled, and
open windows update their tag panel and Information panel.

| Setting                             | Default | Effect                                                                                                                                                                                                                             |
| ----------------------------------- | ------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Exclude**                         | Empty   | Wildcard patterns, one per entry, of items refreshes don't index or look into. `*` matches any characters and `?` one; case matters. Without `/`, a pattern matches names at any depth, such as `node_modules`, `*.tmp` or `.git`. |
| **Index hidden files and folders**  | On      | When off, refreshes skip items with any part of their path starting with `.`.                                                                                                                                                      |
| **Index at most … items**           | 0       | The most items one refresh indexes, up to 10,000,000; 0 shows **No limit**. A refresh that reaches it marks nothing missing.                                                                                                       |
| **Folder depth**                    | 0       | How many folder levels below the library folder are indexed, up to 1000; 1 indexes only its direct contents, and 0 shows **No limit**.                                                                                             |
| **Refresh libraries automatically** | Never   | **Never**, **When Plasma Opus starts**, which refreshes every library once shortly after the first window appears, or **When a folder in the library is opened** in the active pane.                                               |
| **At most once every … minutes**    | 60      | From 1 to 10,080 minutes. Libraries refreshed more recently, also by a quick restart or a manual refresh, aren't refreshed automatically. Disabled when automatic refresh is **Never**.                                            |

A pattern with `/` matches the path below the library folder, such as
`Archive/Old` or `Exports/*`; there `*` and `?` don't match `/`. A trailing
`/` matches folders only. Items inside an excluded folder are excluded too.
Scope settings apply only to refreshes: entries already indexed keep their
tags and fields, as described in [Indexing](#indexing).

## Searching by tags

Advanced queries, in **Search** and in **Select › Select by Query…**, accept
tag criteria alongside the name, path, type, size and modified criteria, and
combine them with `AND`, `OR`, `NOT` and parentheses:

| Criterion    | Matches items in a library that                                                                   |
| ------------ | ------------------------------------------------------------------------------------------------- |
| `tag:Animal` | have a tag whose name, an alias or shorthand is `Animal`, ignoring case, or one of its child tags |
| `tag=Animal` | have such a tag assigned directly, not through a child tag                                        |
| `tag:ani*`   | have a tag whose name, an alias or shorthand matches the wildcard pattern, or one of its children |
| `tagged:yes` | have at least one tag                                                                             |
| `tagged:no`  | have no tags, including items that aren't indexed yet                                             |

For example, `tag:holiday type:image/* NOT tag:private` finds holiday images
that aren't private, and `tagged:no type:file` finds files still to be
tagged. Quote a value to match `*` and `?` literally, as in `tag:"what?"`.
`tags:` is accepted for `tag:`.

Tag criteria only match items inside a registered library. Outside a library,
none match, not even `tagged:no`, so `NOT tagged:yes` is the way to include
items outside libraries. Entries whose file is missing are treated as
untagged, because a different file may now be at their path, and the library
document itself is never a result. A search reads each library once, so
changes made while it runs aren't reflected in its results.

**Select by Query…** and **Deselect by Query…** apply a query to the items
listed in the active pane, or to every shown item in flat view. They support
local folders only and can't use `content:` criteria; use Search to find text
within files. `meta:` criteria work there, but read every listed file before
the selection changes, so they can be slow in large folders. Indexed search
still needs a name or content term, such as `report tag:work`.

[TagStudio]: https://github.com/TagStudioDev/TagStudio
