// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigGroup>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace WorkspaceCatalog
{
enum class PanePlacement {
    Split,
    Tab,
    Window
};
enum class PanelLayout {
    Combined,
    Separate
};
enum class PreviewPlacement {
    Pane,
    Below,
    Right
};
enum class EditorPlacement {
    Panel,
    Below,
    Right,
    Maximized,
    Window
};

struct Presentation {
    PanePlacement panePlacement = PanePlacement::Split;
    PanelLayout panelLayout = PanelLayout::Combined;
    PreviewPlacement previewPlacement = PreviewPlacement::Pane;
    EditorPlacement editorPlacement = EditorPlacement::Panel;
    bool previewVisible = false;
    bool editorVisible = false;
    bool editorPresent = false;
    bool terminalVisible = false;
    QStringList windowObjects;
    bool operator==(const Presentation &other) const = default;
};

struct Layout {
    QByteArray windowState;
    QList<int> paneSizes;
    QList<int> workspaceSizes;
    bool dual = true;
    bool stacked = true;
    std::optional<Presentation> presentation;
    bool operator==(const Layout &other) const = default;
};

struct Workspace {
    QString id;
    QString name;
    QString shortLabel;
    std::optional<Layout> layout;
    bool operator==(const Workspace &other) const = default;
};

struct Catalog {
    QList<Workspace> workspaces;
    QString activeId;
    bool operator==(const Catalog &other) const = default;
};

bool load(const KConfigGroup &group, Catalog &catalog, QString &error);
bool save(KConfigGroup group, const Catalog &catalog, QString &error);
/**
 * Saves updated only when the stored catalog still equals expected.
 * The named configuration must have no pending changes.
 * @returns false and sets error if validation, locking or saving fails
 */
bool saveIfUnchanged(KConfigGroup group, const Catalog &expected, const Catalog &updated, QString &error);
bool create(Catalog &catalog, const QString &name, const QString &shortLabel, QString &error);
bool duplicate(Catalog &catalog, const QString &id, const QString &name, const QString &shortLabel, QString &error);
bool setLayout(Catalog &catalog, const QString &id, const Layout &layout, QString &error);
bool rename(Catalog &catalog, const QString &id, const QString &name, const QString &shortLabel, QString &error);
bool activate(Catalog &catalog, const QString &id, QString &error);
bool remove(Catalog &catalog, const QString &id, QString &error);
}
