# Plasma Mail integration

**[Documentation](../README.md)** · **[Roadmap](../ROADMAP.md)** ·
**[KDE Plasma integration](kde-plasma.md)**

## Status and boundaries

These are proposed workflows, not implemented Plasma Opus features or
commitments by Plasma Mail. The
[Plasma Mail roadmap](https://github.com/mikepadiernos/plasma-mail/blob/main/docs/ROADMAP.md)
is preliminary and does not define an attachment-handoff API. Prefer standard
desktop interfaces; use an application-specific interface only after both
projects define and implement it.

Plasma Mail owns messages, accounts, drafts, recipients and sending. Plasma Opus
owns file selection and file operations. Neither application should read the
other's private data or change its settings without explicit user action.

## Proposed workflows

### Compose with selected files

- Offer an explicit action to compose a message with selected files as
  attachments. Use a supported desktop email integration, such as the
  [XDG Email portal](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Email.html),
  with Plasma Mail as the configured mail client when supported. Respect the
  user's configured client; do not assume a `mailto:` URL alone transfers
  attachments.
- Show the selected filenames and explain unsupported folders or inaccessible
  files before handoff. Preserve multiple-file selection and report a rejected
  request or unavailable mail client. Opening a composer must not send mail.
- For remote files, use a supported handoff or explicitly offer to prepare
  private local copies through KIO. Show the source and download implications,
  require consent, report failures and cancellation, and keep prepared files
  available until the consumer no longer needs them. Clean up temporary copies
  safely without deleting original files.

### Drag and drop into a draft

- Allow an explicit drag of selected files to a compatible mail composer using
  supported desktop URL or document-portal transfers. Keep Plasma Mail
  responsible for accepting, displaying and attaching the files.
- Distinguish local and remote URLs, report unsupported or inaccessible items,
  and do not silently download, attach or send files. Do not expose credentials
  embedded in remote URLs.

### Save and reveal received attachments

- Let Plasma Mail own attachment selection, saving and confirmation. After an
  explicit save, it may ask the file manager to reveal the saved location using
  a supported desktop interface. Plasma Opus's planned
  [FileManager1 service](../ROADMAP.md#m27-integrations) is tracked in
  [#279](https://github.com/mikepadiernos/plasma-opus/issues/279).
- Preserve the selected destination, file identity and conflict handling; show
  errors for unavailable or removed files. Do not access message stores,
  download attachments automatically or infer that a saved file is safe.

### Open mail files and share links

- Open explicitly selected mail files through the user's registered application
  associations. Do not silently import messages or replace default applications.
- If the planned
  [Nextcloud integration](../ROADMAP.md#m29-nextcloud-integration) provides a
  share link, allow the user to choose it for a draft only after reviewing the
  link and its permissions. Do not create a share or disclose a private link
  automatically; keep share management in the Nextcloud workflow.

## Acceptance and dependencies

Verify local and remote selections, multiple files, missing clients, sandbox
portal access, cancellation, inaccessible paths and temporary-file lifetime.
Test with isolated files and synthetic messages; never send a real message as
part of an automated test. Standard desktop handoffs should continue to work
with other mail clients. Any Plasma Mail-specific capability remains optional
until its interface and availability are confirmed.
