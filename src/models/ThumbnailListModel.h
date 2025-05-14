#ifndef THUMBNAILLISTMODEL_H
#define THUMBNAILLISTMODEL_H

#include <QAbstractListModel>
#include <QStringList>
#include <QPixmapCache>
#include <QSize>
#include <QIcon>

// Forward declaration if ThumbnailLoader is a separate class
class ThumbnailLoader; 

class ThumbnailListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit ThumbnailListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setFilePaths(const QStringList &paths);
    QString filePathAt(int row) const;
    void clear();

    void setThumbnailLoader(ThumbnailLoader *loader); 
    void setThumbnailSize(const QSize &size);
    bool isThumbnailLoaded(int row) const; 
    void clearCache(); // New method to clear cached thumbnails

public slots:
    void onThumbnailReady(int row, const QIcon &thumbnail); 

private:
    QStringList m_filePaths;
    QList<QIcon> m_thumbnails; // Cache for loaded thumbnails (or use QPixmapCache)
    QIcon m_placeholderIcon;
    QSize m_thumbnailSize;
    mutable ThumbnailLoader *m_thumbnailLoader; // Made mutable
};

#endif // THUMBNAILLISTMODEL_H
