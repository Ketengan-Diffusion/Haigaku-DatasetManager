#include "ThumbnailListModel.h"
#include "ThumbnailLoader.h" // Will need this once ThumbnailLoader requests thumbnails
#include <QFileInfo>
#include <QPixmap>
#include <QPainter> // For placeholder icon drawing if needed

ThumbnailListModel::ThumbnailListModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_thumbnailLoader(nullptr)
    , m_thumbnailSize(180, 100) // Default, can be changed
{
    // Create a default placeholder icon
    QPixmap placeholderPixmap(m_thumbnailSize);
    placeholderPixmap.fill(Qt::gray); // Or some other neutral color
    // Optionally draw something on it
    // QPainter p(&placeholderPixmap);
    // p.drawText(placeholderPixmap.rect(), Qt::AlignCenter, "?");
    // p.end();
    m_placeholderIcon = QIcon(placeholderPixmap);
}

int ThumbnailListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_filePaths.count();
}

QVariant ThumbnailListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_filePaths.count())
        return QVariant();

    const QString &filePath = m_filePaths.at(index.row());

    if (role == Qt::DisplayRole) {
        return QFileInfo(filePath).fileName();
    } else if (role == Qt::DecorationRole) {
        if (index.row() < m_thumbnails.count() && !m_thumbnails.at(index.row()).isNull()) {
            return m_thumbnails.at(index.row());
        } else {
            // Thumbnail not loaded or not ready yet.
            // The MainWindow will now be responsible for requesting thumbnails based on visibility.
            // This method just returns the placeholder if not already cached.
            return m_placeholderIcon;
        }
    }
    return QVariant();
}

void ThumbnailListModel::setFilePaths(const QStringList &paths)
{
    beginResetModel();
    m_filePaths = paths;
    m_thumbnails.clear();
    m_thumbnails.resize(m_filePaths.count()); // Resize to hold icons, initially null
    endResetModel();

    // Do NOT request any thumbnails here. MainWindow will handle it.
}

QString ThumbnailListModel::filePathAt(int row) const
{
    if (row >= 0 && row < m_filePaths.count()) {
        return m_filePaths.at(row);
    }
    return QString();
}

void ThumbnailListModel::clear()
{
    beginResetModel();
    m_filePaths.clear();
    m_thumbnails.clear();
    endResetModel();
    if (m_thumbnailLoader) {
        m_thumbnailLoader->clearQueue();
    }
}

void ThumbnailListModel::clearCache()
{
    if (m_filePaths.isEmpty()) {
        return;
    }
    // To effectively clear the cache and trigger re-fetch,
    // we can re-initialize m_thumbnails to a list of null QIcons
    // of the same size as m_filePaths.
    // Then emit dataChanged for all items to make the view re-query.
    
    // Simpler: just clear and resize. data() will request if null.
    beginResetModel(); // This is heavy but ensures view updates correctly.
    m_thumbnails.clear();
    m_thumbnails.resize(m_filePaths.count()); // Fill with null QIcons
    endResetModel();

    // Alternative: emit dataChanged for all rows if beginResetModel is too disruptive
    // if (!m_filePaths.isEmpty()) {
    //     m_thumbnails.clear();
    //     m_thumbnails.resize(m_filePaths.count());
    //     emit dataChanged(index(0, 0), index(m_filePaths.count() - 1, 0), {Qt::DecorationRole});
    // }
}

void ThumbnailListModel::setThumbnailLoader(ThumbnailLoader *loader)
{
    m_thumbnailLoader = loader;
    if (m_thumbnailLoader) {
        // Connect the loader's signal to our slot
        connect(m_thumbnailLoader, &ThumbnailLoader::thumbnailReady, this, &ThumbnailListModel::onThumbnailReady);
    }
}

void ThumbnailListModel::setThumbnailSize(const QSize &size)
{
    m_thumbnailSize = size;
    // Recreate placeholder with new size
    QPixmap placeholderPixmap(m_thumbnailSize);
    placeholderPixmap.fill(Qt::gray);
    m_placeholderIcon = QIcon(placeholderPixmap);
    // Existing thumbnails might need to be re-requested or re-scaled if size changes significantly.
    // For simplicity, we'll assume size is set once.
}

bool ThumbnailListModel::isThumbnailLoaded(int row) const
{
    if (row >= 0 && row < m_thumbnails.count()) {
        // A thumbnail is considered "loaded" if it's not null AND not the placeholder.
        // This assumes m_placeholderIcon is distinct from any validly loaded (even if blank) icon.
        // If m_thumbnails can contain null QIcons for unloaded items, then !m_thumbnails.at(row).isNull() is enough.
        // Since we initialize m_thumbnails with default-constructed QIcons (which are null),
        // checking for !isNull is the correct way.
        return !m_thumbnails.at(row).isNull();
    }
    return false; // Or true if out of bounds should mean "nothing to load"
}

void ThumbnailListModel::onThumbnailReady(int row, const QIcon &thumbnail)
{
    if (row >= 0 && row < m_thumbnails.count()) {
        m_thumbnails[row] = thumbnail;
        QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx, {Qt::DecorationRole});
    }
}
