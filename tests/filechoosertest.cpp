// SPDX-License-Identifier: GPL-3.0-or-later

#include "filechooser.h"

#include <KConfig>
#include <KConfigGroup>

#include <QDBusMetaType>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using namespace FileChooser;

class FileChooserTest : public QObject
{
    Q_OBJECT

private:
    static Filter imagesFilter()
    {
        return {QStringLiteral("Images"), {{0, QStringLiteral("*.png")}, {1, QStringLiteral("image/jpeg")}}};
    }

    static Filter textFilter()
    {
        return {QStringLiteral("Text"), {{0, QStringLiteral("*.txt")}}};
    }

private Q_SLOTS:
    void initTestCase()
    {
        registerTypes();
    }

    void parsesOpenOptions()
    {
        const QVariantMap options{
            {QStringLiteral("accept_label"), QStringLiteral("_Import & go")},
            {QStringLiteral("modal"), false},
            {QStringLiteral("multiple"), true},
            {QStringLiteral("directory"), false},
            {QStringLiteral("filters"), QVariant::fromValue(FilterList{imagesFilter(), textFilter()})},
            {QStringLiteral("current_filter"), QVariant::fromValue(textFilter())},
            {QStringLiteral("choices"),
             QVariant::fromValue(ChoiceList{{QStringLiteral("encoding"),
                                             QStringLiteral("Encoding"),
                                             {{QStringLiteral("utf8"), QStringLiteral("UTF-8")}, {QStringLiteral("latin1"), QStringLiteral("Latin-1")}},
                                             QStringLiteral("latin1")}})},
            {QStringLiteral("current_folder"), QByteArray("/tmp/folder\0", 12)},
            {QStringLiteral("current_name"), QStringLiteral("ignored.txt")},
        };
        const Request request = parseRequest(Mode::Open, QStringLiteral("Pick"), options);
        QCOMPARE(request.mode, Mode::Open);
        QCOMPARE(request.title, QStringLiteral("Pick"));
        QCOMPARE(request.acceptLabel, QStringLiteral("&Import && go"));
        QVERIFY(!request.modal);
        QVERIFY(request.multiple);
        QVERIFY(!request.directory);
        QCOMPARE(request.filters.size(), 2);
        QCOMPARE(request.currentFilter, 1);
        QCOMPARE(request.choices.size(), 1);
        QCOMPARE(request.choices.constFirst().options.size(), 2);
        QCOMPARE(request.choices.constFirst().initial, QStringLiteral("latin1"));
        QCOMPARE(request.currentFolder, QUrl::fromLocalFile(QStringLiteral("/tmp/folder")));
        QVERIFY(request.currentName.isEmpty());
    }

    void handlesCurrentFilters()
    {
        const auto withCurrent = [](const FilterList &filters, const Filter &current) {
            QVariantMap options{{QStringLiteral("current_filter"), QVariant::fromValue(current)}};
            if (!filters.isEmpty()) {
                options.insert(QStringLiteral("filters"), QVariant::fromValue(filters));
            }
            return parseRequest(Mode::Save, QString(), options);
        };
        Request request = withCurrent({}, textFilter());
        QVERIFY(request.filters.isEmpty());
        QCOMPARE(request.currentFilter, -1);
        QCOMPARE(request.fixedFilter, textFilter());

        request = withCurrent({imagesFilter()}, textFilter());
        QCOMPARE(request.filters, (FilterList{imagesFilter(), textFilter()}));
        QCOMPARE(request.currentFilter, 1);

        request = parseRequest(Mode::Open, QString(), {{QStringLiteral("filters"), QVariant::fromValue(FilterList{imagesFilter(), textFilter()})}});
        QCOMPARE(request.currentFilter, 0);
    }

    void parsesSaveOptions()
    {
        Request request = parseRequest(Mode::Save,
                                       QString(),
                                       {{QStringLiteral("current_name"), QStringLiteral("report.odt")},
                                        {QStringLiteral("current_file"), QByteArray("/home/user/old.odt\0", 19)},
                                        {QStringLiteral("multiple"), true}});
        QVERIFY(!request.multiple);
        QCOMPARE(request.currentName, QStringLiteral("report.odt"));
        QCOMPARE(request.currentFile, QUrl::fromLocalFile(QStringLiteral("/home/user/old.odt")));
        QCOMPARE(initialName(request), QStringLiteral("report.odt"));
        QCOMPARE(initialFolder(request), QUrl::fromLocalFile(QStringLiteral("/home/user")));

        request.currentName.clear();
        QCOMPARE(initialName(request), QStringLiteral("old.odt"));
        request.currentFolder = QUrl::fromLocalFile(QStringLiteral("/srv"));
        QCOMPARE(initialFolder(request), QUrl::fromLocalFile(QStringLiteral("/srv")));
    }

    void parsesSaveFilesNames()
    {
        const QByteArrayList files{QByteArray("one.txt\0", 8), QByteArray("../two.txt"), QByteArray()};
        Request request = parseRequest(Mode::SaveFiles, QString(), {{QStringLiteral("files"), QVariant::fromValue(files)}});
        QCOMPARE(request.files, (QStringList{QStringLiteral("one.txt"), QStringLiteral("two.txt")}));
        QVERIFY(request.filters.isEmpty());
    }

    void buildsResults()
    {
        Request request = parseRequest(
            Mode::Open,
            QString(),
            {{QStringLiteral("filters"), QVariant::fromValue(FilterList{imagesFilter(), textFilter()})},
             {QStringLiteral("choices"), QVariant::fromValue(ChoiceList{{QStringLiteral("ro"), QStringLiteral("Read only"), {}, QStringLiteral("false")}})}});
        Result result;
        result.urls = {QUrl::fromLocalFile(QStringLiteral("/tmp/a b.txt"))};
        result.filter = 1;
        result.choices = {{QStringLiteral("ro"), QStringLiteral("true")}};
        const QVariantMap map = results(request, result);
        QCOMPARE(map.value(QStringLiteral("uris")).toStringList(), QStringList{QStringLiteral("file:///tmp/a%20b.txt")});
        QCOMPARE(map.value(QStringLiteral("current_filter")).value<Filter>(), textFilter());
        QCOMPARE(map.value(QStringLiteral("choices")).value<ChoiceValues>(), result.choices);
        QCOMPARE(map.value(QStringLiteral("writable")).toBool(), true);

        request.mode = Mode::Save;
        request.choices.clear();
        result.filter = -1;
        const QVariantMap saved = results(request, result);
        QVERIFY(!saved.contains(QStringLiteral("writable")));
        QVERIFY(!saved.contains(QStringLiteral("choices")));
        QVERIFY(!saved.contains(QStringLiteral("current_filter")));
    }

    void marshalsTypes()
    {
        const auto signature = [](QMetaType type) {
            return QByteArray(QDBusMetaType::typeToSignature(type));
        };
        QCOMPARE(signature(QMetaType::fromType<FilterList>()), QByteArray("a(sa(us))"));
        QCOMPARE(signature(QMetaType::fromType<Filter>()), QByteArray("(sa(us))"));
        QCOMPARE(signature(QMetaType::fromType<ChoiceList>()), QByteArray("a(ssa(ss)s)"));
        QCOMPARE(signature(QMetaType::fromType<ChoiceValues>()), QByteArray("a(ss)"));
    }

    void convertsMnemonics()
    {
        QCOMPARE(qtMnemonic(QStringLiteral("_Open")), QStringLiteral("&Open"));
        QCOMPARE(qtMnemonic(QStringLiteral("Save__as")), QStringLiteral("Save_as"));
        QCOMPARE(qtMnemonic(QStringLiteral("Q&A")), QStringLiteral("Q&&A"));
        QCOMPARE(qtMnemonic(QString()), QString());
    }

    void expandsMimeTypes()
    {
        const QStringList globs = globPatterns(imagesFilter());
        QVERIFY(globs.contains(QStringLiteral("*.png")));
        QVERIFY(globs.contains(QStringLiteral("*.jpg")));
        QVERIFY(!globs.contains(QStringLiteral("*.txt")));

        const QStringList images = globPatterns({QString(), {{1, QStringLiteral("image/*")}}});
        QVERIFY(images.contains(QStringLiteral("*.png")));
        QVERIFY(images.contains(QStringLiteral("*.gif")));

        const QStringList text = globPatterns({QString(), {{1, QStringLiteral("text/plain")}}});
        QVERIFY(text.contains(QStringLiteral("*.txt")));
        QVERIFY(text.contains(QStringLiteral("*.c")));
        QCOMPARE(globPatterns({QString(), {{1, QStringLiteral("application/octet-stream")}}}), QStringList{QStringLiteral("*")});
        QVERIFY(globPatterns({QString(), {{1, QStringLiteral("no/such-type")}}}).isEmpty());
    }

    void addsTheFilterExtension()
    {
        const QStringList globs{QStringLiteral("*.png"), QStringLiteral("*.PNG")};
        QCOMPARE(withFilterExtension(QStringLiteral("photo"), globs), QStringLiteral("photo.png"));
        QCOMPARE(withFilterExtension(QStringLiteral("photo.jpeg"), globs), QStringLiteral("photo.jpeg"));
        QCOMPARE(withFilterExtension(QStringLiteral("photo"), {QStringLiteral("*")}), QStringLiteral("photo"));
        QCOMPARE(withFilterExtension(QStringLiteral("Makefile"), {QStringLiteral("Makefile"), QStringLiteral("*.mk")}), QStringLiteral("Makefile"));
        QCOMPARE(withFilterExtension(QStringLiteral("rules"), {QStringLiteral("Makefile"), QStringLiteral("*.mk")}), QStringLiteral("rules.mk"));
        QCOMPARE(withFilterExtension(QString(), globs), QString());
    }

    void resolvesSaveTargets()
    {
        const QUrl folder = QUrl::fromLocalFile(QStringLiteral("/data/docs"));
        QCOMPARE(saveTarget(folder, QStringLiteral("a.txt")), QUrl::fromLocalFile(QStringLiteral("/data/docs/a.txt")));
        QCOMPARE(saveTarget(folder, QStringLiteral("sub/../b.txt")), QUrl::fromLocalFile(QStringLiteral("/data/docs/b.txt")));
        QCOMPARE(saveTarget(folder, QStringLiteral("/etc/c.txt")), QUrl::fromLocalFile(QStringLiteral("/etc/c.txt")));
        QCOMPARE(saveTarget(folder, QStringLiteral("~/d.txt")), QUrl::fromLocalFile(QDir::homePath() + QStringLiteral("/d.txt")));
        QVERIFY(saveTarget(folder, QStringLiteral("  ")).isEmpty());
        QCOMPARE(saveFilesTargets(folder, {QStringLiteral("x.txt"), QStringLiteral("../y.txt")}),
                 (QList<QUrl>{QUrl::fromLocalFile(QStringLiteral("/data/docs/x.txt")), QUrl::fromLocalFile(QStringLiteral("/data/docs/y.txt"))}));
    }

    void choosesTheStartFolder()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QUrl last = QUrl::fromLocalFile(root.path());
        const QUrl home = QUrl::fromLocalFile(QDir::homePath());

        Request request;
        QCOMPARE(startFolder(request), home);
        request.lastFolder = last;
        QCOMPARE(startFolder(request), last);
        request.lastFolder = QUrl::fromLocalFile(root.filePath(QStringLiteral("missing")));
        QCOMPARE(startFolder(request), home);
        request.lastFolder = QUrl(QStringLiteral("sftp://host/folder"));
        QCOMPARE(startFolder(request), home);

        // A folder the application suggests comes first.
        request.lastFolder = last;
        request.currentFile = QUrl::fromLocalFile(QStringLiteral("/data/docs/a.txt"));
        QCOMPARE(startFolder(request), QUrl::fromLocalFile(QStringLiteral("/data/docs")));
        request.currentFolder = QUrl::fromLocalFile(QStringLiteral("/data"));
        QCOMPARE(startFolder(request), request.currentFolder);
    }

    void findsTheFolderOfTheAcceptedChoice()
    {
        Request request;
        Result result;
        QVERIFY(acceptedFolder(request, result).isEmpty());
        result.urls = {QUrl::fromLocalFile(QStringLiteral("/data/docs/a.txt")), QUrl::fromLocalFile(QStringLiteral("/other/b.txt"))};
        QCOMPARE(acceptedFolder(request, result), QUrl::fromLocalFile(QStringLiteral("/data/docs")));

        request.mode = Mode::Save;
        result.urls = {QUrl::fromLocalFile(QStringLiteral("/data/new.txt"))};
        QCOMPARE(acceptedFolder(request, result), QUrl::fromLocalFile(QStringLiteral("/data")));

        request.mode = Mode::SaveFiles;
        result.urls = {QUrl::fromLocalFile(QStringLiteral("/data/sub/x.txt")), QUrl::fromLocalFile(QStringLiteral("/data/sub/y.txt"))};
        QCOMPARE(acceptedFolder(request, result), QUrl::fromLocalFile(QStringLiteral("/data/sub")));

        request.mode = Mode::Open;
        request.directory = true;
        result.urls = {QUrl::fromLocalFile(QStringLiteral("/data/sub/"))};
        QCOMPARE(acceptedFolder(request, result), QUrl::fromLocalFile(QStringLiteral("/data/sub")));

        result.urls = {QUrl(QStringLiteral("sftp://host/data/a.txt"))};
        QVERIFY(acceptedFolder(request, result).isEmpty());
    }

    void remembersTheLastFolderPerApplication()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        KConfig state(root.filePath(QStringLiteral("staterc")), KConfig::SimpleConfig);
        KConfigGroup group = state.group(stateGroupName());
        QVERIFY(lastFolder(group, QStringLiteral("org.example.App")).isEmpty());
        QVERIFY(lastFolder(group, QString()).isEmpty());

        rememberFolder(group, QStringLiteral("org.example.App"), QUrl::fromLocalFile(QStringLiteral("/data/app")));
        rememberFolder(group, QString(), QUrl::fromLocalFile(QStringLiteral("/data/unknown")));
        rememberFolder(group, QStringLiteral("org.example.Other"), QUrl());
        rememberFolder(group, QStringLiteral("org.example.Other"), QUrl(QStringLiteral("sftp://host/data")));
        QVERIFY(state.sync());

        const KConfig reread(root.filePath(QStringLiteral("staterc")), KConfig::SimpleConfig);
        const KConfigGroup saved = reread.group(stateGroupName());
        QCOMPARE(lastFolder(saved, QStringLiteral("org.example.App")), QUrl::fromLocalFile(QStringLiteral("/data/app")));
        QCOMPARE(lastFolder(saved, QString()), QUrl::fromLocalFile(QStringLiteral("/data/unknown")));
        QVERIFY(lastFolder(saved, QStringLiteral("org.example.Other")).isEmpty());
        QCOMPARE(stateGroupName(), QStringLiteral("FileChooser"));
    }
};

QTEST_GUILESS_MAIN(FileChooserTest)

#include "filechoosertest.moc"
