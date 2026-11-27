# Qt Widgets and Qt Quick

[![Docs index](https://img.shields.io/badge/docs-index-3daee9?style=flat-square)](README.md)
[![Roadmap: architecture](https://img.shields.io/badge/roadmap-architecture-8957e5?style=flat-square)](ROADMAP.md#architecture)
[![Issue #563](https://img.shields.io/badge/issue-%23563-6e7781?style=flat-square&logo=github)](https://github.com/mikepadiernos/plasma-opus/issues/563)

Plasma Opus is a Qt Widgets application. This document records why Widgets
remains its user interface toolkit, where Qt Quick may be embedded, and the
rules for doing so.

> [!NOTE]
> No Qt Quick code ships today. Embedded mpv playback ([#563]), the first
> candidate, draws through libmpv's OpenGL render API in a `QOpenGLWidget`,
> so it does not need Qt Quick.

## Contents

- [Decision](#decision)
- [Why Widgets stays primary](#why-widgets-stays-primary)
- [What Widgets already covers](#what-widgets-already-covers)
- [Candidates](#candidates)
- [Embedding rules](#embedding-rules)
- [Costs](#costs)
- [Criteria for new candidates](#criteria-for-new-candidates)
- [Not planned](#not-planned)

## Decision

- **Qt Widgets** stays the toolkit for windows, panes, views, panels, dialogs
  and settings.
- **Qt Quick** may be embedded for a self-contained component only when it
  needs something Widgets cannot provide with reasonable effort, such as a
  renderer that exists only for Qt Quick.
- The application is not rewritten in QML and does not adopt a Kirigami
  application shell.

## Why Widgets stays primary

| Reason                  | Detail                                                                                                                                                                                                          |
| ----------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| KDE components          | KXmlGui, `KFilePlacesView`, `KUrlNavigator`, `KFileItemActions`, `KPropertiesDialog`, KTextEditor and the Konsole part are widgets. Most have no direct Qt Quick equivalent.                                    |
| Dense item views        | Details, folder tree, tree lines, minimap and Yazi mode columns build on `QAbstractItemView`, which provides selection, inline rename, drag and drop, keyboard navigation and accessibility for large listings. |
| Plasma consistency      | Widgets follow Plasma's widget style, colours and fonts in the same way as Dolphin, Kate and Konsole.                                                                                                           |
| Existing implementation | The whole of `src/app/` is Widgets code. Rewriting it would cost far more than it would gain for a desktop file manager.                                                                                        |

## What Widgets already covers

Several features that are often associated with Qt Quick are already
implemented with Widgets and KDE Frameworks:

| Feature                     | Implementation                                                                  |
| --------------------------- | ------------------------------------------------------------------------------- |
| M17 background blur         | KWin's blur effect through `KWindowEffects` in `src/app/windoweffects.cpp`      |
| M17 card view               | Card view mode in `src/app/folderview.cpp`                                      |
| Startup presentation motion | `QPropertyAnimation` in `src/app/startuppresentation.cpp`                       |
| Audio and video playback    | Qt Multimedia's `QMediaPlayer` and `QVideoWidget` in `src/app/mediaview.cpp`    |
| Embedded mpv playback       | libmpv's OpenGL render API in a `QOpenGLWidget` in `src/app/mpvvideowidget.cpp` |
| M32.03 interface animations | Planned with Qt's animation framework, following Plasma's animation speed (M17) |

Window blur comes from KWin, not from the toolkit, so Qt Quick would not
improve it.

## Candidates

| Component                              | Why Qt Quick                                            | Status                                                                             |
| -------------------------------------- | ------------------------------------------------------- | ---------------------------------------------------------------------------------- |
| Embedded mpv playback ([#563], M27.05) | KDE's MpvQt renders libmpv only through a Qt Quick item | Not adopted: libmpv's OpenGL render API draws into a `QOpenGLWidget` without MpvQt |

The **Audio and video playback** setting under **Preferences › Applications ›
Media**, and the backend button beside the playback controls, choose between
Qt Multimedia and mpv. Qt Multimedia remains the fallback when a build lacks
libmpv or OpenGL is unavailable for video.

[KWin scripts](integrations/kde-plasma.md#kwin-scripts) can use QML, but they
are separate packages that KWin runs. They are not part of the application's
user interface and do not change this decision.

## Embedding rules

1. **Host:** embed Qt Quick content with `QQuickWidget`, so it stacks, clips
   and resizes like other widgets in panes, docks and splitters. Use
   `QWidget::createWindowContainer()` only when measurements show
   `QQuickWidget` is too slow, and handle the native window's stacking,
   clipping and focus limits.
2. **Optional build:** find Qt Quick and any component library, such as
   MpvQt, as optional dependencies. Compile the component only when they are
   found, as Plasma Opus already does for Baloo, and keep a Widgets fallback
   with an explicit message when the component is unavailable.
3. **Logic in `src/core/`:** use QML only for presentation. Keep models, state
   and validation in C++ in `src/core/` and cover them with QtTest.
4. **Plasma appearance:** use Qt Quick Controls with KDE's `org.kde.desktop`
   style from qqc2-desktop-style, so controls follow the Plasma widget style.
   Use theme icons and do not bundle a controls theme.
5. **Localization:** expose KI18n to QML so its strings use `i18n()` and
   `i18nc()` like the rest of the application.
6. **Keyboard and accessibility:** move focus into and out of the Qt Quick
   content with <kbd>Tab</kbd> and the pane focus shortcuts, keep KXmlGui
   shortcuts working while it has focus, and give controls accessible names.
7. **Failure isolation:** contain process-wide side effects. For example,
   libmpv requires the C numeric locale, so set it without changing number
   formatting elsewhere in the application. Report initialization failures and
   fall back; never terminate the application.
8. **Lifecycle:** create the component when it is first shown, release its
   graphics resources when it is hidden, and test repeated opening and closing.
9. **Rendering:** respect Qt's graphics API selection and test on Wayland and
   X11, with hardware and software rendering.

## Costs

| Cost            | Detail                                                                                                                                         |
| --------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| Rendering       | `QQuickWidget` renders offscreen and then composites the result, which adds GPU memory use and latency compared with a native Qt Quick window. |
| Dependencies    | Qt Quick, Qt Quick Controls and qqc2-desktop-style become runtime dependencies for the component.                                              |
| Input and focus | Focus, shortcuts and drag and drop must be bridged between Widgets and Qt Quick.                                                               |
| Styling         | Two styling systems must stay consistent with Plasma and with M17 app-local colours.                                                           |
| Testing         | The headless dev container has no GPU. Tests use Qt Quick's software backend, and visual behaviour must be verified on Plasma.                 |

## Criteria for new candidates

Adopt Qt Quick for a component only when all of these apply:

- It needs a capability that Widgets lacks or makes impractical, such as a
  renderer that exists only for Qt Quick or GPU effects that KWin does not
  provide.
- It is self-contained in one panel or view, with a narrow C++ interface.
- It is optional, with a Widgets fallback or an explicit unavailable state.
- Measured rendering cost and input latency are acceptable in its host pane.

Record each accepted candidate in the [Candidates](#candidates) table and in
its issue.

## Not planned

- Rewriting panes, views, dialogs or settings in QML.
- A Kirigami application shell or a convergent mobile layout.
- Qt Quick effects in place of KWin's window blur.

[#563]: https://github.com/mikepadiernos/plasma-opus/issues/563
