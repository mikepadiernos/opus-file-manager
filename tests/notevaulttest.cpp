// SPDX-License-Identifier: GPL-3.0-or-later

#include "notevault.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace NoteVault;

namespace
{
void write(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(contents);
}

QStringList relativePathsOf(const Index &index)
{
    QStringList paths;
    for (const Note &note : index.notes) {
        paths.append(note.relativePath);
    }
    paths.sort();
    return paths;
}
}

class NoteVaultTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_root;
    QString m_outside;

    QString path(const QString &relative) const
    {
        return m_root + QLatin1Char('/') + relative;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        m_root = QFileInfo(m_dir.path()).canonicalFilePath() + QStringLiteral("/Vault");
        m_outside = QFileInfo(m_dir.path()).canonicalFilePath() + QStringLiteral("/Elsewhere");

        write(path(QStringLiteral("Home.md")), "# Home\nSee [[Plan]], [[Projects/Plan|the project plan]] and [[Ideas#Later]].\n![[diagram.png]]\n");
        write(path(QStringLiteral("Ideas.markdown")), "Back to [[home]].\n```\n[[Plan]]\n```\n");
        write(path(QStringLiteral("Plan.md")), "Top-level plan. [[Home]] #Project\n");
        write(path(QStringLiteral("Projects/Plan.md")), "Project plan. [[../Home]] [[./Notes]] [[Plan]]\n");
        write(path(QStringLiteral("Projects/Notes.mkd")), "Notes. [[Plan]] #projects\n");
        write(path(QStringLiteral("Archive/Plan.md")), "---\ntags: [project/old]\n---\nOld plan.\n");
        write(path(QStringLiteral("Journal/Today.md")), "[[Plan]] [[Missing note]] [[../../Elsewhere/Secret]]\n");
        write(path(QStringLiteral("diagram.png")), "png");
        write(path(QStringLiteral("readme.txt")), "[[Home]]\n");
        write(path(QStringLiteral(".obsidian/Hidden.md")), "[[Home]]\n");
        write(path(QStringLiteral("Invalid.md")), "\xff\xfe [[Home]]\n");
        write(m_outside + QStringLiteral("/Secret.md"), "[[Home]]\n");
        QVERIFY(QFile::link(m_outside, path(QStringLiteral("Linked outside"))));
        QVERIFY(QFile::link(path(QStringLiteral("Projects")), path(QStringLiteral("Projects again"))));
    }

    void recognizesMarkdownFileNames()
    {
        QVERIFY(isNoteFileName(QStringLiteral("a.md")));
        QVERIFY(isNoteFileName(QStringLiteral("a.MARKDOWN")));
        QVERIFY(isNoteFileName(QStringLiteral("a.b.mkd")));
        QVERIFY(!isNoteFileName(QStringLiteral("a.txt")));
        QVERIFY(!isNoteFileName(QStringLiteral("md")));
    }

    void indexesNotesInsideTheVault()
    {
        const Index index = scan(m_root);
        QCOMPARE(index.root, m_root);
        QCOMPARE(relativePathsOf(index),
                 (QStringList{QStringLiteral("Archive/Plan.md"),
                              QStringLiteral("Home.md"),
                              QStringLiteral("Ideas.markdown"),
                              QStringLiteral("Journal/Today.md"),
                              QStringLiteral("Plan.md"),
                              QStringLiteral("Projects/Notes.mkd"),
                              QStringLiteral("Projects/Plan.md")}));
        QCOMPARE(index.skipped, QStringList{path(QStringLiteral("Invalid.md"))});
        QVERIFY(!index.truncated);
        QVERIFY(!index.isComplete());

        const qsizetype ideas = index.indexOf(path(QStringLiteral("Ideas.markdown")));
        QVERIFY(ideas >= 0);
        QCOMPARE(index.notes.at(ideas).name, QStringLiteral("Ideas"));
        QCOMPARE(index.notes.at(ideas).links.size(), 1);
        QCOMPARE(index.byName.value(QStringLiteral("plan")).size(), 3);
        QCOMPARE(index.indexOf(QStringLiteral("/nowhere.md")), qsizetype(-1));
    }

    void stopsAtTheFileLimitOrWhenCanceled()
    {
        const Index limited = scan(m_root, 2);
        QCOMPARE(limited.notes.size(), 2);
        QVERIFY(limited.truncated);

        const Index canceled = scan(m_root, DefaultFileLimit, [] {
            return true;
        });
        QVERIFY(canceled.notes.isEmpty());
        QVERIFY(canceled.truncated);

        const Index missing = scan(m_root + QStringLiteral("/No such folder"));
        QVERIFY(missing.root.isEmpty());
        QVERIFY(!missing.isComplete());
        QCOMPARE(resolve(missing, path(QStringLiteral("Home.md")), QStringLiteral("Plan")).status, Status::Missing);
    }

    void resolvesLinks()
    {
        const Index index = scan(m_root);
        const QString home = path(QStringLiteral("Home.md"));
        const QString projectPlan = path(QStringLiteral("Projects/Plan.md"));

        Resolution r = resolve(index, home, QStringLiteral("Plan"));
        QCOMPARE(r.status, Status::Found);
        QCOMPARE(r.path, path(QStringLiteral("Plan.md")));

        QCOMPARE(resolve(index, home, QStringLiteral("projects/plan")).path, projectPlan);
        QCOMPARE(resolve(index, home, QStringLiteral("Projects/Plan.md")).path, projectPlan);
        QCOMPARE(resolve(index, home, QStringLiteral("IDEAS")).path, path(QStringLiteral("Ideas.markdown")));
        QCOMPARE(resolve(index, home, QStringLiteral("Ideas.markdown")).path, path(QStringLiteral("Ideas.markdown")));
        QCOMPARE(resolve(index, home, QString()).path, home);
        QCOMPARE(resolve(index, home, QStringLiteral("diagram.png")).path, path(QStringLiteral("diagram.png")));
        QCOMPARE(resolve(index, home, QStringLiteral("readme.txt")).path, path(QStringLiteral("readme.txt")));

        // Same-folder notes win over the vault-wide name match.
        QCOMPARE(resolve(index, projectPlan, QStringLiteral("Plan")).path, projectPlan);
        QCOMPARE(resolve(index, projectPlan, QStringLiteral("../Home")).path, home);
        QCOMPARE(resolve(index, projectPlan, QStringLiteral("./Notes")).path, path(QStringLiteral("Projects/Notes.mkd")));
        QCOMPARE(resolve(index, projectPlan, QStringLiteral("Notes.mkd")).path, path(QStringLiteral("Projects/Notes.mkd")));

        r = resolve(index, path(QStringLiteral("Journal/Today.md")), QStringLiteral("Plan"));
        QCOMPARE(r.status, Status::Ambiguous);
        QCOMPARE(r.candidates, (QStringList{path(QStringLiteral("Archive/Plan.md")), path(QStringLiteral("Plan.md")), projectPlan}));

        QCOMPARE(resolve(index, home, QStringLiteral("Missing note")).status, Status::Missing);
        QCOMPARE(resolve(index, home, QStringLiteral("Hidden")).status, Status::Missing);
        QCOMPARE(resolve(index, home, QStringLiteral("Projects/Missing")).status, Status::Missing);
        QCOMPARE(resolve(index, path(QStringLiteral("Journal/Today.md")), QStringLiteral("../../Elsewhere/Secret")).status, Status::Outside);
        QCOMPARE(resolve(index, home, QStringLiteral("../Elsewhere/Secret.md")).status, Status::Outside);
        QCOMPARE(resolve(index, home, QStringLiteral("/etc/passwd")).status, Status::Outside);
        QCOMPARE(resolve(index, home, QStringLiteral("~/notes")).status, Status::Outside);
        // A link through a folder that links outside the vault isn't followed.
        QCOMPARE(resolve(index, home, QStringLiteral("Linked outside/Secret.md")).status, Status::Missing);
    }

    void listsBacklinks()
    {
        const Index index = scan(m_root);
        const QList<Backlink> toHome = backlinks(index, path(QStringLiteral("Home.md")));
        QStringList sources;
        for (const Backlink &backlink : toHome) {
            sources.append(QDir(m_root).relativeFilePath(backlink.path));
        }
        sources.sort();
        QCOMPARE(sources, (QStringList{QStringLiteral("Ideas.markdown"), QStringLiteral("Plan.md"), QStringLiteral("Projects/Plan.md")}));

        const QList<Backlink> toProjectPlan = backlinks(index, path(QStringLiteral("Projects/Plan.md")));
        QCOMPARE(toProjectPlan.size(), 2);
        for (const Backlink &backlink : toProjectPlan) {
            QVERIFY(backlink.path == path(QStringLiteral("Home.md")) || backlink.path == path(QStringLiteral("Projects/Notes.mkd")));
        }
        const auto fromHome = std::find_if(toProjectPlan.cbegin(), toProjectPlan.cend(), [this](const Backlink &backlink) {
            return backlink.path == path(QStringLiteral("Home.md"));
        });
        QVERIFY(fromHome != toProjectPlan.cend());
        QCOMPARE(fromHome->link.alias, QStringLiteral("the project plan"));
        QCOMPARE(fromHome->link.line, 1);

        QVERIFY(backlinks(index, path(QStringLiteral("Archive/Plan.md"))).isEmpty());
    }

    void listsTaggedNotes()
    {
        const Index index = scan(m_root);
        const qsizetype plan = index.indexOf(path(QStringLiteral("Plan.md")));
        QVERIFY(plan >= 0);
        QCOMPARE(index.notes.at(plan).tags, QStringList{QStringLiteral("Project")});
        QCOMPARE(tagged(index, QStringLiteral("project")), (QStringList{path(QStringLiteral("Archive/Plan.md")), path(QStringLiteral("Plan.md"))}));
        QCOMPARE(tagged(index, QStringLiteral("Project/Old")), QStringList{path(QStringLiteral("Archive/Plan.md"))});
        QCOMPARE(tagged(index, QStringLiteral("projects")), QStringList{path(QStringLiteral("Projects/Notes.mkd"))});
        QVERIFY(tagged(index, QStringLiteral("proj")).isEmpty());
        QVERIFY(tagged(index, QString()).isEmpty());
    }
};

QTEST_GUILESS_MAIN(NoteVaultTest)

#include "notevaulttest.moc"
