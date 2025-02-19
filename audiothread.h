// audiothread.h
#ifndef AUDIOTHREAD_H
#define AUDIOTHREAD_H

#include "mainwindow.h"

#include <QThread>
#include <QAudioSource>
#include <QAudioSink>
#include <QAudioFormat>
#include <QMutex>
#include <QByteArray>
#include <QLoggingCategory>
#include <vector>
#include <SoundTouch.h>
#include <samplerate.h>

// Declare logging category for audio debugging
Q_DECLARE_LOGGING_CATEGORY(audioCategory)

// ----------------------------------------------------------
// 1. Biquad Filter Class Declaration
// ----------------------------------------------------------

class Biquad
{
public:
    Biquad();
    void setupLowPass(float cutoff, float sampleRate, float Q = 0.7071f);
    void setupHighPass(float cutoff, float sampleRate, float Q = 0.7071f);
    void setupBandPass(float centerFreq, float bandwidth, float sampleRate, float Q = 0.7071f);
    void setupNotch(float centerFreq, float bandwidth, float sampleRate, float Q = 0.7071f);

    float process(float in);

    void applyLowPass(std::vector<float>& buffer, float cutoffHz, int sampleRate);
    void applyHighPass(std::vector<float>& buffer, float cutoffHz, int sampleRate);
    void applyBandPass(std::vector<float>& buffer, float centerFreq, float bandwidth, int sampleRate);
    void applyBandStop(std::vector<float>& buffer, float centerFreq, float bandwidth, int sampleRate);
    void setNoiseGateDB(int value);
    int getNoiseGateDB();
private:
    float b0, b1, b2, a0, a1, a2;
    float z1, z2;
};

// ----------------------------------------------------------
// 2. AudioThread Class Declaration
// ----------------------------------------------------------

class AudioThread : public QThread
{
    Q_OBJECT

public:
    explicit AudioThread(MainWindow* mainWin, QObject* parent = nullptr);
    ~AudioThread();

    void stop();
    void pause();
    void resume();

    void setVolume(int value);
    int getSampleRate() const;

    void updateFilter(int filterIdx, float lowFreq, float highFreq, int sampleRate);

signals:
    void levelChanged(float level);

protected:
    void run() override;

private:
    void initializeAudioDevices();
    void initializeAudioEffects();
    void initializeFilters();
    void cleanup();

    void performSampleRateConversionToFloat(const QByteArray& input, QByteArray& output,
                                            int inSampleRate, int outSampleRate, int inChannels);
    QByteArray int16ToFloat(const QByteArray& input, int channels);

    void applyNoiseGate(QByteArray &inBuffer, int sampleRate);
    void applyPitchShifting(float* inputSamples, int numSamples, QByteArray& outputBuffer);
    void applyDistortion(float* samples, int numSamples, float gain);
    void applyBandFilterFloat(float* samples, int numSamples,
                              int filterIndex, float lowFreq, float highFreq, int sampleRate);
    float computeLevel(const QByteArray &buffer);

    void handleAudioSourceStateChanged(QAudio::State state);
    void handleAudioSinkStateChanged(QAudio::State state);

private:
    MainWindow* m_mainWindow;
    bool m_running;
    bool m_paused;

    QAudioSource* m_audioSource;
    QAudioSink* m_audioSink;
    QAudioFormat m_inputFormat;
    QAudioFormat m_outputFormat;

    int m_inBytesPerSample;
    int m_outBytesPerSample;
    int m_inChannels;
    int m_outChannels;
    int m_chunkSize;

    soundtouch::SoundTouch m_soundTouch;
    SRC_STATE* m_sampleRateConverter;

    Biquad m_biquadFilter;

    QByteArray m_previousPitchOutput;

    QMutex m_parametersMutex;

    bool m_isGateClosed;
    int m_holdCounter;
    float m_gain;
    int noiseGateDB;
};

#endif // AUDIOTHREAD_H
