#ifndef TAGEDITORWIDGET_H
#define TAGEDITORWIDGET_H

#include <QWidget>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QCompleter;
class QStringListModel;
class QScrollArea;
class QTimer; // Added for delayed completion
QT_END_NAMESPACE

class QFlowLayout; 
class TagPillWidget; 

class TagEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TagEditorWidget(QWidget *parent = nullptr);
    ~TagEditorWidget() override;

    QStringList getTags(bool underscoreFormat = false) const; 
    void setTags(const QStringList &tags, bool inputHasUnderscores = false); 
    void setKnownTagsVocabulary(const QStringList &vocabularyWithUnderscores);
    void clear();
    void setStoreTagsWithUnderscores(bool storeWithUnderscores); 
    QLineEdit* mainInputLineEdit() const { return m_tagInputLineEdit; } // Getter

signals:
    void tagsChanged(); 

private slots:
    void onTagInputReturnPressed();
    void onTagCompletionActivated(const QString &tagText); 
    void onPillCloseButtonClicked(TagPillWidget *pill);
    void onTagInputTextChanged(const QString &text);
    // Removed onPillDoubleClicked, will be replaced by onPillTagEdited
    void onPillTagEdited(TagPillWidget *pill, const QString &oldText, const QString &newText); // For in-place edit

private:
    void addTagInternal(const QString &tagTextWithSpaces);
    void removeTagPill(TagPillWidget *pillWidget);
    void updateDisplayedTags();
    QString formatTagForDisplay(const QString &tagWithUnderscores) const;
    QString formatTagForStorage(const QString &tagWithSpaces) const;

    QList<QString> m_tagsInternal; 
    
    QLineEdit *m_tagInputLineEdit;
    QCompleter *m_tagCompleter;
    QStringListModel *m_tagCompletionModel; 
    
    QWidget *m_tagDisplayArea;      
    QFlowLayout *m_flowLayout;      
    QScrollArea *m_scrollArea;      

    QStringList m_knownTagsWithUnderscores; 
    bool m_storeTagsWithUnderscores; 
    QTimer *m_completionTimer; 

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};

#endif // TAGEDITORWIDGET_H
