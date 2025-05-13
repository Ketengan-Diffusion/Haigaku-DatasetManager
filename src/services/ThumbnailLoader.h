#ifndef THUMBNAILLOADER_H
#define THUMBNAILLOADER_H

#include <QObject>
#include <QString>
#include <QSize>
#include <QIcon>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QSet>
#include "ThumbnailWorker.h" // Include the worker definition

class ThumbnailLoader : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailLoader(QObject *parent = nullptr);
    ~ThumbnailLoader();

    void requestThumbnail(int row, const QString &filePath, const QSize &targetSize);
    void requestThumbnailBatch(const QList<ThumbnailRequest> &requests); // For viewport-driven loading
    void clearQueue();
    void setMaxWorkers(int count);

signals:
    void thumbnailReady(int row, const QIcon &icon);
    void workerAvailable(); // Internal signal to pick next task

private slots:
    void onWorkerFinished(); // A worker has finished its task and is available
    void dispatchNextRequest(); // Dispatches a request to an available worker

private:
    void startWorkers();
    void stopWorkers();

    QList<QThread*> m_workerThreads;
    QList<ThumbnailWorker*> m_availableWorkers;
    QList<ThumbnailWorker*> m_busyWorkers;

    QQueue<ThumbnailRequest> m_requestQueue;
    QMutex m_queueMutex;
    QSet<int> m_pendingOrProcessingRows; // Rows that are in queue or being processed

    int m_maxWorkers;
};

#endif // THUMBNAILLOADER_H
