#include "ThumbnailWorker.h"
#include <QImageReader>
#include <QFileInfo>
#include <QPainter>
#include <QDebug>
#include <QProcess>       
#include <QTemporaryFile> 
#include <QCoreApplication> // For applicationDirPath with QTemporaryFile

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
    QImage scaledImage = generateScaledImage(request.filePath, request.targetSize);
    emit imageReady(request.row, scaledImage, request.filePath, request.targetSize); // Emit QImage & original targetSize
    emit finished(); 
}

QImage ThumbnailWorker::generateScaledImage(const QString& filePath, const QSize& targetSize) // Returns QImage
{
    QFileInfo fileInfo(filePath);
    QStringList imageExts = {"jpg", "jpeg", "png", "bmp", "gif", "webp", "tiff"};
    
    if (imageExts.contains(fileInfo.suffix().toLower())) {
        QImageReader reader(filePath);
        reader.setAutoTransform(true); 
        QImage originalImage = reader.read();
        if (!originalImage.isNull()) {
            return originalImage.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        } else {
            qWarning() << "ThumbnailWorker: Failed to read image" << filePath << reader.errorString();
            // Return a specific placeholder for unreadable images
            QImage errorPlaceholder(targetSize, QImage::Format_RGB32);
            errorPlaceholder.fill(Qt::red);
            return errorPlaceholder;
        }
    } else {
        // Could be a video or other non-image type
        // For now, return a generic video placeholder QImage
        // Actual video frame grabbing is a TODO
        QStringList videoExts = {"mp4", "mkv", "webm", "avi", "mov"};
        if (videoExts.contains(fileInfo.suffix().toLower())) {
            QTemporaryFile tempFile;
            tempFile.setFileTemplate(QCoreApplication::applicationDirPath() + "/temp_thumb_XXXXXX.jpg"); // Ensure it's in a writable location if needed
            if (tempFile.open()) {
                QString tempFramePath = tempFile.fileName();
                tempFile.close(); // Close it so FFmpeg can write to it

                QProcess ffmpeg;
                QStringList arguments;
                arguments << "-ss" << "00:00:01" // Seek to 1 second
                          << "-i" << filePath
                          << "-vframes" << "1"   // Extract one frame
                          << "-q:v" << "2"       // Good quality for JPG
                          << "-y"                // Overwrite output file if it exists
                          << tempFramePath;
                
                // qDebug() << "FFmpeg command:" << "ffmpeg" << arguments.join(" ");
                ffmpeg.start("ffmpeg", arguments);

                if (ffmpeg.waitForStarted(3000)) { // Wait for ffmpeg to start
                    if (ffmpeg.waitForFinished(5000)) { // Wait up to 5 seconds for completion
                        if (ffmpeg.exitStatus() == QProcess::NormalExit && ffmpeg.exitCode() == 0) {
                            QImage frame(tempFramePath);
                            QFile::remove(tempFramePath); // Clean up temp file
                            if (!frame.isNull()) {
                                return frame.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                            } else {
                                qDebug() << "ThumbnailWorker: FFmpeg extracted null frame for" << filePath;
                            }
                        } else {
                            qWarning() << "ThumbnailWorker: FFmpeg failed for" << filePath 
                                       << "Exit code:" << ffmpeg.exitCode() 
                                       << "Error:" << ffmpeg.readAllStandardError();
                        }
                    } else {
                        qWarning() << "ThumbnailWorker: FFmpeg timed out for" << filePath;
                        ffmpeg.kill(); // Ensure process is killed if it timed out
                    }
                } else {
                     qWarning() << "ThumbnailWorker: FFmpeg failed to start for" << filePath << ffmpeg.errorString();
                }
                QFile::remove(tempFramePath); // Ensure cleanup even on failure
            } else {
                 qWarning() << "ThumbnailWorker: Could not open temporary file for video frame.";
            }

            // Fallback placeholder if FFmpeg fails
            QImage videoPlaceholder(targetSize, QImage::Format_RGB32);
            videoPlaceholder.fill(Qt::darkCyan); 
            QPainter painter(&videoPlaceholder);
            painter.setPen(Qt::white);
            painter.drawText(videoPlaceholder.rect(), Qt::AlignCenter, "Video");
            painter.end();
            return videoPlaceholder;
        }
    }
    // Fallback for unknown types
    QImage unknownPlaceholder(targetSize, QImage::Format_RGB32);
    unknownPlaceholder.fill(Qt::lightGray);
    return unknownPlaceholder;
}
