# Pending tasks

## Clarify separators and expanded space (#444)

The original screenshot shows the toolbar, Explorer panel and file tree, but
does not identify the affected control, configuration or steps. The
tree-expansion height check passed, so the tree-specific diagnosis is
unconfirmed. The main toolbar's default expanding spacer was found to disappear
when its layout was saved; newly saved layouts now preserve it, but previously
saved layouts may need a spacer added manually or a toolbar reset. The
screenshot alone does not establish which separator is missing or where it was
configured. Obtain the affected toolbar and configuration steps and verify the
separator separately; leave [#444](https://github.com/mikepadiernos/opus-file-manager/issues/444)
open in Development until then.

## Verify the affected ExifTool installation (#446)

The audit environment selected a Perl interpreter unable to load ExifTool's
module. A controlled PATH using the system Perl allowed the metadata tests to
pass; Opus now identifies the selected executable and its error. Repair the
affected ExifTool/Perl installation or PATH, run the reported executable with
`-ver` in Opus's environment, and verify metadata preview and writes there.
Leave [#446](https://github.com/mikepadiernos/opus-file-manager/issues/446)
open until that environment is verified.
