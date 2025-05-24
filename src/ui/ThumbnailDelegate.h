#ifndef THUMBNAILDELEGATE_H
#define THUMBNAILDELEGATE_H

#include <QStyledItemDelegate>
#include <QPixmap> // For QPixmap::fromImage in case we need it

class ThumbnailDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ThumbnailDelegate(const QSize& iconTargetSize, QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QSize m_iconTargetDisplaySize; // The target display area for the icon
    int m_spacing;  // Spacing between icon and text, and around item
};

#endif // THUMBNAILDELEGATE_H
