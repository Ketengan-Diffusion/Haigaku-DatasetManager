#include "ThumbnailWorker.h"
#include <QImageReader>
#include <QFileInfo>
#include <QPainter>
#include <QDebug>
#include <QProcess>       
#include <QTemporaryFile> 
#include <QCoreApplication> 
#include <QDir> // Added for QDir::tempPath()
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QEventLoop>
#include <QTimer> // For timeouts

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
    qDebug() << "ThumbnailWorker::generateScaledImage for:" << filePath;
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    qDebug() << "File suffix:" << suffix;

    QStringList imageExts = {"jpg", "jpeg", "png", "bmp", "gif", "webp", "tiff"};
    
    if (imageExts.contains(suffix)) {
        qDebug() << "Processing as image (Simplified Path):" << filePath;
        QImageReader reader(filePath);
        reader.setAutoTransform(true); 
        QImage image = reader.read(); 

        if (!image.isNull()) {
            qDebug() << "Image read successfully, original size:" << image.size() << "Target size:" << targetSize << "for" << filePath;
            QImage finalScaledImage = image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            qDebug() << "Scaled image for delegate, isNull:" << finalScaledImage.isNull() << "Size:" << finalScaledImage.size();
            return finalScaledImage;
        } else {
            qWarning() << "ThumbnailWorker: [Simplified Path] Failed to read image" << filePath << "Error:" << reader.errorString() << "Code:" << reader.error();
            QImage errorPlaceholder(targetSize, QImage::Format_RGB32);
            errorPlaceholder.fill(Qt::magenta); // Changed placeholder color for this test
            return errorPlaceholder;
        }
    } else {
        qDebug() << "File not in imageExts, checking videoExts:" << filePath;
        QStringList videoExts = {"mp4", "mkv", "webm", "avi", "mov"};
        if (videoExts.contains(suffix)) {
            qDebug() << "Processing as video with FFmpeg/QProcess:" << filePath;
            QTemporaryFile tempFile;
            // Try to create temp file in a standard temp location first
            tempFile.setFileTemplate(QDir::tempPath() + "/haigaku_thumb_XXXXXX.jpg");
            if (!tempFile.open()) {
                // Fallback to application directory if system temp fails (e.g. permissions)
                tempFile.setFileTemplate(QCoreApplication::applicationDirPath() + "/temp_thumb_XXXXXX.jpg");
                if (!tempFile.open()) {
                    qWarning() << "ThumbnailWorker: Could not create temporary file for video frame in temp or app dir.";
                    QImage errorPlaceholder(targetSize, QImage::Format_RGB32);
                    errorPlaceholder.fill(Qt::darkRed);
                    return errorPlaceholder;
                }
            }
            QString tempFramePath = tempFile.fileName();
            tempFile.close(); 

            QProcess ffmpegProcess;
            ffmpegProcess.setWorkingDirectory(QCoreApplication::applicationDirPath()); 

            QStringList arguments;
            arguments << "-hide_banner" << "-loglevel" << "error"
                      << "-ss" << "00:00:01.000" // Seek to 1 second
                      << "-i" << filePath
                      << "-frames:v" << "1"      // Extract 1 frame
                      << "-vf" << QString("scale=%1:%2:force_original_aspect_ratio=decrease:flags=bicubic").arg(targetSize.width()).arg(targetSize.height()) // Added bicubic scaling
                      << "-q:v" << "2"          // Good quality for JPG
                      << "-y" << tempFramePath;

            QString ffmpegCommand = QCoreApplication::applicationDirPath() + "/ffmpeg.exe";
            
            qDebug() << "Attempting to start FFmpeg:" << ffmpegCommand << arguments;
            ffmpegProcess.start(ffmpegCommand, arguments);

            if (ffmpegProcess.waitForStarted(3000)) {
                if (ffmpegProcess.waitForFinished(10000)) { // Increased timeout
                    if (ffmpegProcess.exitStatus() == QProcess::NormalExit && ffmpegProcess.exitCode() == 0) {
                        QImage frame(tempFramePath);
                        QFile::remove(tempFramePath);
                        if (!frame.isNull()) {
                            // The frame is already scaled by ffmpeg's -vf argument
                            return frame; 
                        } else {
                            qDebug() << "ThumbnailWorker: FFmpeg extracted null frame for" << filePath;
                        }
                    } else {
                        qWarning() << "ThumbnailWorker: FFmpeg failed for" << filePath
                                   << "Exit code:" << ffmpegProcess.exitCode()
                                   << "Error:" << ffmpegProcess.readAllStandardError().trimmed() 
                                   << "Output:" << ffmpegProcess.readAllStandardOutput().trimmed();
                    }
                } else {
                    qWarning() << "ThumbnailWorker: FFmpeg timed out for" << filePath;
                    ffmpegProcess.kill();
                    ffmpegProcess.waitForFinished(1000); // wait briefly after kill
                }
            } else {
                qWarning() << "ThumbnailWorker: FFmpeg failed to start for" << filePath << ". Command:" << ffmpegCommand << "Error:" << ffmpegProcess.errorString();
            }
            QFile::remove(tempFramePath); // Ensure cleanup

            QImage videoPlaceholder(targetSize, QImage::Format_RGB32);
            videoPlaceholder.fill(Qt::darkCyan); 
            QPainter painter(&videoPlaceholder);
            painter.setPen(Qt::white);
            painter.drawText(videoPlaceholder.rect(), Qt::AlignCenter, "Video");
            painter.end();
            return videoPlaceholder;
        }
    }
    qDebug() << "File type not recognized for thumbnail generation (image/video):" << filePath;
    // Fallback for unknown types
    QImage unknownPlaceholder(targetSize, QImage::Format_RGB32);
    unknownPlaceholder.fill(Qt::lightGray);
    return unknownPlaceholder;
}
