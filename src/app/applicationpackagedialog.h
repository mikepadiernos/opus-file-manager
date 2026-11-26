// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "apppackages.h"

#include <QDialog>
#include <QHash>
#include <QPointer>

#include <functional>
#include <memory>

class KMessageWidget;
class PackageTools;
class QCheckBox;
class QFormLayout;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTemporaryDir;

/**
 * Shows what an AppImage, Flatpak or snap package is and offers its actions:
 * running it, installing, updating or uninstalling it with flatpak or snap,
 * opening it in Discover and adding an AppImage to the application menu.
 *
 * Changes are confirmed first and run one at a time. The dialog never
 * changes sandbox permissions or confinement, and asks before marking an
 * AppImage executable.
 */
class ApplicationPackageDialog : public QDialog
{
    Q_OBJECT

public:
    using Confirmation = std::function<bool(const QString &title, const QString &text, const QString &action)>;

    /// @param tools stays owned by the caller
    ApplicationPackageDialog(const QString &path,
                             const QString &mimeType,
                             PackageTools *tools,
                             const AppPackages::Locations &locations = AppPackages::Locations::system(),
                             QWidget *parent = nullptr);
    ~ApplicationPackageDialog() override;

    const AppPackages::Package &package() const;
    /// Whether a tool is running.
    bool isBusy() const;
    /// Replaces the confirmation question, for tests.
    void setConfirmation(Confirmation confirmation);
    /// Performs @p action after the checks and confirmation the buttons use.
    void trigger(AppPackages::Action action);

Q_SIGNALS:
    /// The package was installed, updated, removed or integrated.
    void packageChanged();

private:
    void reload();
    void rebuildActions();
    void loadDetails(int index);
    void runChange(AppPackages::Action action);
    void integrate(int step);
    void finishIntegration();
    void finishChange(AppPackages::Action action, bool succeeded, const QString &error);
    void setBusy(bool busy);
    void appendOutput(const AppPackages::Command &command, const QString &output);
    bool confirm(AppPackages::Action action);
    bool ensureExecutable();
    QString actionText(AppPackages::Action action) const;
    QString missingToolText(const QString &tool) const;

    QString m_path;
    QString m_mimeType;
    QPointer<PackageTools> m_tools;
    AppPackages::Locations m_locations;
    AppPackages::Package m_package;
    Confirmation m_confirmation;
    std::unique_ptr<QTemporaryDir> m_extractFolder;
    bool m_busy = false;

    KMessageWidget *m_message;
    QLabel *m_icon;
    QLabel *m_title;
    QFormLayout *m_form;
    QCheckBox *m_systemWide;
    QPlainTextEdit *m_details;
    QProgressBar *m_progress;
    QWidget *m_actions;
    QHash<AppPackages::Action, QPushButton *> m_buttons;
};
