// SPDX-License-Identifier: GPL-3.0-or-later

#include "libraryregistry.h"

#include "taglibrary.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDir>
#include <QFileInfo>

namespace
{
KConfigGroup group()
{
    return KSharedConfig::openConfig()->group(QStringLiteral("Tag Libraries"));
}

bool within(const QString &path, const QString &root)
{
    return path == root || path.startsWith(root + QLatin1Char('/'));
}

void store(QStringList roots)
{
    roots.removeDuplicates();
    roots.sort();
    KConfigGroup config = group();
    config.writePathEntry("Roots", roots);
    config.sync();
}
}

QStringList LibraryRegistry::roots()
{
    QStringList result;
    for (const QString &root : group().readPathEntry("Roots", QStringList())) {
        if (QDir::isAbsolutePath(root)) {
            result.append(QDir::cleanPath(root));
        }
    }
    result.removeDuplicates();
    result.sort();
    return result;
}

TagLibrary::Storage LibraryRegistry::storageFor(const QString &root, TagLibrary::Storage preferred)
{
    if (QFileInfo::exists(TagLibrary::privateDocumentPath(root))) {
        return TagLibrary::Storage::PrivateFolder;
    }
    if (QFileInfo::exists(TagLibrary::folderDocumentPath(root))) {
        return TagLibrary::Storage::LibraryFolder;
    }
    return preferred;
}

QString LibraryRegistry::create(const QString &root, TagLibrary::Storage storage, const QList<TagLibrary::Field> &fields)
{
    const QString clean = QDir::cleanPath(root);
    if (const QString problem = TagLibrary::checkLocation(clean); !problem.isEmpty()) {
        return problem;
    }
    const QStringList existing = roots();
    for (const QString &other : existing) {
        if (other == clean) {
            return i18nc("@info", "%1 is already a library.", clean);
        }
        if (within(clean, other)) {
            return i18nc("@info", "%1 is inside the library %2. Libraries can't be nested.", clean, other);
        }
        if (within(other, clean)) {
            return i18nc("@info", "%1 contains the library %2. Libraries can't be nested.", clean, other);
        }
    }
    QString error;
    if (QFileInfo::exists(TagLibrary::documentPath(clean))) {
        if (TagLibrary::Library::load(clean, &error).isNull()) {
            return error;
        }
    } else {
        TagLibrary::Library library(clean, storage);
        if (library.isReadOnly()) {
            if (storage == TagLibrary::Storage::PrivateFolder) {
                return i18nc("@info", "The library document %1 can't be written.", library.filePath());
            }
            return i18nc("@info",
                         "The folder is read-only, so a library can't be created there: %1. To keep its metadata privately instead, change "
                         "Preferences › Tagging › Privacy.",
                         clean);
        }
        for (TagLibrary::Field field : fields) {
            if (field.id.isEmpty()) {
                field.id = TagLibrary::createId();
            }
            library.setField(field);
        }
        if (!library.save(&error)) {
            return error;
        }
    }
    store(existing + QStringList{clean});
    return {};
}

void LibraryRegistry::remove(const QString &root)
{
    QStringList remaining = roots();
    remaining.removeAll(QDir::cleanPath(root));
    store(remaining);
}

QString LibraryRegistry::rootFor(const QString &path)
{
    const QString clean = QDir::cleanPath(path);
    for (const QString &root : roots()) {
        if (within(clean, root)) {
            return root;
        }
    }
    return {};
}

bool LibraryRegistry::contains(const QString &root)
{
    return roots().contains(QDir::cleanPath(root));
}
