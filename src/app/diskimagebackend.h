// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

class QWidget;

/**
 * Mounts and unmounts disk images through Dolphin's Mount ISO plugin, and
 * reads their state from Solid, so tests can replace both.
 */
class DiskImageBackend
{
public:
    enum class State {
        /// No loop device is backed by the image.
        Detached,
        /// A loop device is backed by the image, but none of its file systems is mounted.
        Attached,
        Mounted,
    };

    struct Status {
        State state = State::Detached;
        /// Where the image is mounted, when it is.
        QString mountPoint;
    };

    virtual ~DiskImageBackend() = default;

    virtual bool pluginInstalled() const = 0;
    /**
     * Runs the plugin's Mount or Unmount action for @p file. The plugin
     * waits for UDisks2 itself and reports nothing, so read status()
     * afterwards.
     * @returns false when the plugin offered no action, as when the session
     *          bus cannot pass file descriptors
     */
    virtual bool runPluginAction(const QString &file, QWidget *parent) = 0;
    virtual Status status(const QString &file) const = 0;
};

class SystemDiskImageBackend : public DiskImageBackend
{
public:
    bool pluginInstalled() const override;
    bool runPluginAction(const QString &file, QWidget *parent) override;
    Status status(const QString &file) const override;
};
