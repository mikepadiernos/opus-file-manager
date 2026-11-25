// SPDX-License-Identifier: GPL-3.0-or-later

#include "tagassignment.h"

#include "libraryregistry.h"
#include "libraryscan.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QHash>

namespace
{
struct Cached {
    QString signature;
    TagLibrary::Library library;
};

// File timestamps can be coarse and inodes are reused, so writers in this
// process also invalidate entries instead of relying on the signature alone.
QHash<QString, Cached> &cache()
{
    thread_local QHash<QString, Cached> libraries;
    return libraries;
}
}

TagAssignment::Selection TagAssignment::resolve(const QStringList &absolutePaths)
{
    Selection selection;
    if (absolutePaths.isEmpty()) {
        selection.problem = i18nc("@info", "Select files or folders to tag.");
        return selection;
    }
    for (const QString &path : absolutePaths) {
        const QString root = LibraryRegistry::rootFor(path);
        const QString clean = QDir::cleanPath(path);
        if (root.isEmpty() || clean == root) {
            selection = {};
            selection.problem = absolutePaths.size() == 1 ? i18nc("@info", "The selected item isn't in a tag library.")
                                                          : i18nc("@info", "Some selected items aren't in a tag library.");
            return selection;
        }
        if (!selection.root.isEmpty() && selection.root != root) {
            selection = {};
            selection.problem = i18nc("@info", "The selected items are in different libraries; tags can only be assigned within one library.");
            return selection;
        }
        selection.root = root;
        selection.relativePaths.append(clean.mid(root.size() + 1));
    }
    return selection;
}

bool TagAssignment::apply(const QString &root,
                          const QStringList &absolutePaths,
                          const QStringList &add,
                          const QStringList &remove,
                          int *changed,
                          QString *error)
{
    QList<TagLibrary::Observation> items;
    for (const QString &path : absolutePaths) {
        TagLibrary::Observation observation;
        if (QFileInfo(path).isSymLink()) {
            if (error) {
                *error = i18nc("@info", "Symbolic links can't be tagged: %1", path);
            }
            return false;
        }
        if (LibraryRegistry::rootFor(path) != root || !LibraryScan::observe(root, path, &observation)) {
            if (error) {
                *error = i18nc("@info", "This item can't be tagged in the library: %1", path);
            }
            return false;
        }
        items.append(observation);
    }
    const bool saved = TagLibrary::update(
        root,
        [&](TagLibrary::Library &library, QString *problem) {
            return library.assignTags(items, add, remove, changed, problem);
        },
        error);
    invalidate(root);
    return saved;
}

TagLibrary::Library TagAssignment::cachedLibrary(const QString &root, QString *error)
{
    QHash<QString, Cached> &libraries = cache();
    const QString signature = TagLibrary::documentSignature(root);
    const auto it = libraries.constFind(root);
    if (it != libraries.cend() && it->signature == signature) {
        return it->library;
    }
    TagLibrary::Library library = TagLibrary::Library::load(root, error);
    if (library.isNull()) {
        libraries.remove(root);
        return library;
    }
    libraries.insert(root, {signature, library});
    return library;
}

void TagAssignment::invalidate(const QString &root)
{
    if (root.isEmpty()) {
        cache().clear();
    } else {
        cache().remove(root);
    }
}
