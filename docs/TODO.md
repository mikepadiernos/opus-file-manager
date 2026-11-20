# Pending tasks

## Clarify separators and expanded space (#444)

The original screenshot shows the toolbar, Explorer panel and file tree, but
does not identify the affected control, configuration or steps. The
tree-expansion height check passed, so the tree-specific diagnosis is
unconfirmed. Ask which separator and expanded space were configured (including
whether these are toolbar customization items), the steps and settings used,
and what should appear instead. Reproduce the reported behaviour before
implementing a fix; leave [#444](https://github.com/mikepadiernos/opus-file-manager/issues/444)
open in Development until then.

## Verify the affected ExifTool installation (#446)

The audit environment selected a Perl interpreter unable to load ExifTool's
module. A controlled PATH using the system Perl allowed the metadata tests to
pass; Opus now identifies the selected executable and its error. Repair the
affected ExifTool/Perl installation or PATH, run the reported executable with
`-ver` in Opus's environment, and verify metadata preview and writes there.
Leave [#446](https://github.com/mikepadiernos/opus-file-manager/issues/446)
open until that environment is verified.
