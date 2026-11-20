// SPDX-License-Identifier: GPL-3.0-or-later

#include "transferqueue.h"

#include <KLocalizedString>

#include <QTimer>

#include <utility>

QueuedTransfer::QueuedTransfer(QString description, Factory factory, TransferQueue *queue)
    : KJob(queue)
    , m_queue(queue)
    , m_description(std::move(description))
    , m_factory(std::move(factory))
{
    setAutoDelete(false);
    setCapabilities(Killable);
}

QueuedTransfer::~QueuedTransfer()
{
    if (m_job) {
        m_job->disconnect(this);
        m_job->kill(KJob::Quietly);
    }
}

QString QueuedTransfer::description() const
{
    return m_description;
}

QueuedTransfer::State QueuedTransfer::state() const
{
    return m_state;
}

unsigned long QueuedTransfer::bytesPerSecond() const
{
    return m_speed;
}

QString QueuedTransfer::failure() const
{
    return m_failure;
}

void QueuedTransfer::start()
{
    m_queue->schedule();
}

void QueuedTransfer::begin()
{
    m_state = State::Running;
    m_job = m_factory();
    m_factory = {};
    if (!m_job) {
        m_state = State::Failed;
        m_failure = i18nc("@info", "Could not start the transfer.");
        setError(KJob::UserDefinedError);
        setErrorText(m_failure);
        Q_EMIT m_queue->changed();
        emitResult();
        m_queue->schedule();
        return;
    }
    setCapabilities(m_job->capabilities());
    connect(m_job, &KJob::percentChanged, this, [this](KJob *, unsigned long value) {
        setPercent(value);
        Q_EMIT m_queue->changed();
    });
    connect(m_job, &KJob::speed, this, [this](KJob *, unsigned long speed) {
        m_speed = speed;
        emitSpeed(speed);
        Q_EMIT m_queue->changed();
    });
    connect(m_job, &KJob::result, this, [this](KJob *job) {
        if (m_killing) {
            return;
        }
        m_job = nullptr;
        m_speed = 0;
        if (job->error()) {
            m_state = State::Failed;
            m_failure = job->errorString();
            setError(job->error());
            setErrorText(job->errorText());
        } else {
            m_state = State::Completed;
            setPercent(100);
        }
        Q_EMIT m_queue->changed();
        emitResult();
        m_queue->schedule();
    });
    connect(m_job, &KJob::finished, this, [this](KJob *) {
        if (m_killing) {
            return;
        }
        QTimer::singleShot(0, this, [this] {
            if (m_state != State::Running && m_state != State::Paused) {
                return;
            }
            m_job = nullptr;
            m_state = State::Failed;
            m_failure = i18nc("@info", "The transfer ended without a result.");
            setError(KJob::UserDefinedError);
            setErrorText(m_failure);
            Q_EMIT m_queue->changed();
            emitResult();
            m_queue->schedule();
        });
    });
    connect(m_job, &QObject::destroyed, this, [this] {
        if (m_killing || (m_state != State::Running && m_state != State::Paused)) {
            return;
        }
        m_job = nullptr;
        m_state = State::Failed;
        m_failure = i18nc("@info", "The transfer job ended unexpectedly.");
        setError(KJob::UserDefinedError);
        setErrorText(m_failure);
        Q_EMIT m_queue->changed();
        emitResult();
        m_queue->schedule();
    });
    Q_EMIT m_queue->changed();
}

bool QueuedTransfer::doKill()
{
    if (m_state == State::Waiting) {
        m_factory = {};
    } else if (m_job) {
        m_killing = true;
        const bool killed = m_job->kill(KJob::Quietly);
        m_killing = false;
        if (!killed) {
            return false;
        }
        m_job = nullptr;
    } else {
        return false;
    }
    m_state = State::Canceled;
    m_speed = 0;
    Q_EMIT m_queue->changed();
    m_queue->schedule();
    return true;
}

bool QueuedTransfer::doSuspend()
{
    if (!m_job || !m_job->suspend()) {
        return false;
    }
    m_state = State::Paused;
    m_speed = 0;
    Q_EMIT m_queue->changed();
    return true;
}

bool QueuedTransfer::doResume()
{
    if (!m_job || !m_job->resume()) {
        return false;
    }
    m_state = State::Running;
    Q_EMIT m_queue->changed();
    return true;
}

TransferQueue::TransferQueue(QObject *parent)
    : QObject(parent)
{
}

QueuedTransfer *TransferQueue::enqueue(const QString &description, QueuedTransfer::Factory factory)
{
    auto *entry = new QueuedTransfer(description, std::move(factory), this);
    m_entries.append(entry);
    Q_EMIT changed();
    entry->start();
    return entry;
}

QList<QueuedTransfer *> TransferQueue::entries() const
{
    return m_entries;
}

void TransferQueue::setConcurrency(int concurrency)
{
    if (concurrency < 0 || concurrency == m_concurrency) {
        return;
    }
    m_concurrency = concurrency;
    Q_EMIT changed();
    schedule();
}

int TransferQueue::concurrency() const
{
    return m_concurrency;
}

void TransferQueue::clearFinished()
{
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        const auto state = (*it)->state();
        if (state == QueuedTransfer::State::Completed || state == QueuedTransfer::State::Failed || state == QueuedTransfer::State::Canceled) {
            (*it)->deleteLater();
            it = m_entries.erase(it);
        } else {
            ++it;
        }
    }
    Q_EMIT changed();
}

void TransferQueue::schedule()
{
    if (!m_scheduled) {
        m_scheduled = true;
        QTimer::singleShot(0, this, &TransferQueue::startNext);
    }
}

void TransferQueue::startNext()
{
    m_scheduled = false;
    int running = 0;
    for (const QueuedTransfer *entry : std::as_const(m_entries)) {
        if (entry->state() == QueuedTransfer::State::Running || entry->state() == QueuedTransfer::State::Paused) {
            ++running;
        }
    }
    const auto entries = m_entries;
    for (QueuedTransfer *entry : entries) {
        if (m_concurrency > 0 && running >= m_concurrency) {
            break;
        }
        if (m_entries.contains(entry) && entry->state() == QueuedTransfer::State::Waiting) {
            entry->begin();
            if (entry->state() == QueuedTransfer::State::Running || entry->state() == QueuedTransfer::State::Paused) {
                ++running;
            }
        }
    }
}
