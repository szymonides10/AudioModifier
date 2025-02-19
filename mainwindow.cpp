#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "audiothread.h"
#include "levelmeter.h"

#include <QCloseEvent>
#include <QDebug>
#include <QIntValidator>
#include <QDoubleValidator>
#include <QTimer>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_audioThread(nullptr)
    , m_distortionGain(1.0f)
    , m_pitchFactor(1.0f)
    , m_lowBandFreq(500)
    , m_highBandFreq(5000)
    , m_filterIndex(0)
{
    ui->setupUi(this);

    // -----------------------------
    // Make window unresizable
    // -----------------------------
    setWindowFlags(Qt::Window | Qt::MSWindowsFixedSizeDialogHint);

    // -----------------------------
    // Noise Gate Slider
    //     Suppose range is -60..0 dB (example).
    //     Zero or positive means "off" or "no gating," negative => gating
    // -----------------------------
    ui->noiseGateSlider->setRange(-60, 0);
    ui->noiseGateSlider->setValue(0);     // default "off"
    connect(ui->noiseGateSlider, &QSlider::valueChanged,
            this, &MainWindow::on_noiseGateSlider_valueChanged);

    // -----------------------------
    // Volume Slider
    // -----------------------------
    ui->volumeSlider->setValue(7);
    connect(ui->volumeSlider, &QSlider::valueChanged,
            this, &MainWindow::on_volumeSlider_valueChanged);

    // -----------------------------
    // Pitch: slider range = 50..200 => 0.5..2.0
    // -----------------------------
    ui->pitchSlider->setRange(50, 200);
    ui->pitchSlider->setValue(100); // => 1.0
    connect(ui->pitchSlider, &QSlider::valueChanged,
            this, &MainWindow::on_pitchSlider_valueChanged);
    ui->pitchValueEditLine->setText(QString::number(m_pitchFactor, 'f', 2));
    connect(ui->pitchValueEditLine, &QLineEdit::editingFinished,
            this, &MainWindow::on_pitchValueEditLine_editingFinished);

    // -----------------------------
    // Distortion: slider range = 0..400 => 0.0..4.0
    // -----------------------------
    ui->distortionSlider->setRange(0, 400);
    ui->distortionSlider->setValue(100); // => 1.0 default
    connect(ui->distortionSlider, &QSlider::valueChanged,
            this, &MainWindow::on_distortionSlider_valueChanged);
    ui->distortionValueEditLine->setText(QString::number(m_distortionGain, 'f', 2));
    connect(ui->distortionValueEditLine, &QLineEdit::editingFinished,
            this, &MainWindow::on_distortionValueEditLine_editingFinished);

    // -----------------------------
    // Low Band: slider range = 20..22000
    // -----------------------------
    ui->lowBandSlider->setRange(20, 22000);
    ui->lowBandSlider->setValue(m_lowBandFreq);
    connect(ui->lowBandSlider, &QSlider::valueChanged,
            this, &MainWindow::on_lowBandSlider_valueChanged);
    ui->lowBandValueEditLine->setText(QString::number(m_lowBandFreq));
    connect(ui->lowBandValueEditLine, &QLineEdit::editingFinished,
            this, &MainWindow::on_lowBandValueEditLine_editingFinished);

    // -----------------------------
    // High Band: slider range = 20..22000
    // -----------------------------
    ui->highBandSlider->setRange(20, 22000);
    ui->highBandSlider->setValue(m_highBandFreq);
    connect(ui->highBandSlider, &QSlider::valueChanged,
            this, &MainWindow::on_highBandSlider_valueChanged);
    ui->highBandValueEditLine->setText(QString::number(m_highBandFreq));
    connect(ui->highBandValueEditLine, &QLineEdit::editingFinished,
            this, &MainWindow::on_highBandValueEditLine_editingFinished);

    // -----------------------------
    // Band filters
    // -----------------------------
    ui->filterLabel->setToolTip("Select a filter type:\n"
                                "- Low-pass: Passes frequencies below the cutoff.\n"
                                "- High-pass: Passes frequencies above the cutoff.\n"
                                "- Band-pass: Passes frequencies within a range.\n"
                                "- Band-stop: Rejects frequencies within a range.");
    ui->bandFilterComboBox->addItem("None");
    ui->bandFilterComboBox->addItem("Low pass");
    ui->bandFilterComboBox->addItem("High pass");
    ui->bandFilterComboBox->addItem("Band pass");
    ui->bandFilterComboBox->addItem("Band stop");
    connect(ui->bandFilterComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::on_bandFilterComboBox_currentIndexChanged);

    // Initialize filter index
    m_filterIndex = ui->bandFilterComboBox->currentIndex();

    // -----------------------------
    // Playback checkbox
    // -----------------------------
    ui->playback_CheckBox->setChecked(true);
    connect(ui->playback_CheckBox, &QCheckBox::toggled,
            this, &MainWindow::on_playback_CheckBox_toggled);

    // -----------------------------
    // Level-meter widget
    // -----------------------------
    m_levelMeter = new LevelMeter(this);
    m_levelMeter->setGeometry(160, 480, 50, 50);
    m_levelMeter->setNumSteps(5);
    if (ui->playback_CheckBox->isChecked())
        m_levelMeter->show();
    else
        m_levelMeter->hide();

    // -----------------------------
    // Start the audio thread
    // -----------------------------
    m_audioThread = new AudioThread(this);
    connect(this, &MainWindow::filterParametersChanged,
            m_audioThread, &AudioThread::updateFilter);
    connect(m_audioThread, &AudioThread::levelChanged,
            this, &MainWindow::handleLevelChanged);

    // Launch the audio thread
    m_audioThread->start();
}

MainWindow::~MainWindow()
{
    if (m_audioThread) {
        m_audioThread->stop();
        m_audioThread->wait();
        delete m_audioThread;
        m_audioThread = nullptr;
    }
    delete ui;
}

//------------------------------------------------------------
// 1. Noise Gate Methods
//------------------------------------------------------------
void MainWindow::on_noiseGateSlider_valueChanged(int value)
{
    // We delegate to setNoiseGate().
    setNoiseGate(value);
}

int MainWindow::getNoiseGate()
{
    // The AudioThread calls this to read the noise gate threshold in dB.
    return ui->noiseGateSlider->value();
}

void MainWindow::setNoiseGate(int value)
{
    // You can log or further handle the new noise gate threshold.
    // For example, negative values => gate is active; 0 => disabled.
    qCDebug(audioCategory) << "[UI] Noise Gate set to:" << value << "dB";
    // If you want to force an immediate filter update or do something else,
    // you can emit filterParametersChanged(...) here or do it only
    // in AudioThread by calling getNoiseGate() inside the loop.
}

//------------------------------------------------------------
// 2. Volume
//------------------------------------------------------------
void MainWindow::on_volumeSlider_valueChanged(int value)
{
    if (m_audioThread) {
        m_audioThread->setVolume(value);
    }
}

//------------------------------------------------------------
// 3. Pitch
//------------------------------------------------------------
void MainWindow::on_pitchSlider_valueChanged(int value)
{
    float oldPitchFactor = m_pitchFactor;
    m_pitchFactor = static_cast<float>(value) / 100.0f;

    ui->pitchValueEditLine->setText(QString::number(m_pitchFactor, 'f', 2));

    logUIChange("Pitch Slider",
                QString::number(oldPitchFactor, 'f', 2),
                ui->pitchValueEditLine->text());

    // Trigger filter update (some DSP might read m_pitchFactor in AudioThread)
    emit filterParametersChanged(m_filterIndex,
                                 m_lowBandFreq,
                                 m_highBandFreq,
                                 m_audioThread->getSampleRate());
}

void MainWindow::on_pitchValueEditLine_editingFinished()
{
    QString text = ui->pitchValueEditLine->text();
    bool ok;
    double value = text.toDouble(&ok);
    if (ok && value >= 0.5 && value <= 2.0) {
        m_pitchFactor = static_cast<float>(value);
        int sliderValue = static_cast<int>(m_pitchFactor * 100);
        if (ui->pitchSlider->value() != sliderValue) {
            ui->pitchSlider->setValue(sliderValue);
        }
    } else {
        // Invalid input: revert to last valid value
        QMessageBox::warning(this, "Invalid Input",
                             "Pitch factor must be between 0.50 and 2.00.");
        ui->pitchValueEditLine->setText(
            QString::number(m_pitchFactor, 'f', 2));
    }
}

//------------------------------------------------------------
// 4. Distortion
//------------------------------------------------------------
void MainWindow::on_distortionSlider_valueChanged(int value)
{
    float oldDistortionGain = m_distortionGain;
    m_distortionGain = static_cast<float>(value) / 100.0f;

    ui->distortionValueEditLine->setText(
        QString::number(m_distortionGain, 'f', 2));

    logUIChange("Distortion Slider",
                QString::number(oldDistortionGain, 'f', 2),
                ui->distortionValueEditLine->text());

    // Emit the signal to update distortion in AudioThread
    emit filterParametersChanged(m_filterIndex,
                                 m_lowBandFreq,
                                 m_highBandFreq,
                                 m_audioThread->getSampleRate());
}

void MainWindow::on_distortionValueEditLine_editingFinished()
{
    QString text = ui->distortionValueEditLine->text();
    bool ok;
    double value = text.toDouble(&ok);
    if (ok && value >= 0.0 && value <= 4.0) {
        m_distortionGain = static_cast<float>(value);
        int sliderValue = static_cast<int>(m_distortionGain * 100);
        if (ui->distortionSlider->value() != sliderValue) {
            ui->distortionSlider->setValue(sliderValue);
        }
    } else {
        // Invalid input: revert to last valid value
        QMessageBox::warning(this, "Invalid Input",
                             "Distortion gain must be between 0.00 and 4.00.");
        ui->distortionValueEditLine->setText(
            QString::number(m_distortionGain, 'f', 2));
    }
}

//------------------------------------------------------------
// 5. Low/High Band
//------------------------------------------------------------
void MainWindow::on_lowBandSlider_valueChanged(int value)
{
    int oldLowBandFreq = m_lowBandFreq;
    m_lowBandFreq = value;

    ui->lowBandValueEditLine->setText(QString::number(m_lowBandFreq));

    logUIChange("Low Band Slider",
                QString::number(oldLowBandFreq) + " Hz",
                QString::number(m_lowBandFreq)  + " Hz");

    emit filterParametersChanged(m_filterIndex,
                                 static_cast<float>(m_lowBandFreq),
                                 static_cast<float>(m_highBandFreq),
                                 m_audioThread->getSampleRate());
}

void MainWindow::on_highBandSlider_valueChanged(int value)
{
    int oldHighBandFreq = m_highBandFreq;
    m_highBandFreq = value;

    ui->highBandValueEditLine->setText(QString::number(m_highBandFreq));

    logUIChange("High Band Slider",
                QString::number(oldHighBandFreq) + " Hz",
                QString::number(m_highBandFreq)  + " Hz");

    emit filterParametersChanged(m_filterIndex,
                                 static_cast<float>(m_lowBandFreq),
                                 static_cast<float>(m_highBandFreq),
                                 m_audioThread->getSampleRate());
}

void MainWindow::on_lowBandValueEditLine_editingFinished()
{
    QString text = ui->lowBandValueEditLine->text();
    bool ok;
    int value = text.toInt(&ok);
    if (ok && value >= 20 && value <= 22000) {
        m_lowBandFreq = value;
        if (ui->lowBandSlider->value() != m_lowBandFreq) {
            ui->lowBandSlider->setValue(m_lowBandFreq);
        }
    } else {
        QMessageBox::warning(this, "Invalid Input",
                             "Low band frequency must be between 20 and 22000 Hz.");
        ui->lowBandValueEditLine->setText(
            QString::number(m_lowBandFreq));
    }
}

void MainWindow::on_highBandValueEditLine_editingFinished()
{
    QString text = ui->highBandValueEditLine->text();
    bool ok;
    int value = text.toInt(&ok);
    if (ok && value >= 20 && value <= 22000) {
        m_highBandFreq = value;
        if (ui->highBandSlider->value() != m_highBandFreq) {
            ui->highBandSlider->setValue(m_highBandFreq);
        }
    } else {
        QMessageBox::warning(this, "Invalid Input",
                             "High band frequency must be between 20 and 22000 Hz.");
        ui->highBandValueEditLine->setText(
            QString::number(m_highBandFreq));
    }
}

//------------------------------------------------------------
// 6. Filter ComboBox
//------------------------------------------------------------
void MainWindow::on_bandFilterComboBox_currentIndexChanged(int index)
{
    int oldFilterIndex = m_filterIndex;
    m_filterIndex = index;

    logUIChange("Band Filter ComboBox:",
                QString::number(oldFilterIndex),
                QString::number(index));

    emit filterParametersChanged(m_filterIndex,
                                 static_cast<float>(m_lowBandFreq),
                                 static_cast<float>(m_highBandFreq),
                                 m_audioThread->getSampleRate());
}

//------------------------------------------------------------
// 7. Playback (Pause/Resume)
//------------------------------------------------------------
void MainWindow::on_playback_CheckBox_toggled(bool checked)
{
    if (m_audioThread) {
        if (checked) {
            m_audioThread->resume();
            m_levelMeter->show();
            qCDebug(audioCategory) << "[UI] Playback Checkbox checked: Playback enabled.";
        } else {
            m_audioThread->pause();
            m_levelMeter->hide();
            qCDebug(audioCategory) << "[UI] Playback Checkbox unchecked: Playback paused.";
        }
    } else {
        qCWarning(audioCategory) << "[UI] Playback Checkbox toggled but AudioThread is null.";
    }
}

//------------------------------------------------------------
// 8. Level Meter
//------------------------------------------------------------
void MainWindow::handleLevelChanged(float level)
{
    m_levelMeter->setLevel(level);
}

//------------------------------------------------------------
// 9. Utility / Logging
//------------------------------------------------------------
void MainWindow::logUIChange(const QString &elementName,
                             const QString &oldValue,
                             const QString &newValue)
{
    qCDebug(audioCategory) << "[UI]" << elementName << "changed from"
                           << oldValue << "to" << newValue;
}

QString MainWindow::filterIndexToString(int index) const
{
    switch (index) {
    case 1: return "Low pass";
    case 2: return "High pass";
    case 3: return "Band pass";
    case 4: return "Band stop";
    default: return "None";
    }
}
