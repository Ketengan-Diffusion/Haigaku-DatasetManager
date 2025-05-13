#include "ThumbnailWorker.h"
#include <QImageReader>
#include <QFileInfo>
#include <QPainter>
#include <QDebug>

ThumbnailWorker::ThumbnailWorker(QObject *parent) : QObject(parent)
{
}

ThumbnailWorker::~ThumbnailWorker()
{
    // qDebug() << "ThumbnailWorker destroyed";
}

void ThumbnailWorker::processRequest(const ThumbnailRequest &request)
{
    // This method is called in the worker thread.
    QIcon icon = generateIcon(request.filePath, request.targetSize);
    emit thumbnailReady(request.row, icon);
    emit finished(); // Signal that this worker has finished its current task
}

QIcon ThumbnailWorker::generateIcon(const QString& filePath, const QSize& targetSize)
{
    // This is the same core logic as ThumbnailLoader::generateThumbnailTask
    QFileInfo fileInfo(filePath);
    QStringList imageExts = {"jpg", "jpeg", "png", "bmp", "gif", "webp", "tiff"};
    
    if (imageExts.contains(fileInfo.suffix().toLower())) {
        QImageReader reader(filePath);
        QImage originalImage = reader.read();
        if (!originalImage.isNull()) {
            QImage scaledImage = originalImage.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPixmap finalThumbnail(targetSize);
            finalThumbnail.fill(Qt::transparent);
            QPainter painter(&finalThumbnail);
            int x = (targetSize.width() - scaledImage.width()) / 2;
            int y = (targetSize.height() - scaledImage.height()) / 2;
            painter.drawImage(x, y, scaledImage);
            painter.end();
            return QIcon(finalThumbnail);
        } else {
            qWarning() << "ThumbnailWorker: Failed to read image" << filePath << reader.errorString();
        }
    } else { 
        QPixmap placeholder(targetSize);
        placeholder.fill(Qt::darkGray); 
        return QIcon(placeholder);
    }
    QPixmap placeholder(targetSize);
    placeholder.fill(Qt::lightGray);
    return QIcon(placeholder);
}
