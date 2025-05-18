#include "TagEditorWidget.h"
#include "TagPillWidget.h"
#include "utils/QFlowLayout.h"

#include <QLineEdit>
#include <QCompleter>
#include <QStringListModel>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QKeyEvent>
#include <QDebug>
#include <QAbstractItemView>
#include <QTimer> 
#include <QDragEnterEvent> 
#include <QDropEvent>      
#include <QMimeData>       

TagEditorWidget::TagEditorWidget(QWidget *parent)
    : QWidget(parent),
      m_tagInputLineEdit(new QLineEdit(this)),
      m_tagCompleter(new QCompleter(this)),
      m_tagCompletionModel(new QStringListModel(this)),
      m_tagDisplayArea(new QWidget(this)),
      m_flowLayout(new QFlowLayout(m_tagDisplayArea, 2, 3, 3)),
      m_scrollArea(new QScrollArea(this)),
      m_storeTagsWithUnderscores(false),
      m_completionTimer(new QTimer(this)) 
{
    m_completionTimer->setSingleShot(true);
    m_completionTimer->setInterval(150); 
    connect(m_completionTimer, &QTimer::timeout, this, [this]() {
        if (m_tagCompleter && m_tagInputLineEdit) {
            if (m_tagCompleter->widget() != m_tagInputLineEdit) {
                m_tagCompleter->setWidget(m_tagInputLineEdit);
            }
            QString currentPrefix = m_tagInputLineEdit->text().trimmed();
            if (!currentPrefix.isEmpty()) {
                m_tagCompleter->setCompletionPrefix(currentPrefix);
                m_tagCompleter->complete();
            } else {
                if (m_tagCompleter->popup()) m_tagCompleter->popup()->hide();
            }
        }
    });

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);
    mainLayout->setSpacing(3);

    m_tagDisplayArea->setLayout(m_flowLayout);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setWidget(m_tagDisplayArea);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); 
    m_scrollArea->setStyleSheet("QScrollArea { border: 1px solid palette(mid); border-radius: 3px; background-color: palette(base); }");
    
    m_scrollArea->setAcceptDrops(true); 
    setAcceptDrops(true);


    mainLayout->addWidget(m_scrollArea, 1); 

    m_tagInputLineEdit->setPlaceholderText(tr("Add tag..."));
    connect(m_tagInputLineEdit, &QLineEdit::returnPressed, this, &TagEditorWidget::onTagInputReturnPressed);
    connect(m_tagInputLineEdit, &QLineEdit::textChanged, this, &TagEditorWidget::onTagInputTextChanged);
    mainLayout->addWidget(m_tagInputLineEdit);

    m_tagCompleter->setModel(m_tagCompletionModel);
    m_tagCompleter->setWidget(m_tagInputLineEdit); 
    m_tagCompleter->setCompletionMode(QCompleter::PopupCompletion);
    m_tagCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_tagCompleter->setFilterMode(Qt::MatchContains); 
    connect(m_tagCompleter, QOverload<const QString &>::of(&QCompleter::activated),
            this, &TagEditorWidget::onTagCompletionActivated);
    
    setLayout(mainLayout);
}

TagEditorWidget::~TagEditorWidget() {} 

QStringList TagEditorWidget::getTags(bool underscoreFormat) const
{
    QStringList result;
    bool useUnderscores = underscoreFormat || m_storeTagsWithUnderscores;
    for (const QString &tag : qAsConst(m_tagsInternal)) {
        result.append(useUnderscores ? formatTagForStorage(tag) : tag);
    }
    return result;
}

void TagEditorWidget::setTags(const QStringList &tags, bool inputHasUnderscores)
{
    clear(); 
    for (const QString &tag : tags) {
        QString processedTag;
        if (inputHasUnderscores) {
            processedTag = formatTagForDisplay(tag).trimmed(); 
        } else {
            processedTag = tag.trimmed(); 
        }
        if (!processedTag.isEmpty() && !m_tagsInternal.contains(processedTag, Qt::CaseInsensitive)) {
            m_tagsInternal.append(processedTag);
        }
    }
    updateDisplayedTags();
    emit tagsChanged(); 
}

void TagEditorWidget::setKnownTagsVocabulary(const QStringList &vocabularyWithUnderscores)
{
    m_knownTagsWithUnderscores = vocabularyWithUnderscores;
    QStringList displayVocabulary;
    for (const QString &tag : m_knownTagsWithUnderscores) {
        displayVocabulary.append(formatTagForDisplay(tag));
    }
    m_tagCompletionModel->setStringList(displayVocabulary);
}

void TagEditorWidget::clear()
{
    while (QLayoutItem* item = m_flowLayout->takeAt(0)) {
        delete item->widget(); 
        delete item;
    }
    m_tagsInternal.clear();
    m_tagInputLineEdit->clear();
    emit tagsChanged(); 
}

void TagEditorWidget::setStoreTagsWithUnderscores(bool storeWithUnderscores)
{
    m_storeTagsWithUnderscores = storeWithUnderscores;
}

void TagEditorWidget::onTagInputReturnPressed()
{
    if (m_tagCompleter->widget() != m_tagInputLineEdit) { 
        m_tagCompleter->setWidget(m_tagInputLineEdit);
    }
    QString tagText = m_tagInputLineEdit->text().trimmed();
    if (!tagText.isEmpty()) {
        addTagInternal(tagText); 
        m_tagInputLineEdit->clear();
    }
}

void TagEditorWidget::onTagCompletionActivated(const QString &tagTextFromCompleter)
{
    if (m_tagCompleter->widget() == m_tagInputLineEdit) {
        addTagInternal(tagTextFromCompleter.trimmed());
        m_tagInputLineEdit->clear();
    } else if (QLineEdit* pillLineEdit = qobject_cast<QLineEdit*>(m_tagCompleter->widget())) {
        TagPillWidget* pillWidget = qobject_cast<TagPillWidget*>(pillLineEdit->parentWidget());
        if (pillWidget) {
            pillLineEdit->setText(tagTextFromCompleter); 
            pillWidget->finishEditing(); 
        }
    }
}

void TagEditorWidget::onPillCloseButtonClicked(TagPillWidget *pill)
{
    if (pill) {
        QString tagToRemove = pill->text().trimmed(); 
        bool removed = false;
        for(int i=0; i < m_tagsInternal.size(); ++i) {
            if(m_tagsInternal.at(i).compare(tagToRemove, Qt::CaseSensitive) == 0) { 
                m_tagsInternal.removeAt(i);
                removed = true;
                break;
            }
        }
        if(!removed) { 
             for (int i = m_tagsInternal.size() - 1; i >= 0; --i) {
                if (m_tagsInternal.at(i).compare(tagToRemove, Qt::CaseInsensitive) == 0) {
                    m_tagsInternal.removeAt(i);
                    removed = true; 
                }
            }
        }
        removeTagPill(pill);
        if (removed) emit tagsChanged(); 
    }
}

void TagEditorWidget::onTagInputTextChanged(const QString &text)
{
    Q_UNUSED(text); 
    if (!m_tagCompleter || !m_completionTimer || !m_tagInputLineEdit) return;

    if (m_tagCompleter->widget() != m_tagInputLineEdit) {
        m_tagCompleter->setWidget(m_tagInputLineEdit);
    }
    m_completionTimer->start(); 
}

void TagEditorWidget::onPillTagEdited(TagPillWidget *pill, const QString &oldTextFromPill, const QString &newTextFromPill)
{
    if (!pill) {
        m_tagInputLineEdit->clear();
        return;
    }

    if (newTextFromPill.isEmpty()) {
        onPillCloseButtonClicked(pill); 
        m_tagInputLineEdit->clear();
        return;
    }

    if (oldTextFromPill.compare(newTextFromPill, Qt::CaseSensitive) == 0) {
        m_tagInputLineEdit->clear();
        return;
    }

    int foundOldTextIndex = -1;
    QString actualTextInListMatchingOld;
    for (int i = 0; i < m_tagsInternal.size(); ++i) {
        if (m_tagsInternal.at(i).compare(oldTextFromPill, Qt::CaseInsensitive) == 0) {
            foundOldTextIndex = i;
            actualTextInListMatchingOld = m_tagsInternal.at(i);
            break;
        }
    }

    if (foundOldTextIndex == -1) {
        pill->setText(oldTextFromPill); 
        m_tagInputLineEdit->clear();
        return;
    }

    bool newTextIsDuplicateOfAnother = false;
    for (int i = 0; i < m_tagsInternal.size(); ++i) {
        if (i == foundOldTextIndex) { 
            continue;
        }
        if (m_tagsInternal.at(i).compare(newTextFromPill, Qt::CaseInsensitive) == 0) {
            newTextIsDuplicateOfAnother = true;
            break;
        }
    }

    if (newTextIsDuplicateOfAnother) {
        pill->setText(actualTextInListMatchingOld); 
    } else {
        m_tagsInternal[foundOldTextIndex] = newTextFromPill; 
        emit tagsChanged(); 
    }

    m_tagInputLineEdit->clear(); 
}


void TagEditorWidget::addTagInternal(const QString &tagTextWithSpaces)
{
    QString cleanTag = tagTextWithSpaces.trimmed();
    if (cleanTag.isEmpty()) return;

    for (const QString &existingTag : qAsConst(m_tagsInternal)) {
        if (existingTag.compare(cleanTag, Qt::CaseInsensitive) == 0) {
            return; 
        }
    }
    
    m_tagsInternal.append(cleanTag); 
    TagPillWidget *pill = new TagPillWidget(cleanTag, m_tagCompleter, m_tagDisplayArea); 
    connect(pill, &TagPillWidget::closeClicked, this, &TagEditorWidget::onPillCloseButtonClicked);
    connect(pill, &TagPillWidget::tagEdited, this, &TagEditorWidget::onPillTagEdited); 
    m_flowLayout->addWidget(pill);
    emit tagsChanged();
}

void TagEditorWidget::removeTagPill(TagPillWidget *pillWidget)
{
    m_flowLayout->removeWidget(pillWidget);
    pillWidget->deleteLater();
}

void TagEditorWidget::updateDisplayedTags()
{
    if (m_tagCompleter && m_tagInputLineEdit) {
         m_tagCompleter->setWidget(m_tagInputLineEdit);
    }

    while (QLayoutItem* item = m_flowLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (const QString &tagText : qAsConst(m_tagsInternal)) {
        TagPillWidget *pill = new TagPillWidget(tagText, m_tagCompleter, m_tagDisplayArea); 
        connect(pill, &TagPillWidget::closeClicked, this, &TagEditorWidget::onPillCloseButtonClicked);
        connect(pill, &TagPillWidget::tagEdited, this, &TagEditorWidget::onPillTagEdited); 
        m_flowLayout->addWidget(pill);
    }
}

QString TagEditorWidget::formatTagForDisplay(const QString &tagWithUnderscores) const
{
    return QString(tagWithUnderscores).replace('_', ' ');
}

QString TagEditorWidget::formatTagForStorage(const QString &tagWithSpaces) const
{
    return QString(tagWithSpaces).replace(' ', '_');
}

void TagEditorWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasText()) {
        TagPillWidget *sourcePill = qobject_cast<TagPillWidget*>(event->source());
        if (sourcePill && sourcePill->parentWidget() == m_tagDisplayArea) {
            event->setDropAction(Qt::MoveAction);
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->ignore();
    }
}

void TagEditorWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasText()) {
        TagPillWidget *sourcePill = qobject_cast<TagPillWidget*>(event->source());
         if (sourcePill && sourcePill->parentWidget() == m_tagDisplayArea) {
            event->setDropAction(Qt::MoveAction);
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->ignore();
    }
}

void TagEditorWidget::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasText()) {
        TagPillWidget *sourcePill = qobject_cast<TagPillWidget*>(event->source());
        if (sourcePill && sourcePill->parentWidget() == m_tagDisplayArea) {
            QString droppedTagText = event->mimeData()->text();
            
            int fromIndex = -1;
            for(int i=0; i < m_tagsInternal.size(); ++i) {
                if (m_tagsInternal.at(i).compare(droppedTagText, Qt::CaseInsensitive) == 0) {
                    fromIndex = i;
                    droppedTagText = m_tagsInternal.at(i); // Use the actual cased string from list
                    break;
                }
            }

            if (fromIndex == -1) {
                qWarning() << "Drop event: Could not find dragged tag [" << droppedTagText << "] in internal list.";
                event->ignore();
                return;
            }
            
            QPoint displayAreaDropPos = m_tagDisplayArea->mapFrom(this, event->position().toPoint());
            int toIndex = m_tagsInternal.count(); // Default to end if not dropped on/before another pill

            for (int i = 0; i < m_flowLayout->count(); ++i) {
                QWidget *widget = m_flowLayout->itemAt(i)->widget();
                if (!widget || widget == sourcePill) continue; 

                QRect widgetRect = widget->geometry();
                if (widgetRect.contains(displayAreaDropPos)) { // Dropped onto a pill
                    // Insert before if dropped on left half, after if on right half
                    toIndex = (displayAreaDropPos.x() < widgetRect.center().x()) ? i : i + 1;
                    break; 
                } else if (displayAreaDropPos.y() < widgetRect.bottom()) { 
                    // If on the same visual "row" (or above) and to the left of a pill
                    if (displayAreaDropPos.x() < widgetRect.left()) {
                         toIndex = i;
                         break;
                    }
                }
            }
            
            if (fromIndex == toIndex || (fromIndex == toIndex -1 && toIndex > fromIndex) ) { 
                 event->acceptProposedAction(); 
                 if(sourcePill) sourcePill->show(); 
                 return;
            }
            
            QString tagToMove = m_tagsInternal.takeAt(fromIndex);
            
            if (toIndex > fromIndex) {
                toIndex--; 
            }
            if (toIndex < 0) toIndex = 0; 
            if (toIndex > m_tagsInternal.count()) toIndex = m_tagsInternal.count();

            m_tagsInternal.insert(toIndex, tagToMove);
            
            updateDisplayedTags(); 
            emit tagsChanged();
            event->acceptProposedAction(); 
            return;
        }
    }
    event->ignore();
}
