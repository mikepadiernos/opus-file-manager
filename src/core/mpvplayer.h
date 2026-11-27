// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QObject>
#include <QString>

struct mpv_handle;

/**
 * Plays one local audio or video file through libmpv.
 *
 * The player only decodes and controls playback; MpvVideoWidget in the
 * application draws its video through libmpv's render API. Options passed to
 * the constructor override the defaults, which disable user configuration,
 * scripts, network helpers and input bindings.
 */
class MpvPlayer : public QObject
{
    Q_OBJECT

public:
    explicit MpvPlayer(const QHash<QString, QString> &options = {}, QObject *parent = nullptr);
    ~MpvPlayer() override;

    /// Whether libmpv started; otherwise errorString() explains why.
    bool isValid() const;
    QString errorString() const;
    mpv_handle *handle() const;

    /// Loads a local file, replacing the current one. Audio-only loads skip video tracks.
    void load(const QString &path, bool video, bool autoplay);
    void stop();
    void setPaused(bool paused);
    bool isPaused() const;
    /// Seeks to an absolute position in seconds.
    void seek(double seconds);
    double duration() const;
    double position() const;
    QString path() const;

Q_SIGNALS:
    void pausedChanged(bool paused);
    void durationChanged(double seconds);
    void positionChanged(double seconds);
    void fileLoaded();
    void errorOccurred(const QString &message);

private:
    void processEvents();
    void command(const QList<QByteArray> &arguments);

    mpv_handle *m_mpv = nullptr;
    QString m_error;
    QString m_path;
    bool m_paused = true;
    bool m_atEnd = false;
    double m_duration = 0;
    double m_position = 0;
};
