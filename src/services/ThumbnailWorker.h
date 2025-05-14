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
    void imageReady(int row, const QImage &scaledImage, const QString &filePath, const QSize &originalTargetSize); // Added originalTargetSize
    void finished();

private:
    QImage generateScaledImage(const QString& filePath, const QSize& targetSize); // Returns QImage
};

#endif // THUMBNAILWORKER_H
