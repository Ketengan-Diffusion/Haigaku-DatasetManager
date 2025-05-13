#ifndef THUMBNAILWORKER_H
#define THUMBNAILWORKER_H

#include <QObject>
#include <QString>
#include <QSize>
#include <QIcon>

struct ThumbnailRequest {
    int row;
    QString filePath;
    QSize targetSize;
};

class ThumbnailWorker : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailWorker(QObject *parent = nullptr);
    ~ThumbnailWorker();

public slots:
    void processRequest(const ThumbnailRequest &request); // Slot to start processing

signals:
    void thumbnailReady(int row, const QIcon &icon);
    void finished(); // To signal completion to ThumbnailLoader for thread management

private:
    QIcon generateIcon(const QString& filePath, const QSize& targetSize);
};

#endif // THUMBNAILWORKER_H
