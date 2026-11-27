// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "apppackages.h"

#include <QObject>
#include <QString>

#include <functional>

/**
 * Runs the package tools AppPackages describes, so tests can replace them.
 * Callbacks run only while @p context exists.
 */
class PackageTools : public QObject
{
    Q_OBJECT

public:
    struct Result {
        /// Whether the program could be started at all.
        bool started = false;
        int exitCode = -1;
        /// Standard output and error, merged.
        QString output;

        bool succeeded() const
        {
            return started && exitCode == 0;
        }
    };

    using QObject::QObject;

    /// Whether @p program is installed and in PATH.
    virtual bool hasProgram(const QString &program) const = 0;
    /// Runs @p command to completion in @p workingDirectory and reports its output.
    virtual void run(const AppPackages::Command &command, const QString &workingDirectory, QObject *context, std::function<void(const Result &)> done) = 0;
    /// Starts @p command as a separate application; @p done receives an error message, or an empty string.
    virtual void launch(const AppPackages::Command &command, QObject *context, std::function<void(const QString &)> done) = 0;
};

/// Runs tools with QProcess and starts applications through KIO, so Plasma
/// tracks them and reports launch failures.
class SystemPackageTools : public PackageTools
{
    Q_OBJECT

public:
    using PackageTools::PackageTools;

    bool hasProgram(const QString &program) const override;
    void run(const AppPackages::Command &command, const QString &workingDirectory, QObject *context, std::function<void(const Result &)> done) override;
    void launch(const AppPackages::Command &command, QObject *context, std::function<void(const QString &)> done) override;
};
