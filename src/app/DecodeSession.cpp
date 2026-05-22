#include "app/DecodeSession.h"

#include "core/decode/DecodeWorker.h"

#include <QThread>

DecodeSession::DecodeSession(QObject *parent)
    : QObject(parent)
{
}

DecodeSession::~DecodeSession()
{
    stopCurrent(false);
}

int DecodeSession::generation() const
{
    return m_generation;
}

bool DecodeSession::isActive() const
{
    return !m_worker.isNull() && !m_thread.isNull();
}

void DecodeSession::start(const QString &filePath,
                          int startFrameIndex,
                          bool pauseAfterFirstFrame,
                          const FrameSeekCheckpoint &seekCheckpoint)
{
    ++m_generation;
    stopCurrent(false);

    const int generation = m_generation;
    m_threadGeneration = generation;
    m_finishedGeneration = -1;
    m_thread = new QThread(this);
    m_worker = new DecodeWorker;
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, [worker = QPointer<DecodeWorker>(m_worker),
                                                    filePath,
                                                    startFrameIndex,
                                                    pauseAfterFirstFrame,
                                                    seekCheckpoint]() {
        if (worker.isNull()) {
            return;
        }
        if (seekCheckpoint.frameIndex >= 0) {
            worker->decodeFileFromCheckpoint(filePath, startFrameIndex, pauseAfterFirstFrame, seekCheckpoint);
        } else {
            worker->decodeFileFromFrame(filePath, startFrameIndex, pauseAfterFirstFrame);
        }
    });

    connect(m_worker, &DecodeWorker::streamOpened,
            this, [this, generation](const StreamInfo &streamInfo) {
                if (generation == m_threadGeneration) {
                    emit streamOpened(streamInfo);
                }
            },
            Qt::QueuedConnection);
    connect(m_worker, &DecodeWorker::frameReady,
            this, [this, generation](int frameIndex, const DecodedVideoFramePtr &frame, const FrameAnalysis &analysis) {
                if (generation == m_threadGeneration) {
                    emit frameReady(frameIndex, frame, analysis);
                }
            },
            Qt::QueuedConnection);
    connect(m_worker, &DecodeWorker::accessUnitAnalysisDecoded,
            this, [this, generation](const FrameAnalysis &analysis) {
                if (generation == m_threadGeneration) {
                    emit accessUnitAnalysisDecoded(analysis);
                }
            },
            Qt::QueuedConnection);
    connect(m_worker, &DecodeWorker::seekCheckpointReady,
            this, [this, generation](const FrameSeekCheckpoint &checkpoint) {
                if (generation == m_threadGeneration) {
                    emit seekCheckpointReady(checkpoint);
                }
            },
            Qt::QueuedConnection);
    connect(m_worker, &DecodeWorker::bufferingProgress,
            this, [this, generation](int startFrameIndex, int currentFrameIndex, int targetFrameIndex) {
                if (generation == m_threadGeneration) {
                    emit bufferingProgress(startFrameIndex, currentFrameIndex, targetFrameIndex);
                }
            },
            Qt::QueuedConnection);
    connect(m_worker, &DecodeWorker::logMessage,
            this, [this, generation](const QString &message) {
                if (generation == m_threadGeneration) {
                    emit logMessage(message);
                }
            },
            Qt::QueuedConnection);
    connect(m_worker, &DecodeWorker::errorOccurred,
            this, [this, generation](const QString &message) {
                if (generation == m_threadGeneration) {
                    emit errorOccurred(message);
                }
            },
            Qt::QueuedConnection);

    connect(m_worker, &DecodeWorker::finished, m_thread, &QThread::quit);
    connect(m_worker, &DecodeWorker::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, this, [this, generation]() {
        finishGeneration(generation, true);
    });

    m_thread->start();
}

void DecodeSession::stop()
{
    stopCurrent(true);
}

void DecodeSession::play()
{
    if (!m_worker.isNull()) {
        m_worker->play();
    }
}

void DecodeSession::pause()
{
    if (!m_worker.isNull()) {
        m_worker->pause();
    }
}

void DecodeSession::stepForward()
{
    if (!m_worker.isNull()) {
        m_worker->stepForward();
    }
}

void DecodeSession::stopCurrent(bool emitStoppedSignal)
{
    const int generation = m_threadGeneration;

    if (!m_worker.isNull()) {
        m_worker->stop();
    }

    if (!m_thread.isNull()) {
        QObject::disconnect(m_thread, nullptr, this, nullptr);
        m_thread->quit();
        m_thread->wait();
        finishGeneration(generation, emitStoppedSignal);
    }
}

void DecodeSession::finishGeneration(int generation, bool emitStoppedSignal)
{
    if (generation <= 0 || generation != m_threadGeneration || m_finishedGeneration == generation) {
        return;
    }

    m_finishedGeneration = generation;
    m_thread = nullptr;
    m_worker = nullptr;
    m_threadGeneration = 0;
    if (emitStoppedSignal) {
        emit stopped();
    }
}
