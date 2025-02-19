#include "levelmeter.h"
#include <QPainter>
#include <QColor>

LevelMeter::LevelMeter(QWidget *parent)
    : QWidget(parent)
    , m_level(0.0f)
    , m_numSteps(5)
{}

void LevelMeter::setLevel(float level)
{
    // Clamp to [0..1]
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    m_level = level;
    // Trigger a repaint
    update();
}

void LevelMeter::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);

    if (m_level < 0.01f) {
        painter.fillRect(rect(), Qt::darkGreen);
        return;
    }

    // For a vertical "stairs" meter:
    //   - We'll divide the widget's height into m_numSteps segments.
    //   - Each step i corresponds to a threshold: i+1 / m_numSteps
    //   - If m_level >= threshold, that step is green; else gray.

    int w = width();
    int h = height();

    // Step height in px
    float stepHeight = static_cast<float>(h) / m_numSteps;

    // Draw from bottom to top
    for (int i = 0; i < m_numSteps; ++i) {
        // threshold for this step
        float stepThreshold = (i + 1) / static_cast<float>(m_numSteps);

        // Compute the rectangle for this step
        int stepTop    = h - static_cast<int>((i+1) * stepHeight);
        int stepBottom = h - static_cast<int>(i * stepHeight);

        QRect stepRect(0, stepTop, w, stepBottom - stepTop);

        // If current level is high enough to “fill” this step
        if (m_level >= stepThreshold) {
            painter.fillRect(stepRect, Qt::green);
        } else {
            // Draw a darker color to show the step
            painter.fillRect(stepRect, Qt::darkGreen);
        }
    }
}
