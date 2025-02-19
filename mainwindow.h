#pragma once

#include <QMainWindow>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(audioCategory)

class AudioThread;
class LevelMeter;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @brief The MainWindow class manages the user interface and interacts with the AudioThread.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Audio parameters
    float m_distortionGain;   ///< Current distortion gain.
    float m_pitchFactor;      ///< Current pitch factor.
    int m_lowBandFreq;        ///< Current low band frequency.
    int m_highBandFreq;       ///< Current high band frequency.
    int m_filterIndex;        ///< Current filter index.

    /**
     * @brief Returns the user-selected noise gate threshold in dB
     */
    int getNoiseGate();

    ~MainWindow();

signals:
    /**
     * @brief Emitted when filter parameters change.
     * @param filterIdx Index indicating the type of filter.
     * @param lowFreq Lower frequency bound for band filters.
     * @param highFreq Upper frequency bound for band filters.
     * @param sampleRate Sample rate of the audio in Hz.
     */
    void filterParametersChanged(int filterIdx, float lowFreq, float highFreq, int sampleRate);

private slots:
    void on_noiseGateSlider_valueChanged(int value);
    void on_volumeSlider_valueChanged(int value);
    void on_pitchSlider_valueChanged(int value);
    void on_distortionSlider_valueChanged(int value);
    void on_lowBandSlider_valueChanged(int value);
    void on_highBandSlider_valueChanged(int value);
    void on_bandFilterComboBox_currentIndexChanged(int index);
    void on_playback_CheckBox_toggled(bool checked);

    void on_pitchValueEditLine_editingFinished();
    void on_distortionValueEditLine_editingFinished();
    void on_lowBandValueEditLine_editingFinished();
    void on_highBandValueEditLine_editingFinished();

    /**
     * @brief Called whenever the AudioThread emits a new audio level.
     */
    void handleLevelChanged(float level);

private:
    /**
     * @brief Sets the noise gate threshold (dB) internally and logs the change.
     * Called when the user moves the noiseGate slider.
     */
    void setNoiseGate(int value);

    /**
     * @brief Logs UI changes for debugging.
     */
    void logUIChange(const QString &elementName, const QString &oldValue, const QString &newValue);

    /**
     * @brief Helper to convert filter index to a descriptive string.
     */
    QString filterIndexToString(int index) const;

private:
    Ui::MainWindow *ui;          ///< Pointer to the UI elements.
    AudioThread* m_audioThread;  ///< Pointer to the AudioThread.
    LevelMeter* m_levelMeter;    ///< Pointer to the LevelMeter widget.
};

