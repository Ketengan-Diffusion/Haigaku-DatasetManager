#include "ThumbnailDelegate.h"
#include <QPainter>
#include <QApplication> // For style and font metrics
#include <QStyle>
#include <QTextOption>
#include <QDebug>

ThumbnailDelegate::ThumbnailDelegate(const QSize& iconTargetSize, QObject *parent)
    : QStyledItemDelegate(parent), m_iconTargetDisplaySize(iconTargetSize), m_spacing(5)
{
}

void ThumbnailDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    
    QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

    QVariant decorationData = index.data(Qt::DecorationRole);
    QPixmap pixmap = decorationData.value<QPixmap>(); 
    // qDebug() << "ThumbnailDelegate::paint for row" << index.row() << "Variant type:" << decorationData.typeName() << "Pixmap isNull:" << pixmap.isNull() << "Size:" << pixmap.size();
    QString text = index.data(Qt::DisplayRole).toString();

    // --- Icon Drawing (Thumbnail) ---
    // The pixmap from DecorationRole is already scaled to fit m_iconTargetDisplaySize by the model.
    // We need to place it centered horizontally within the item (opt.rect).
    // The vertical position will be at the top, respecting some spacing.
    
    QRect iconActualRect(0, 0, pixmap.width(), pixmap.height());
    if (pixmap.isNull()) { // If pixmap is null, use target display size for placeholder
        iconActualRect.setSize(m_iconTargetDisplaySize);
    }

    // Center the icon's actual rectangle horizontally within opt.rect
    iconActualRect.moveLeft(opt.rect.x() + (opt.rect.width() - iconActualRect.width()) / 2);
    iconActualRect.moveTop(opt.rect.y() + m_spacing); // Add top spacing

    if (!pixmap.isNull()) {
        painter->drawPixmap(iconActualRect.topLeft(), pixmap);
    } else {
        painter->fillRect(iconActualRect, Qt::lightGray); // Placeholder
    }

    // --- Text Drawing (Filename) ---
    // Define the rectangle for text drawing, ensuring it's below the icon
    // and uses the full available width of the item for centering and wrapping.
    QRect textDrawRect(opt.rect.left() + m_spacing,
                       iconActualRect.bottom() + m_spacing,
                       opt.rect.width() - 2 * m_spacing,
                       opt.rect.bottom() - (iconActualRect.bottom() + m_spacing) - m_spacing);


    if (!text.isEmpty() && textDrawRect.height() > 0) {
        QTextOption textOption;
        textOption.setAlignment(Qt::AlignHCenter | Qt::AlignTop); 
        textOption.setWrapMode(QTextOption::WordWrap);      

        if (opt.state & QStyle::State_Selected) {
            painter->setPen(opt.palette.highlightedText().color());
        } else {
            painter->setPen(opt.palette.text().color());
        }
        painter->setFont(opt.font); 
        painter->drawText(textDrawRect, text, textOption);
    }

    painter->restore();
}

QSize ThumbnailDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // Use the m_iconTargetDisplaySize for width hint, as items will flow.
    // Height includes icon, spacing, and text.
    int itemWidth = m_iconTargetDisplaySize.width() + 2 * m_spacing; 
    int itemHeight = m_iconTargetDisplaySize.height() + 2 * m_spacing; // Icon + top/bottom spacing for icon part

    QString text = index.data(Qt::DisplayRole).toString();
    if (!text.isEmpty()) {
        QFontMetrics fm(option.font); // Use font from style option for accuracy
        // Calculate height for wrapped text, assuming text width is constrained by icon width
        QRect textBoundingRect = fm.boundingRect(QRect(0, 0, m_iconTargetDisplaySize.width(), 0), Qt::AlignHCenter | Qt::TextWordWrap, text);
        itemHeight += textBoundingRect.height() + m_spacing; // Add text height and spacing below text
    } else {
        itemHeight += m_spacing; // Just bottom spacing if no text
    }
    
    // The QListView in IconMode with wrapping will determine the actual number of columns.
    // The width hint here is more about the preferred width of a single item if it had infinite horizontal space.
    // The height hint is more critical.
    return QSize(itemWidth, itemHeight);
}
