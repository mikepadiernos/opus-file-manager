// SPDX-License-Identifier: GPL-3.0-or-later

#include "mpvplayer.h"

#include <KLocalizedString>

#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>

#include <mpv/client.h>

#include <clocale>
#include <cstring>
#include <utility>

namespace
{
enum Observed : quint64 {
    PauseProperty = 1,
    DurationProperty,
    PositionProperty,
    EndProperty,
};

mpv_handle *createHandle()
{
    if (mpv_handle *handle = mpv_create()) {
        return handle;
    }
    // Some libmpv versions refuse to start unless numbers use the C locale. Qt
    // formats numbers through QLocale, so only the C library's parsing changes.
    const char *numeric = std::setlocale(LC_NUMERIC, nullptr);
    if (numeric && std::strcmp(numeric, "C") != 0) {
        std::setlocale(LC_NUMERIC, "C");
        return mpv_create();
    }
    return nullptr;
}
}

MpvPlayer::MpvPlayer(const QHash<QString, QString> &options, QObject *parent)
    : QObject(parent)
{
    m_mpv = createHandle();
    if (!m_mpv) {
        m_error = i18nc("@info", "libmpv could not be created");
        return;
    }
    // Previews never read the user's mpv configuration, run scripts or reach the network on their own.
    QHash<QString, QString> settings{
        {QStringLiteral("config"), QStringLiteral("no")},
        {QStringLiteral("terminal"), QStringLiteral("no")},
        {QStringLiteral("load-scripts"), QStringLiteral("no")},
        {QStringLiteral("load-auto-profiles"), QStringLiteral("no")},
        {QStringLiteral("ytdl"), QStringLiteral("no")},
        {QStringLiteral("load-unsafe-playlists"), QStringLiteral("no")},
        {QStringLiteral("input-default-bindings"), QStringLiteral("no")},
        {QStringLiteral("input-vo-keyboard"), QStringLiteral("no")},
        {QStringLiteral("idle"), QStringLiteral("yes")},
        {QStringLiteral("keep-open"), QStringLiteral("yes")},
        {QStringLiteral("hwdec"), QStringLiteral("auto-safe")},
        {QStringLiteral("vo"), QStringLiteral("libmpv")},
    };
    settings.insert(options);
    for (auto it = settings.cbegin(); it != settings.cend(); ++it) {
        mpv_set_option_string(m_mpv, it.key().toUtf8().constData(), it.value().toUtf8().constData());
    }
    const int initialized = mpv_initialize(m_mpv);
    if (initialized < 0) {
        m_error = QString::fromUtf8(mpv_error_string(initialized));
        mpv_terminate_destroy(std::exchange(m_mpv, nullptr));
        return;
    }
    mpv_observe_property(m_mpv, PauseProperty, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, DurationProperty, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, PositionProperty, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, EndProperty, "eof-reached", MPV_FORMAT_FLAG);
    // mpv calls this from its own threads; events are read on the player's thread.
    mpv_set_wakeup_callback(
        m_mpv,
        [](void *context) {
            QMetaObject::invokeMethod(static_cast<MpvPlayer *>(context), &MpvPlayer::processEvents, Qt::QueuedConnection);
        },
        this);
}

MpvPlayer::~MpvPlayer()
{
    if (m_mpv) {
        // No wakeup runs after this returns, and queued calls die with the object.
        mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
        mpv_terminate_destroy(m_mpv);
    }
}

bool MpvPlayer::isValid() const
{
    return m_mpv;
}

QString MpvPlayer::errorString() const
{
    return m_error;
}

mpv_handle *MpvPlayer::handle() const
{
    return m_mpv;
}

void MpvPlayer::load(const QString &path, bool video, bool autoplay)
{
    if (!m_mpv) {
        return;
    }
    m_path = QFileInfo(path).absoluteFilePath();
    m_atEnd = false;
    m_duration = 0;
    m_position = 0;
    mpv_set_property_string(m_mpv, "vid", video ? "auto" : "no");
    mpv_set_property_string(m_mpv, "pause", autoplay ? "no" : "yes");
    // An absolute path is never read as a protocol or option.
    command({QByteArrayLiteral("loadfile"), QFile::encodeName(m_path)});
}

void MpvPlayer::stop()
{
    if (m_mpv) {
        command({QByteArrayLiteral("stop")});
    }
    m_path.clear();
    m_atEnd = false;
}

void MpvPlayer::setPaused(bool paused)
{
    if (!m_mpv) {
        return;
    }
    if (!paused && m_atEnd) {
        seek(0);
    }
    int flag = paused ? 1 : 0;
    mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &flag);
}

bool MpvPlayer::isPaused() const
{
    return m_paused;
}

void MpvPlayer::seek(double seconds)
{
    if (m_mpv) {
        command({QByteArrayLiteral("seek"), QByteArray::number(seconds, 'f', 3), QByteArrayLiteral("absolute")});
    }
}

double MpvPlayer::duration() const
{
    return m_duration;
}

double MpvPlayer::position() const
{
    return m_position;
}

QString MpvPlayer::path() const
{
    return m_path;
}

void MpvPlayer::command(const QList<QByteArray> &arguments)
{
    QList<const char *> argv;
    argv.reserve(arguments.size() + 1);
    for (const QByteArray &argument : arguments) {
        argv.append(argument.constData());
    }
    argv.append(nullptr);
    mpv_command_async(m_mpv, 0, argv.data());
}

void MpvPlayer::processEvents()
{
    const QPointer<MpvPlayer> self(this);
    while (self && m_mpv) {
        const mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            return;
        }
        switch (event->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE: {
            const auto *property = static_cast<const mpv_event_property *>(event->data);
            const bool hasValue = property->data && property->format != MPV_FORMAT_NONE;
            switch (event->reply_userdata) {
            case PauseProperty:
                if (hasValue && property->format == MPV_FORMAT_FLAG) {
                    m_paused = *static_cast<const int *>(property->data);
                    Q_EMIT pausedChanged(m_paused);
                }
                break;
            case DurationProperty:
                m_duration = hasValue && property->format == MPV_FORMAT_DOUBLE ? *static_cast<const double *>(property->data) : 0;
                Q_EMIT durationChanged(m_duration);
                break;
            case PositionProperty:
                m_position = hasValue && property->format == MPV_FORMAT_DOUBLE ? *static_cast<const double *>(property->data) : 0;
                Q_EMIT positionChanged(m_position);
                break;
            case EndProperty:
                m_atEnd = hasValue && property->format == MPV_FORMAT_FLAG && *static_cast<const int *>(property->data);
                break;
            default:
                break;
            }
            break;
        }
        case MPV_EVENT_FILE_LOADED:
            Q_EMIT fileLoaded();
            break;
        case MPV_EVENT_END_FILE: {
            const auto *end = static_cast<const mpv_event_end_file *>(event->data);
            if (end->reason == MPV_END_FILE_REASON_ERROR) {
                m_error = QString::fromUtf8(mpv_error_string(end->error));
                Q_EMIT errorOccurred(m_error);
            }
            break;
        }
        case MPV_EVENT_SHUTDOWN:
            return;
        default:
            break;
        }
    }
}
