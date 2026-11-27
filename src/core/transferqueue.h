// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KJob>

#include <QList>
#include <QPointer>
#include <QString>

#include <functional>

class TransferQueue;

class QueuedTransfer : public KJob
{
    Q_OBJECT

public:
    enum class State {
        Waiting,
        Running,
        Paused,
        Completed,
        Failed,
        Canceled
    };
    using Factory = std::function<KJob *()>;

    ~QueuedTransfer() override;
    QString description() const;
    State state() const;
    unsigned long bytesPerSecond() const;
    QString failure() const;
    void start() override;

protected:
    bool doKill() override;
    bool doSuspend() override;
    bool doResume() override;

private:
    friend class TransferQueue;
    QueuedTransfer(QString description, Factory factory, TransferQueue *queue);
    void begin();

    TransferQueue *m_queue;
    QString m_description;
    Factory m_factory;
    QPointer<KJob> m_job;
    State m_state = State::Waiting;
    unsigned long m_speed = 0;
    QString m_failure;
    bool m_killing = false;
};

class TransferQueue : public QObject
{
    Q_OBJECT

public:
    explicit TransferQueue(QObject *parent = nullptr);
    QueuedTransfer *enqueue(const QString &description, QueuedTransfer::Factory factory);
    QList<QueuedTransfer *> entries() const;
    /// Zero starts all waiting transfers without a concurrency limit.
    void setConcurrency(int concurrency);
    int concurrency() const;
    /// Suspends running transfers and holds waiting ones until resumed.
    void setPaused(bool paused);
    bool isPaused() const;
    void clearFinished();

Q_SIGNALS:
    void changed();

private:
    friend class QueuedTransfer;
    void schedule();
    void startNext();

    QList<QueuedTransfer *> m_entries;
    int m_concurrency = 1;
    bool m_scheduled = false;
    bool m_paused = false;
};
