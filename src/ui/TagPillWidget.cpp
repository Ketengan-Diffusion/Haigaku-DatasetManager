#include "TagPillWidget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit> 
#include <QStyle> 
#include <QMouseEvent> 
#include <QFocusEvent> 
#include <QCompleter> 
#include <QDebug>     
#include <QApplication>       
#include <QAbstractItemView>  
#include <QKeyEvent> 
#include <QTimer> 
#include <QDrag>      // Added for QDrag
#include <QMimeData>  // Added for QMimeData
#include <QPainter>   // For rendering pixmap for drag

TagPillWidget::TagPillWidget(const QString &text, QCompleter *completer, QWidget *parent) 
    : QWidget(parent), m_isEditing(false), m_completer(completer), m_pillCompletionTimer(new QTimer(this)) 
{
    m_tagText = text.trimmed(); 

    m_pillCompletionTimer->setSingleShot(true);
    m_pillCompletionTimer->setInterval(100); 
    connect(m_pillCompletionTimer, &QTimer::timeout, this, [this]() {
        if (m_isEditing && m_completer && m_editLineEdit && m_completer->widget() == m_editLineEdit) { 
            QString currentPrefix = m_editLineEdit->text().trimmed();
            if (!currentPrefix.isEmpty()) {
                m_completer->setCompletionPrefix(currentPrefix); 
                m_completer->complete(); 
            } else {
                if (m_completer->popup()) m_completer->popup()->hide();
            }
        }
    });

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2); 
    layout->setSpacing(4);

    m_textLabel = new QLabel(m_tagText, this); 
    
    m_editLineEdit = new QLineEdit(m_tagText, this); 
    m_editLineEdit->setFrame(false); 
    m_editLineEdit->setVisible(false); 
    QFont labelFont = m_textLabel->font();
    m_editLineEdit->setFont(labelFont);
    m_editLineEdit->setFixedHeight(m_textLabel->sizeHint().height());
    connect(m_editLineEdit, &QLineEdit::textChanged, this, [this](const QString &text){
        Q_UNUSED(text);
        if(m_isEditing && m_pillCompletionTimer) m_pillCompletionTimer->start();
    });

    layout->addWidget(m_textLabel);
    layout->addWidget(m_editLineEdit); 

    m_closeButton = new QPushButton(this);
    m_closeButton->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    m_closeButton->setFixedSize(m_textLabel->fontMetrics().height() -2 , m_textLabel->fontMetrics().height() - 2); 
    m_closeButton->setIconSize(QSize(m_textLabel->fontMetrics().height() / 2, m_textLabel->fontMetrics().height() / 2)); 
    m_closeButton->setStyleSheet("QPushButton { border: none; padding: 0px; margin: 0px; background-color: transparent; }"); 
    m_closeButton->setFlat(true); 
    m_closeButton->setToolTip(tr("Remove tag"));
    connect(m_closeButton, &QPushButton::clicked, this, &TagPillWidget::onCloseButtonClicked);
    layout->addWidget(m_closeButton);
    
    m_editLineEdit->installEventFilter(this); 

    setStyleSheet("TagPillWidget { background-color: palette(alternate-base); border: 1px solid palette(mid); border-radius: 8px; padding: 2px; }");
    
    setLayout(layout);
    adjustSize(); 
    setFixedHeight(sizeHint().height()); 
}

QString TagPillWidget::text() const
{
    return m_tagText; 
}

void TagPillWidget::setText(const QString &text)
{
    m_tagText = text.trimmed();
    m_textLabel->setText(m_tagText);
    if (m_isEditing) { 
        m_editLineEdit->setText(m_tagText);
    }
}

void TagPillWidget::onCloseButtonClicked()
{
    emit closeClicked(this);
}

void TagPillWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_isEditing) {
        switchToEditMode();
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TagPillWidget::switchToEditMode() {
    if (m_isEditing) return;
    m_isEditing = true;
    // qDebug() << "TagPillWidget [" << m_tagText << "] switchToEditMode";

    m_textLabel->setVisible(false);
    m_editLineEdit->setText(m_tagText); 
    m_editLineEdit->setVisible(true);
    m_editLineEdit->selectAll();
    
    if (m_completer) { 
        m_completer->setWidget(m_editLineEdit); 
    }
    m_editLineEdit->setFocus(Qt::MouseFocusReason); 
}

void TagPillWidget::finishEditing() {
    if (!m_isEditing) return; 
    
    QString oldText = m_tagText; 
    QString newText = m_editLineEdit->text().trimmed(); 

    // qDebug() << "TagPillWidget [" << oldText << "] finishEditing. New text: [" << newText << "]";
    
    m_isEditing = false;      
    m_editLineEdit->setVisible(false);
    m_textLabel->setVisible(true);
    if(m_pillCompletionTimer) m_pillCompletionTimer->stop(); 

    if (m_completer && m_completer->widget() == m_editLineEdit) {
        m_completer->setWidget(nullptr); 
    }

    if (newText.isEmpty()) { 
        emit closeClicked(this); 
        return;
    }

    if (oldText.compare(newText, Qt::CaseSensitive) != 0) {
        QString originalOldTextForSignal = oldText; 
        m_tagText = newText;                        
        m_textLabel->setText(m_tagText);            
        emit tagEdited(this, originalOldTextForSignal, newText); 
    } else {
        m_textLabel->setText(m_tagText);
    }
}

bool TagPillWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_editLineEdit && m_isEditing) { 
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                finishEditing();
                return true; 
            } else if (keyEvent->key() == Qt::Key_Escape) {
                m_editLineEdit->setText(m_tagText); 
                m_editLineEdit->setVisible(false);
                m_textLabel->setVisible(true);
                m_isEditing = false;
                if(m_pillCompletionTimer) m_pillCompletionTimer->stop();
                if (m_completer && m_completer->widget() == m_editLineEdit) {
                    m_completer->setWidget(nullptr);
                }
                return true; 
            }
        } else if (event->type() == QEvent::FocusOut) {
            QFocusEvent *focusEvent = static_cast<QFocusEvent*>(event);
            if (focusEvent->reason() == Qt::PopupFocusReason) {
                // Do nothing special if focus goes to completer popup
            } else {
                finishEditing(); // Finish if focus lost to something else
            }
             // Let QLineEdit handle the FocusOut event further if needed,
             // unless we are certain we've done all necessary.
             // For now, don't consume (return false) to be safe.
             // However, if finishEditing was called, it might be better to consume.
             // Let's try consuming if we called finishEditing due to non-popup focus out.
            if (focusEvent->reason() != Qt::PopupFocusReason) return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TagPillWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_isEditing) { // Don't drag if editing
        m_dragStartPosition = event->pos();
    }
    QWidget::mousePressEvent(event);
}

void TagPillWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton) || m_isEditing) { // Don't drag if editing
        QWidget::mouseMoveEvent(event);
        return;
    }
    if ((event->pos() - m_dragStartPosition).manhattanLength() < QApplication::startDragDistance()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;

    // Set data: the tag text itself.
    // The drop target (TagEditorWidget) will use this to identify the tag.
    mimeData->setText(m_tagText);
    drag->setMimeData(mimeData);

    // Create a pixmap for the drag cursor
    QPixmap pixmap(size());
    pixmap.fill(Qt::transparent); // Or render the widget to it for a "ghost"
    QPainter painter(&pixmap);
    render(&painter); // Render current look of the pill
    painter.end();
    drag->setPixmap(pixmap);
    drag->setHotSpot(event->pos()); // Drag from where the mouse is

    // Hide the original widget while dragging
    // this->hide(); // Or set opacity, or let drop target handle visual feedback

    // Start the drag
    // Qt::MoveAction indicates the data is moved, not copied.
    Qt::DropAction dropAction = drag->exec(Qt::MoveAction);

    if (dropAction == Qt::MoveAction) {
        // The drop was accepted and data was "moved".
        // The TagEditorWidget is responsible for reordering its internal list
        // and calling updateDisplayedTags(), which will delete old pills and create new ones.
        // So, this original pill instance will be deleted by that process.
        // We don't need to explicitly delete it here, but we also shouldn't show it again
        // if it was hidden. For now, we didn't hide it.
        // If TagEditorWidget does NOT delete and recreate, then this pill needs to be deleted.
        // To be safe, if the action was a move, let's hide this instance,
        // assuming TagEditorWidget will handle the new state.
        // A more robust way is for TagEditorWidget to delete the sourcePill widget directly.
        // For now, let's assume updateDisplayedTags in TagEditorWidget handles it.
    } else {
        // Drag was cancelled or ignored, ensure this pill is visible if it was hidden.
        // (We are not hiding it currently during drag).
    }
    
    // QWidget::mouseMoveEvent(event); // Not needed as QDrag takes over mouse events
}
