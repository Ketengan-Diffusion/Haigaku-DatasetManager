#include "WordCloudWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent> 
#include <QToolTip>
#include <algorithm> 
#include <QtMath>    
#include <QDebug>    
#include <QRandomGenerator> 

// Helper struct for sorting
struct FrequencyItem {
    QString text;
    int count;
    bool operator<(const FrequencyItem& other) const {
        return count > other.count; // Sort descending by count
    }
};

WordCloudWidget::WordCloudWidget(QWidget *parent)
    : QWidget(parent),
      m_hoveredWordIndex(-1),
      m_defaultColor("#D19970"),
      m_hoverColor("#6DD9B8"),
      m_fontFamily("Arial"),
      m_maxWords(100),
      m_minFontSize(6), 
      m_maxFontSize(50),
      m_zoomFactor(1.0) 
{
    setMouseTracking(true);
}

WordCloudWidget::~WordCloudWidget()
{
}

void WordCloudWidget::setWordData(const QMap<QString, int>& frequencies)
{
    m_sourceFrequencies = frequencies;
    m_drawableWords.clear();
    m_hoveredWordIndex = -1;
    prepareWordItems(); 
    update(); 
}

void WordCloudWidget::zoomIn() {
    setZoomFactor(m_zoomFactor * 1.2);
}

void WordCloudWidget::zoomOut() {
    setZoomFactor(m_zoomFactor / 1.2);
}

void WordCloudWidget::setZoomFactor(double factor) {
    m_zoomFactor = qMax(0.05, qMin(factor, 10.0)); // Wider zoom range, min zoom makes things very small
    if (!m_sourceFrequencies.isEmpty()) {
        prepareWordItems();
        update();
    }
}

void WordCloudWidget::prepareWordItems()
{
    m_drawableWords.clear();
    if (m_sourceFrequencies.isEmpty() || width() <= 0 || height() <= 0) {
        m_lastWidgetSize = size();
        return;
    }
    m_lastWidgetSize = size();

    QList<FrequencyItem> sortedItems;
    for (auto it = m_sourceFrequencies.constBegin(); it != m_sourceFrequencies.constEnd(); ++it) {
        sortedItems.append({it.key(), it.value()});
    }
    std::sort(sortedItems.begin(), sortedItems.end());

    int itemsToProcess = qMin(m_maxWords, sortedItems.size());
    if (itemsToProcess == 0) return;

    int minFreq = sortedItems.last().count; 
    int maxFreq = sortedItems.first().count; 

    if (minFreq == maxFreq && itemsToProcess > 0) { 
        minFreq = qMax(1, maxFreq -1); 
    }
     if (minFreq <= 0 && maxFreq <= 0 && itemsToProcess > 0) { 
        minFreq = 1; maxFreq = 2; 
    } else if (minFreq <= 0) { 
        minFreq = 1;
    }
    if (maxFreq < minFreq) maxFreq = minFreq +1; 


    double currentOverallMinFontSize = static_cast<double>(m_minFontSize) * m_zoomFactor;
    double currentOverallMaxFontSize = static_cast<double>(m_maxFontSize) * m_zoomFactor;
    
    // Ensure min font size is at least 1 for visibility, max is at least min+1
    currentOverallMinFontSize = qMax(1.0, currentOverallMinFontSize);
    currentOverallMaxFontSize = qMax(currentOverallMinFontSize + 1.0, currentOverallMaxFontSize);


    int dynamicMaxFontSize = qMax(static_cast<int>(currentOverallMinFontSize + 4), static_cast<int>(static_cast<double>(height()) / 10.0 /* * m_zoomFactor - this might be double applying zoom */) );
    dynamicMaxFontSize = qMin(dynamicMaxFontSize, static_cast<int>(currentOverallMaxFontSize));   
    
    int dynamicMinFontSize = qMax(static_cast<int>(currentOverallMinFontSize), dynamicMaxFontSize / 4); 
    if (dynamicMaxFontSize > currentOverallMinFontSize) {
        dynamicMinFontSize = qMin(dynamicMinFontSize, dynamicMaxFontSize - 1);
    } else { 
        dynamicMinFontSize = static_cast<int>(currentOverallMinFontSize);
    }
    dynamicMinFontSize = qMax(static_cast<int>(currentOverallMinFontSize), dynamicMinFontSize); 
    dynamicMinFontSize = qMax(1, dynamicMinFontSize); 
    dynamicMaxFontSize = qMax(dynamicMinFontSize +1, dynamicMaxFontSize); 


    QList<WordItem> successfullyPlacedWords;

    for (int i = 0; i < itemsToProcess; ++i) {
        const auto& freqItem = sortedItems.at(i);
        WordItem currentWord;
        currentWord.text = freqItem.text;
        currentWord.count = freqItem.count;
        
        double relativeFreq = 0.0;
        if (maxFreq > 0) { // Avoid division by zero if maxFreq is 0 (e.g. no tags)
            relativeFreq = static_cast<double>(freqItem.count) / static_cast<double>(maxFreq);
        }

        // Apply a power scaling (e.g., sqrt) to the relative frequency to enhance visual differences
        // An exponent < 1 will boost lower values, > 1 will compress lower values.
        // Let's try 0.5 (sqrt) or 0.7 for a start.
        double scalingExponent = 0.6; 
        double scaledRelativeFreq = qPow(relativeFreq, scalingExponent);
        
        int fontSize = dynamicMinFontSize + static_cast<int>(scaledRelativeFreq * (dynamicMaxFontSize - dynamicMinFontSize));
        fontSize = qMax(dynamicMinFontSize, qMin(fontSize, dynamicMaxFontSize)); 
        
        currentWord.font = QFont(m_fontFamily, fontSize);
        currentWord.color = m_defaultColor; 

        if (tryPlaceWord(currentWord, successfullyPlacedWords)) {
            successfullyPlacedWords.append(currentWord);
        }
    }
    m_drawableWords = successfullyPlacedWords;
}

bool WordCloudWidget::tryPlaceWord(WordItem &item, const QList<WordItem>& placedItems) {
    QFontMetrics fm(item.font);
    item.rect = fm.boundingRect(item.text); // Get initial rect for width/height

    if (item.rect.width() > width() || item.rect.height() > height()) {
        return false; // Word itself is larger than the widget
    }

    // Center of the widget
    double centerX = width() / 2.0;
    double centerY = height() / 2.0;

    if (placedItems.isEmpty()) { // First word, place in center
        item.rect.moveTo(centerX - item.rect.width() / 2.0, centerY - item.rect.height() / 2.0);
        return true;
    }

    // Rectangular spiral placement
    double step = qMax(item.rect.width(), item.rect.height()) * 0.2; // Step size based on word size
    if (step < 5) step = 5; // Minimum step
    double angle = 0;
    double distance = step;
    int turns = 0;
    int pointsPerTurn = 8; // Check more points as we go further out

    for (int k = 0; k < 5000; ++k) { // Max iterations for spiral search
        double x = centerX + distance * qCos(angle);
        double y = centerY + distance * qSin(angle);

        item.rect.moveTo(x - item.rect.width() / 2.0, y - item.rect.height() / 2.0);

        // Check bounds
        if (item.rect.left() < 0 || item.rect.right() > width() ||
            item.rect.top() < 0 || item.rect.bottom() > height()) {
            // If one attempt goes out of bounds, maybe increase distance faster or change strategy
            // For now, just continue spiral; it will naturally expand
        }

        bool collision = false;
        for (const auto& placed : placedItems) {
            if (item.rect.intersects(placed.rect)) {
                collision = true;
                break;
            }
        }

        if (!collision) {
            // Check bounds again more strictly after finding non-colliding spot
             if (item.rect.left() >= 0 && item.rect.right() <= width() &&
                 item.rect.top() >= 0 && item.rect.bottom() <= height()) {
                return true; // Found a spot
            }
        }

        angle += (2.0 * M_PI) / pointsPerTurn; 
        if (angle > (turns + 1) * 2.0 * M_PI) {
            turns++;
            distance += step; // Increase distance for next layer of spiral
            pointsPerTurn = qMax(8, static_cast<int>(distance / step) * 4); // More points for larger circles
        }
        
        // If spiral gets too large compared to widget, stop
        if (distance > qMax(width(), height()) * 1.5) break; 
    }

    return false; // Could not place
}


void WordCloudWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), palette().window()); 

    if (m_lastWidgetSize != size() && !m_sourceFrequencies.isEmpty()){
        prepareWordItems();
    }

    for (int i = 0; i < m_drawableWords.size(); ++i) {
        const auto& wordItem = m_drawableWords.at(i);
        painter.setFont(wordItem.font);
        painter.setPen((i == m_hoveredWordIndex) ? m_hoverColor : wordItem.color);
        painter.drawText(wordItem.rect, Qt::AlignCenter, wordItem.text); // Align text in its rect
    }
}

void WordCloudWidget::mouseMoveEvent(QMouseEvent *event)
{
    int previouslyHovered = m_hoveredWordIndex;
    m_hoveredWordIndex = -1;

    for (int i = 0; i < m_drawableWords.size(); ++i) {
        if (m_drawableWords.at(i).rect.contains(event->position())) {
            m_hoveredWordIndex = i;
            QToolTip::showText(event->globalPosition().toPoint(),
                               QString("%1 (%2)").arg(m_drawableWords.at(i).text).arg(m_drawableWords.at(i).count),
                               this);
            break;
        }
    }

    if (m_hoveredWordIndex == -1) {
        QToolTip::hideText();
    }

    if (previouslyHovered != m_hoveredWordIndex) {
        update(); 
    }
}

void WordCloudWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    if (m_hoveredWordIndex != -1) {
        m_hoveredWordIndex = -1;
        QToolTip::hideText();
        update();
    }
}

void WordCloudWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_sourceFrequencies.isEmpty()) {
        prepareWordItems();
        update(); 
    }
}

void WordCloudWidget::wheelEvent(QWheelEvent *event)
{
    int delta = event->angleDelta().y();
    if (delta > 0) {
        zoomIn();
    } else if (delta < 0) {
        zoomOut();
    }
    event->accept();
}
