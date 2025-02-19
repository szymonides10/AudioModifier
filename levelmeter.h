#pragma once

#include <QWidget>

class LevelMeter : public QWidget
{
    Q_OBJECT
public:
    explicit LevelMeter(QWidget *parent = nullptr);
    void setNumSteps(int steps) { m_numSteps = steps; }

public slots:
    void setLevel(float level);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    float m_level;      // 0.0 .. 1.0
    int   m_numSteps;   // how many “stairs” steps
};

