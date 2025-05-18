#ifndef TAGPILLWIDGET_H
#define TAGPILLWIDGET_H

#include <QWidget>
#include <QString>
#include <QCompleter> // For QCompleter

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QLineEdit; 
class QMouseEvent; 
class QEvent;      
class QTimer; // Added for delayed completion in pill
QT_END_NAMESPACE

class TagPillWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TagPillWidget(const QString &text, QCompleter *completer, QWidget *parent = nullptr); // Completer is for the line edit within this pill
    QString text() const;
    void setText(const QString &text); // This will set the display text

signals:
    void closeClicked(TagPillWidget *pill); 
    void tagEdited(TagPillWidget *pill, const QString &oldText, const QString &newText); // Emitted after in-place edit

public slots: // Made public so TagEditorWidget can call it
    void finishEditing();

private slots:
    void onCloseButtonClicked();
    void switchToEditMode();
    // void finishEditing(); // Moved to public slots

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override; 
    void mousePressEvent(QMouseEvent *event) override;    // For starting drag
    void mouseMoveEvent(QMouseEvent *event) override;     // For drag movement

private:
    QPoint m_dragStartPosition; // To track drag start
    QLabel *m_textLabel;
    QPushButton *m_closeButton;
    QLineEdit *m_editLineEdit; // For in-place editing
    QString m_tagText;         // Stores the current tag text (should be clean)
    bool m_isEditing;
    QCompleter *m_completer;   
    QTimer *m_pillCompletionTimer; // Timer for this pill's line edit completer
};

#endif // TAGPILLWIDGET_H
