// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

class KConfigGroup;
class QDBusArgument;

/**
 * Requests and results of the XDG portal file chooser backend interface,
 * org.freedesktop.impl.portal.FileChooser, independent of any dialog.
 */
namespace FileChooser
{
/// Portal responses: the user accepted, cancelled, or the request ended another way.
enum Response : uint {
    Accepted = 0,
    Cancelled = 1,
    Ended = 2,
};

/// One pattern of a filter: a glob (type 0) or a MIME type (type 1).
struct FilterRule {
    uint type = 0;
    QString pattern;
    bool operator==(const FilterRule &other) const = default;
};

/// A named file filter, serialized as (sa(us)).
struct Filter {
    QString name;
    QList<FilterRule> rules;
    bool operator==(const Filter &other) const = default;
};
using FilterList = QList<Filter>;

struct ChoiceOption {
    QString id;
    QString label;
    bool operator==(const ChoiceOption &other) const = default;
};

/**
 * A combo box the application adds to the chooser, serialized as
 * (ssa(ss)s). No options means a check box with the values "true" and "false".
 */
struct Choice {
    QString id;
    QString label;
    QList<ChoiceOption> options;
    QString initial;
    bool operator==(const Choice &other) const = default;
};
using ChoiceList = QList<Choice>;

/// The value chosen for a Choice, serialized as (ss).
struct ChoiceValue {
    QString id;
    QString value;
    bool operator==(const ChoiceValue &other) const = default;
};
using ChoiceValues = QList<ChoiceValue>;

enum class Mode {
    Open,
    Save,
    SaveFiles,
};

struct Request {
    Mode mode = Mode::Open;
    QString title;
    /// The accept button label, with a Qt & mnemonic.
    QString acceptLabel;
    bool modal = true;
    bool multiple = false;
    bool directory = false;
    FilterList filters;
    /// The index of the initially selected filter in filters, or -1.
    int currentFilter = -1;
    /// A filter to apply without offering a choice, when filters is empty.
    Filter fixedFilter;
    ChoiceList choices;
    QString currentName;
    /// The suggested folder, or an empty URL.
    QUrl currentFolder;
    /// The file being saved again, or an empty URL.
    QUrl currentFile;
    /// The names of the files SaveFiles saves.
    QStringList files;
    /// The requesting application's ID, or an empty string when the portal cannot identify it.
    QString appId;
    /// The folder of the application's last accepted choice, used when the request suggests none.
    QUrl lastFolder;
};

struct Result {
    QList<QUrl> urls;
    ChoiceValues choices;
    /// The index of the selected filter in the request's filters, or -1.
    int filter = -1;
};

/// Registers the D-Bus types; call before sending or receiving requests.
void registerTypes();

/**
 * Reads a request from the options of OpenFile, SaveFile or SaveFiles.
 * Unknown options and options of the wrong type are ignored.
 */
Request parseRequest(Mode mode, const QString &title, const QVariantMap &options);

/// The results vardict for an accepted request.
QVariantMap results(const Request &request, const Result &result);

/**
 * Converts an accept label's GTK mnemonic, an underscore, to Qt's ampersand.
 * @param label the label from the request
 */
QString qtMnemonic(const QString &label);

/**
 * The glob patterns that match a filter, expanding MIME types, including
 * wildcard types such as "image" followed by a slash and an asterisk, to
 * their patterns.
 */
QStringList globPatterns(const Filter &filter);

/**
 * Adds the extension of a filter's first glob, such as *.png, to a name
 * that has no extension.
 * @param name the name typed when saving
 * @param globs the selected filter's globs
 */
QString withFilterExtension(const QString &name, const QStringList &globs);

/**
 * The file to save, from the name field: an absolute path, or a name or
 * relative path inside @p folder. A leading ~/ means the home folder.
 * @returns an empty URL for an empty name
 */
QUrl saveTarget(const QUrl &folder, const QString &name);

/// The files SaveFiles writes into @p folder, in the order of @p names.
QList<QUrl> saveFilesTargets(const QUrl &folder, const QStringList &names);

/// The initial folder of a request: the suggested folder, the saved file's folder, or an empty URL.
QUrl initialFolder(const Request &request);

/**
 * The folder the chooser starts in: the initial folder, or else the last
 * folder when it is a local folder that still exists, or else the home folder.
 */
QUrl startFolder(const Request &request);

/**
 * The folder of an accepted choice, to start in next time: the chosen folder
 * of a folder request, or else the folder of the first chosen file.
 * @returns a local folder, or an empty URL
 */
QUrl acceptedFolder(const Request &request, const Result &result);

/// The group of the state configuration that keeps the folders of the last accepted choices.
QString stateGroupName();

/**
 * The folder of the last accepted choice of the application @p appId,
 * kept in @p group of the state configuration.
 * @returns a local folder, or an empty URL
 */
QUrl lastFolder(const KConfigGroup &group, const QString &appId);

/// Remembers @p folder as the application's last folder in @p group; an empty URL is ignored.
void rememberFolder(KConfigGroup &group, const QString &appId, const QUrl &folder);

/// The initial name of a SaveFile request: the suggested name or the saved file's name.
QString initialName(const Request &request);
}

Q_DECLARE_METATYPE(FileChooser::FilterRule)
Q_DECLARE_METATYPE(FileChooser::Filter)
Q_DECLARE_METATYPE(FileChooser::ChoiceOption)
Q_DECLARE_METATYPE(FileChooser::Choice)
Q_DECLARE_METATYPE(FileChooser::ChoiceValue)

QDBusArgument &operator<<(QDBusArgument &argument, const FileChooser::FilterRule &rule);
const QDBusArgument &operator>>(const QDBusArgument &argument, FileChooser::FilterRule &rule);
QDBusArgument &operator<<(QDBusArgument &argument, const FileChooser::Filter &filter);
const QDBusArgument &operator>>(const QDBusArgument &argument, FileChooser::Filter &filter);
QDBusArgument &operator<<(QDBusArgument &argument, const FileChooser::ChoiceOption &option);
const QDBusArgument &operator>>(const QDBusArgument &argument, FileChooser::ChoiceOption &option);
QDBusArgument &operator<<(QDBusArgument &argument, const FileChooser::Choice &choice);
const QDBusArgument &operator>>(const QDBusArgument &argument, FileChooser::Choice &choice);
QDBusArgument &operator<<(QDBusArgument &argument, const FileChooser::ChoiceValue &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, FileChooser::ChoiceValue &value);
