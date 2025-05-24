#ifndef WORDCLOUDWIDGET_H
#define WORDCLOUDWIDGET_H

#include <QWidget>
#include <QMap>
#include <QString>
#include <QList>
#include <QRectF>
#include <QColor>

QT_BEGIN_NAMESPACE
class QPaintEvent;
class QMouseEvent;
class QEvent;
class QWheelEvent; // Added for wheel zoom
QT_END_NAMESPACE

class WordCloudWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WordCloudWidget(QWidget *parent = nullptr);
    ~WordCloudWidget() override;

    void setWordData(const QMap<QString, int>& frequencies);

public slots:
    void zoomIn();
    void zoomOut();
    void setZoomFactor(double factor);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override; 
    void wheelEvent(QWheelEvent *event) override; // Added for wheel zoom

private:
    struct WordItem {
        QString text;
        int count;
        QRectF rect;
        QFont font;
        QColor color;
    };

    void prepareWordItems(); // Helper to process frequencies into drawable items
    bool tryPlaceWord(WordItem &item, const QList<WordItem>& placedItems); // Placement algorithm helper

    QMap<QString, int> m_sourceFrequencies;
    QList<WordItem> m_drawableWords; // Words successfully placed

    int m_hoveredWordIndex;

    QColor m_defaultColor;
    QColor m_hoverColor;
    QString m_fontFamily;
    int m_maxWords;
    int m_minFontSize;
    int m_maxFontSize;

    // For placement strategy
    QSize m_lastWidgetSize;
    double m_zoomFactor;
};

#endif // WORDCLOUDWIDGET_H
