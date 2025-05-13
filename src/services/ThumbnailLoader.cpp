#include "ThumbnailLoader.h"
#include "ThumbnailWorker.h" // Already included via .h but good practice
#include <QDebug>
#include <QMutexLocker>
#include <QCoreApplication> // For processEvents

ThumbnailLoader::ThumbnailLoader(QObject *parent) 
    : QObject(parent), m_maxWorkers(QThread::idealThreadCount() / 2)
{
    if (m_maxWorkers < 1) m_maxWorkers = 1;
    if (m_maxWorkers > 4) m_maxWorkers = 4; // Cap at 4 for I/O bound tasks
    qDebug() << "ThumbnailLoader max workers:" << m_maxWorkers;

    connect(this, &ThumbnailLoader::workerAvailable, this, &ThumbnailLoader::dispatchNextRequest, Qt::QueuedConnection);
    startWorkers();
}

ThumbnailLoader::~ThumbnailLoader()
{
    stopWorkers();
}

void ThumbnailLoader::startWorkers() {
    for (int i = 0; i < m_maxWorkers; ++i) {
        QThread* thread = new QThread(this); // Parented to ThumbnailLoader for lifecycle
        ThumbnailWorker* worker = new ThumbnailWorker(); // No parent, will be moved to thread
        worker->moveToThread(thread);

        connect(thread, &QThread::finished, worker, &QObject::deleteLater); // Clean up worker when thread finishes
        connect(worker, &ThumbnailWorker::thumbnailReady, this, &ThumbnailLoader::thumbnailReady); // Forward signal
        connect(worker, &ThumbnailWorker::finished, this, &ThumbnailLoader::onWorkerFinished, Qt::QueuedConnection);
        
        m_workerThreads.append(thread);
        m_availableWorkers.append(worker); // Initially all workers are available
        thread->start();
    }
}

void ThumbnailLoader::stopWorkers() {
    m_queueMutex.lock();
    m_requestQueue.clear();
    m_pendingOrProcessingRows.clear();
    m_queueMutex.unlock();

    for (QThread* thread : m_workerThreads) {
        thread->quit(); // Ask thread's event loop to exit
    }
    for (QThread* thread : m_workerThreads) {
        if (!thread->wait(1000)) { // Wait up to 1 sec for clean exit
            qWarning() << "ThumbnailLoader: Worker thread did not exit cleanly, terminating.";
            thread->terminate(); // Force terminate if not exiting
            thread->wait();      // Wait for termination
        }
    }
    // Workers are deleted via thread->finished connection
    m_workerThreads.clear();
    m_availableWorkers.clear(); 
    m_busyWorkers.clear();
}


void ThumbnailLoader::requestThumbnail(int row, const QString &filePath, const QSize &targetSize)
{
    QMutexLocker locker(&m_queueMutex);
    if (m_pendingOrProcessingRows.contains(row)) {
        // qDebug() << "Request for row" << row << "already pending or processing.";
        return; 
    }
    m_requestQueue.enqueue({row, filePath, targetSize});
    m_pendingOrProcessingRows.insert(row);
    // qDebug() << "Queued request for row" << row << filePath << "Queue size:" << m_requestQueue.size();
    locker.unlock(); // Unlock before emitting, though workerAvailable is queued.
    
    emit workerAvailable(); // Signal that there might be work to do
}

void ThumbnailLoader::requestThumbnailBatch(const QList<ThumbnailRequest> &requests)
{
    QMutexLocker locker(&m_queueMutex);
    bool workAdded = false;
    for(const auto& req : requests) {
        if (!m_pendingOrProcessingRows.contains(req.row)) {
            m_requestQueue.enqueue(req);
            m_pendingOrProcessingRows.insert(req.row);
            workAdded = true;
        }
    }
    locker.unlock();

    if(workAdded) {
        // Emit multiple times if multiple workers might be free, or just once.
        // One signal is enough if dispatchNextRequest handles multiple available workers.
        emit workerAvailable(); 
    }
}


void ThumbnailLoader::onWorkerFinished()
{
    ThumbnailWorker* worker = qobject_cast<ThumbnailWorker*>(sender());
    if (worker) {
        QMutexLocker locker(&m_queueMutex); // Protect access to worker lists
        m_busyWorkers.removeAll(worker);
        m_availableWorkers.append(worker);
        // The row this worker processed is implicitly removed from m_pendingOrProcessingRows
        // when its thumbnailReady signal is handled by the model, or should be.
        // Let's ensure it's removed here too if the model doesn't.
        // However, we don't know which 'row' this worker just finished without more info.
        // The `thumbnailReady` signal carries the row, so the model should update `m_pendingOrProcessingRows`.
        // For now, we assume the model or the `thumbnailReady` connection handles removing from `m_pendingOrProcessingRows`.
    }
    emit workerAvailable(); // Signal that a worker is free
}

void ThumbnailLoader::dispatchNextRequest()
{
    QMutexLocker locker(&m_queueMutex);
    if (m_requestQueue.isEmpty() || m_availableWorkers.isEmpty()) {
        return;
    }

    ThumbnailWorker* worker = m_availableWorkers.takeFirst();
    m_busyWorkers.append(worker);
    ThumbnailRequest request = m_requestQueue.dequeue();
    
    // m_pendingOrProcessingRows already contains request.row from when it was enqueued.
    // It will be removed when the model processes thumbnailReady.

    locker.unlock(); // Unlock before invoking method on another thread

    // qDebug() << "Dispatching request for row" << request.row << "to worker. Queue size:" << m_requestQueue.size();
    // Use QMetaObject::invokeMethod to call processRequest on the worker's thread
    QMetaObject::invokeMethod(worker, "processRequest", Qt::QueuedConnection,
                              Q_ARG(ThumbnailRequest, request));
    
    // If there are more requests and more workers, try to dispatch again
    if (!m_requestQueue.isEmpty() && !m_availableWorkers.isEmpty()) {
        emit workerAvailable();
    }
}


void ThumbnailLoader::clearQueue()
{
    QMutexLocker locker(&m_queueMutex);
    m_requestQueue.clear();
    // m_pendingOrProcessingRows should ideally be cleared by individual task cancellations/completions.
    // For a full clear, we might need to signal workers to stop and clear their current task if possible.
    // For now, just clear the queue and our tracking set.
    m_pendingOrProcessingRows.clear(); 
    qDebug() << "ThumbnailLoader queue and pending requests cleared.";
    // Note: This doesn't stop tasks already running in worker threads.
    // True cancellation is more complex.
}
