// audiothread.cpp

#include "mainwindow.h"

#include <QMediaDevices>
#include <QAudioDevice>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <cmath>
#include <vector>
#include <QtMath> // For M_PI

#include <SoundTouch.h>
#include <samplerate.h>

#include <QFile>
#include <QAudioBuffer>
#include <QAudioDecoder>
#include "audiothread.h"
#include <QMutex>

using namespace soundtouch;

Q_LOGGING_CATEGORY(audioCategory, "audio")

// ----------------------------------------------------------
// 1. Biquad Filter Class Implementation
// ----------------------------------------------------------

Biquad::Biquad()
    : b0(1.0f), b1(0.0f), b2(0.0f),
    a0(1.0f), a1(0.0f), a2(0.0f),
    z1(0.0f), z2(0.0f)
{
}

void Biquad::setupLowPass(float cutoff, float sampleRate, float Q)
{
    float omega = 2.0f * M_PI * cutoff / sampleRate;
    float alpha = sinf(omega) / (2.0f * Q);

    float cos_omega = cosf(omega);
    float a0_inv = 1.0f / (1.0f + alpha);

    b0 = (1.0f - cos_omega) / 2.0f;
    b1 = 1.0f - cos_omega;
    b2 = (1.0f - cos_omega) / 2.0f;
    a0 = 1.0f + alpha;
    a1 = -2.0f * cos_omega;
    a2 = 1.0f - alpha;

    b0 *= a0_inv;
    b1 *= a0_inv;
    b2 *= a0_inv;
    a1 *= a0_inv;
    a2 *= a0_inv;
}

void Biquad::setupHighPass(float cutoff, float sampleRate, float Q)
{
    float omega = 2.0f * M_PI * cutoff / sampleRate;
    float alpha = sinf(omega) / (2.0f * Q);

    float cos_omega = cosf(omega);
    float a0_inv = 1.0f / (1.0f + alpha);

    b0 = (1.0f + cos_omega) / 2.0f;
    b1 = -(1.0f + cos_omega);
    b2 = (1.0f + cos_omega) / 2.0f;
    a0 = 1.0f + alpha;
    a1 = -2.0f * cos_omega;
    a2 = 1.0f - alpha;

    b0 *= a0_inv;
    b1 *= a0_inv;
    b2 *= a0_inv;
    a1 *= a0_inv;
    a2 *= a0_inv;
}

void Biquad::setupBandPass(float centerFreq, float bandwidth, float sampleRate, float Q)
{
    float omega = 2.0f * M_PI * centerFreq / sampleRate;
    float alpha = sinf(omega) * sinhf(logf(2.0f) / 2.0f * bandwidth * omega / sinf(omega));

    float cos_omega = cosf(omega);
    float a0_inv = 1.0f / (1.0f + alpha);

    b0 = alpha;
    b1 = 0.0f;
    b2 = -alpha;
    a0 = 1.0f + alpha;
    a1 = -2.0f * cos_omega;
    a2 = 1.0f - alpha;

    b0 *= a0_inv;
    b1 *= a0_inv;
    b2 *= a0_inv;
    a1 *= a0_inv;
    a2 *= a0_inv;
}

void Biquad::setupNotch(float centerFreq, float bandwidth, float sampleRate, float Q)
{
    float omega = 2.0f * M_PI * centerFreq / sampleRate;
    float alpha = sinf(omega) * sinhf(logf(2.0f) / 2.0f * bandwidth * omega / sinf(omega));

    float cos_omega = cosf(omega);
    float a0_inv = 1.0f / (1.0f + alpha);

    b0 = 1.0f;
    b1 = -2.0f * cos_omega;
    b2 = 1.0f;
    a0 = 1.0f + alpha;
    a1 = -2.0f * cos_omega;
    a2 = 1.0f - alpha;

    b0 *= a0_inv;
    b1 *= a0_inv;
    b2 *= a0_inv;
    a1 *= a0_inv;
    a2 *= a0_inv;
}

float Biquad::process(float in)
{
    float out = b0 * in + b1 * z1 + b2 * z2
                - a1 * z1 - a2 * z2;
    z2 = z1;
    z1 = out;
    return out;
}

void Biquad::applyLowPass(std::vector<float> &buffer, float cutoffHz, int sampleRate)
{
    if (cutoffHz <= 0.f || cutoffHz >= sampleRate * 0.5f) {
        return; // Invalid or trivial
    }
    setupLowPass(cutoffHz, sampleRate);
    for (auto &sample : buffer) {
        sample = process(sample);
    }
}

void Biquad::applyHighPass(std::vector<float> &buffer, float cutoffHz, int sampleRate)
{
    if (cutoffHz <= 0.f || cutoffHz >= sampleRate * 0.5f) {
        return;
    }
    setupHighPass(cutoffHz, sampleRate);
    for (auto &sample : buffer) {
        sample = process(sample);
    }
}

void Biquad::applyBandPass(std::vector<float> &buffer, float centerFreq, float bandwidth, int sampleRate)
{
    if (centerFreq <= 0.f || centerFreq >= sampleRate * 0.5f) {
        return;
    }
    setupBandPass(centerFreq, bandwidth, sampleRate);
    for (auto &sample : buffer) {
        sample = process(sample);
    }
}

void Biquad::applyBandStop(std::vector<float> &buffer, float centerFreq, float bandwidth, int sampleRate)
{
    if (centerFreq <= 0.f || centerFreq >= sampleRate * 0.5f) {
        return;
    }
    setupNotch(centerFreq, bandwidth, sampleRate);
    for (auto &sample : buffer) {
        sample = process(sample);
    }
}

// ----------------------------------------------------------
// 2. AudioThread Class Implementation
// ----------------------------------------------------------

AudioThread::AudioThread(MainWindow* mainWin, QObject* parent)
    : QThread(parent)
    , m_mainWindow(mainWin)
    , m_running(false)
    , m_audioSource(nullptr)
    , m_audioSink(nullptr)
    , m_inBytesPerSample(0)
    , m_outBytesPerSample(0)
    , m_inChannels(0)
    , m_outChannels(0)
    , m_chunkSize(0)
    , m_paused(false)
    , m_sampleRateConverter(nullptr)
    , m_isGateClosed(false)
    , m_holdCounter(0)
    , m_gain(1.0f)
    , noiseGateDB(-20)
{
}

AudioThread::~AudioThread()
{
    stop();
    wait(); // Ensure the thread has finished
}

void AudioThread::stop()
{
    qCDebug(audioCategory) << "Stopping AudioThread";
    m_running = false;
}

void AudioThread::run()
{
    // Set thread priority to highest to minimize preemption
    QThread::currentThread()->setPriority(QThread::TimeCriticalPriority);

    m_running = true;
    qCDebug(audioCategory) << "AudioThread started";

    if (!m_mainWindow) {
        qCWarning(audioCategory) << "MainWindow pointer is null!";
        m_running = false;
    }

    // ----------------------------------------------------------
    // 1) Initialize Audio Devices and Formats
    // ----------------------------------------------------------
    initializeAudioDevices();

    if (!m_running) {
        cleanup();
        return;
    }

    // ----------------------------------------------------------
    // 2) Initialize Audio Effects: SoundTouch and libsamplerate
    // ----------------------------------------------------------
    initializeAudioEffects();

    if (!m_running) {
        cleanup();
        return;
    }

    // ----------------------------------------------------------
    // 3) Initialize Filters
    // ----------------------------------------------------------
    initializeFilters();

    // ----------------------------------------------------------
    // 4) Start Audio Streams
    // ----------------------------------------------------------
    QIODevice* inputIO = m_audioSource->start();
    QIODevice* outputIO = m_audioSink->start();

    if (!inputIO) {
        qCWarning(audioCategory) << "Failed to start QAudioSource!";
    }
    if (!outputIO) {
        qCWarning(audioCategory) << "Failed to start QAudioSink!";
    }

    if (!inputIO || !outputIO) {
        qCWarning(audioCategory) << "Failed to start QAudioSource or QAudioSink input/output!";
        m_running = false;
        cleanup();
        return;
    }

    // ----------------------------------------------------------
    // 5) Main Processing Loop
    // ----------------------------------------------------------
    qCDebug(audioCategory) << "AudioThread: Starting main loop";
    int counter = 0;

    while (m_running) {
        bool was_pitched = false;
        bool was_distorted = false;
        bool was_filtered = false;

        if (m_paused) {
            QThread::msleep(10);
            continue;
        }

        if (m_audioSource->bytesAvailable() == 0) {
            QThread::msleep(5);
            continue;
        }

        // Fetch latest parameters before processing
        float pitchFactor;
        float distortionGain;
        int currentFilterIdx;
        float lowFreq, highFreq;
        {
            QMutexLocker lock(&m_parametersMutex);
            pitchFactor     = m_mainWindow->m_pitchFactor;
            distortionGain  = m_mainWindow->m_distortionGain;
            currentFilterIdx= m_mainWindow->m_filterIndex;
            lowFreq         = static_cast<float>(m_mainWindow->m_lowBandFreq);
            highFreq        = static_cast<float>(m_mainWindow->m_highBandFreq);
        }

        // Update SoundTouch pitch if changed
        if (std::fabs(pitchFactor - 1.0f) > 0.0001f) {
            float pitchSemiTones = 12.0f * qLn(pitchFactor) / qLn(2.0f);
            m_soundTouch.setPitchSemiTones(pitchSemiTones);
        } else {
            m_soundTouch.setPitchSemiTones(0.0f);
        }

        // ---------------------------
        //  Read from microphone
        // ---------------------------
        QByteArray inputBuffer;
        qint64 readSize = qMin(m_audioSource->bytesAvailable(), m_chunkSize);
        inputBuffer.resize(readSize);
        qint64 len = inputIO->read(inputBuffer.data(), readSize);
        if (len <= 0) {
            QThread::msleep(1);
            continue;
        }
        inputBuffer.resize(len);

        // If input device is not mono, consider downmixing here
        // For now, just warn if not 1 channel:
        if (m_inChannels != 1) {
            // You could do a real downmix, but we just warn:
            qCWarning(audioCategory) << "Input has" << m_inChannels << "channels; code expects 1 (mono).";
        }

        // ---------------------------
        // Convert Int16 to Float +
        //  Sample Rate Conversion if needed
        // ---------------------------
        QByteArray convertedBuffer;
        if (m_inputFormat.sampleRate() != m_outputFormat.sampleRate()) {
            // Always convert to float
            performSampleRateConversionToFloat(
                inputBuffer, convertedBuffer,
                m_inputFormat.sampleRate(),
                m_outputFormat.sampleRate(),
                m_inChannels
                );
        } else {
            // If sample rates match, just do int16->float here
            convertedBuffer = int16ToFloat(inputBuffer, m_inChannels);
        }

        // ---------------------------
        // Apply Noise Gate (float)
        // ---------------------------
        int dBValue = m_mainWindow->getNoiseGate();
        if (dBValue < 0) {
            applyNoiseGate(convertedBuffer, m_outputFormat.sampleRate());
        }

        // ---------------------------
        // Convert to float array
        // ---------------------------
        int numSamples = convertedBuffer.size() / sizeof(float);
        std::vector<float> inputSamples(numSamples);
        std::memcpy(inputSamples.data(), convertedBuffer.constData(), numSamples * sizeof(float));

        // ---------------------------
        // Pitch Shifting
        // ---------------------------
        if (std::fabs(pitchFactor - 1.0f) > 0.0001f) {
            applyPitchShifting(inputSamples.data(), numSamples, convertedBuffer);
            // Re-copy pitched samples back into inputSamples
            std::memcpy(inputSamples.data(), convertedBuffer.data(), numSamples * sizeof(float));
            was_pitched = true;
        }

        // ---------------------------
        // Distortion
        // ---------------------------
        if (std::fabs(distortionGain - 1.0f) > 0.0001f) {
            applyDistortion(inputSamples.data(), numSamples, distortionGain);
            was_distorted = true;
        }

        // ---------------------------
        // Band Filter (Float)
        // ---------------------------
        if (currentFilterIdx != 0) {
            applyBandFilterFloat(inputSamples.data(), numSamples, currentFilterIdx,
                                 lowFreq, highFreq, m_outputFormat.sampleRate());
            was_filtered = true;
        }

        // ---------------------------
        // Copy final samples back to QByteArray
        // for output to speakers
        // ---------------------------
        std::memcpy(convertedBuffer.data(), inputSamples.data(), numSamples * sizeof(float));

        // Write to speaker
        qint64 bytesWritten = outputIO->write(convertedBuffer);
        if (bytesWritten <= 0) {
            qWarning() << "Failed to write audio data to output! Bytes queued:" << convertedBuffer.size();
        }

        // Emit audio level for UI level meter (float)
        float level = computeLevel(convertedBuffer);
        emit levelChanged(level);

        // Log occasional debug info (not every block)
        ++counter;
        if (counter >= 100) {
            counter = 0;
            qDebug("Wrote to output - Bytes: %lld", static_cast<long long>(bytesWritten));
            if (was_pitched)    qDebug("Pitch shifted");
            if (was_distorted)  qDebug("Distorted");
            if (was_filtered)   qDebug("Filtered");
        }
    }

    // Cleanup resources upon exiting the loop
    cleanup();
}

// ----------------------------------------------------------
// 3. Additional Member Functions
// ----------------------------------------------------------

void AudioThread::setVolume(int value)
{
    if (m_audioSink) {
        float volume = static_cast<float>(value) / 10.0f;
        m_audioSink->setVolume(volume);
    }
}

void AudioThread::applyNoiseGate(QByteArray &inBuffer, int sampleRate)
{
    if (inBuffer.isEmpty()) return;

    // Convert from dB to linear
    float thresholdLinear = std::pow(10.0f, noiseGateDB / 20.0f);

    // Attack/Release in milliseconds
    const int attackMs = 10;
    const int releaseMs = 50;
    int attackSamples  = static_cast<int>((sampleRate * attackMs) / 1000.0f);
    int releaseSamples = static_cast<int>((sampleRate * releaseMs) / 1000.0f);

    // The dynamic reduction based on noiseGateDB
    float reduction = 1.0f - std::pow(10.0f, noiseGateDB / 20.0f);
    reduction = std::clamp(reduction, 0.0f, 0.9f);

    float *samples = reinterpret_cast<float*>(inBuffer.data());
    int numSamples = inBuffer.size() / sizeof(float);

    for (int i = 0; i < numSamples; ++i) {
        float sample = samples[i];
        float level = std::fabs(sample);

        if (level < thresholdLinear) {
            if (!m_isGateClosed) {
                m_isGateClosed = true;
                m_holdCounter = 0;
            }
            if (m_gain > reduction) {
                float gainStep = (1.0f - reduction) / static_cast<float>(releaseSamples);
                m_gain -= gainStep;
                m_gain = std::max(m_gain, reduction);
            }
        } else {
            if (m_isGateClosed) {
                m_holdCounter++;
                if (m_holdCounter > attackSamples) {
                    m_isGateClosed = false;
                }
            }
            if (m_gain < 1.0f) {
                float gainStep = (1.0f - reduction) / static_cast<float>(attackSamples);
                m_gain += gainStep;
                m_gain = std::min(m_gain, 1.0f);
            }
        }
        samples[i] *= m_gain;
    }
}

void AudioThread::cleanup()
{
    if (m_audioSource) {
        m_audioSource->stop();
    }
    if (m_audioSink) {
        m_audioSink->stop();
    }
    delete m_audioSource;
    delete m_audioSink;
    m_audioSource = nullptr;
    m_audioSink   = nullptr;

    // Cleanup libsamplerate
    if (m_sampleRateConverter) {
        src_delete(m_sampleRateConverter);
        m_sampleRateConverter = nullptr;
    }
}

int AudioThread::getSampleRate() const
{
    if (!m_audioSink) {  // Ensure m_audioSink is valid
        qCWarning(audioCategory) << "[Error] Audio Sink pointer is NULL!";
        return 0;
    }
    if (m_audioSink->isNull()) {
        qCWarning(audioCategory) << "[Error] Audio Sink is NULL!";
        return 0;
    }
    if (!m_outputFormat.isValid()) {
        qCWarning(audioCategory) << "[Error] Invalid Sample Rate!";
        return 0;
    }
    return m_outputFormat.sampleRate();
}

void AudioThread::pause()
{
    qDebug() << "[AudioThread] Paused.";
    m_paused = true;
}

void AudioThread::resume()
{
    qDebug() << "[AudioThread] Resumed.";
    m_paused = false;
}

// ----------------------------------------------------------
// 4. Initialization
// ----------------------------------------------------------

void AudioThread::initializeAudioDevices()
{
    QAudioDevice inputDevice  = QMediaDevices::defaultAudioInput();
    QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();

    qCDebug(audioCategory) << "Default input device:"  << inputDevice.description();
    qCDebug(audioCategory) << "Default output device:" << outputDevice.description();

    // Set desired audio format
    m_inputFormat  = inputDevice.preferredFormat();
    m_outputFormat = outputDevice.preferredFormat();

    // Force input to 16-bit, output to float
    m_inputFormat.setSampleFormat(QAudioFormat::Int16);
    m_outputFormat.setSampleFormat(QAudioFormat::Float);

    // Debug logs for input format
    qCDebug(audioCategory) << "Input format in use:"
                           << "  SampleRate =" << m_inputFormat.sampleRate()
                           << "  Channels ="    << m_inputFormat.channelCount()
                           << "  SampleFormat ="<< m_inputFormat.sampleFormat();

    // Debug logs for output format
    qCDebug(audioCategory) << "Output format in use:"
                           << "  SampleRate =" << m_outputFormat.sampleRate()
                           << "  Channels ="    << m_outputFormat.channelCount()
                           << "  SampleFormat ="<< m_outputFormat.sampleFormat();

    // Create QAudioSource and QAudioSink
    m_audioSource = new QAudioSource(inputDevice, m_inputFormat, nullptr);
    if (m_audioSource->isNull()) {
        qCWarning(audioCategory) << "QAudioSource is not available!";
        m_running = false;
    }

    m_audioSink = new QAudioSink(outputDevice, m_outputFormat, nullptr);
    if (m_audioSink->isNull()) {
        qCWarning(audioCategory) << "QAudioSink is not available!";
        m_running = false;
    }

    connect(m_audioSource, &QAudioSource::stateChanged, this, &AudioThread::handleAudioSourceStateChanged);
    connect(m_audioSink,   &QAudioSink::stateChanged,   this, &AudioThread::handleAudioSinkStateChanged);

    m_audioSource->setVolume(1.0);
    m_audioSink->setVolume(1.0);

    // Calculate bytes per sample and channels
    m_inBytesPerSample = 2;  // Because Int16
    m_inChannels       = m_inputFormat.channelCount();
    int inBytesPerFrame= m_inChannels * m_inBytesPerSample;
    m_chunkSize        = 256 * inBytesPerFrame;

    // For output, we'll be writing float data
    m_outBytesPerSample = 4;
    m_outChannels       = m_outputFormat.channelCount();

    qDebug() << "Input Format: SampleRate=" << m_inputFormat.sampleRate()
             << "Channels=" << m_inChannels
             << "BytesPerSample=" << m_inBytesPerSample;
    qDebug() << "Chunk Size:" << m_chunkSize;
    qDebug() << "Output Format: Channels=" << m_outChannels
             << "BytesPerSample=" << m_outBytesPerSample;
}

void AudioThread::initializeAudioEffects()
{
    // Initialize SoundTouch
    m_soundTouch.setSampleRate(m_inputFormat.sampleRate());
    m_soundTouch.setChannels(m_inputFormat.channelCount());
    m_soundTouch.setPitchSemiTones(0.0f);
    m_soundTouch.setTempo(1.0f);
    m_soundTouch.setRate(1.0f);
    m_soundTouch.setSetting(SETTING_USE_AA_FILTER, 1);
    m_soundTouch.setSetting(SETTING_SEQUENCE_MS,   40);
    m_soundTouch.setSetting(SETTING_SEEKWINDOW_MS, 15);
    m_soundTouch.setSetting(SETTING_OVERLAP_MS,    8);

    int error;
    m_sampleRateConverter = src_new(SRC_SINC_FASTEST, m_inputFormat.channelCount(), &error);
    if (!m_sampleRateConverter) {
        qCWarning(audioCategory) << "libsamplerate initialization failed:" << src_strerror(error);
        m_running = false;
    }
}

void AudioThread::initializeFilters()
{
    int filterIdx;
    float lowFreq, highFreq;
    {
        QMutexLocker lock(&m_parametersMutex);
        filterIdx = m_mainWindow->m_filterIndex;
        lowFreq   = static_cast<float>(m_mainWindow->m_lowBandFreq);
        highFreq  = static_cast<float>(m_mainWindow->m_highBandFreq);
    }

    // Set up the chosen filter once. If you want dynamic changes,
    // you can call updateFilter(...) from the UI
    switch (filterIdx) {
    case 1: // Low Pass
        m_biquadFilter.setupLowPass(lowFreq, m_inputFormat.sampleRate());
        break;
    case 2: // High Pass
        m_biquadFilter.setupHighPass(highFreq, m_inputFormat.sampleRate());
        break;
    case 3: // Band Pass
        m_biquadFilter.setupBandPass((lowFreq + highFreq) * 0.5f,
                                     (highFreq - lowFreq),
                                     m_inputFormat.sampleRate());
        break;
    case 4: // Notch
        m_biquadFilter.setupNotch((lowFreq + highFreq) * 0.5f,
                                  (highFreq - lowFreq),
                                  m_inputFormat.sampleRate());
        break;
    default:
        // No filter
        break;
    }
}

// ----------------------------------------------------------
// 5. Audio Processing Functions
// ----------------------------------------------------------

// Always produce float data:
void AudioThread::performSampleRateConversionToFloat(
    const QByteArray& input,
    QByteArray& output,
    int inSampleRate,
    int outSampleRate,
    int inChannels)
{
    if (!m_sampleRateConverter || input.isEmpty()) {
        qCWarning(audioCategory) << "Invalid SRC conversion state or empty input";
        return;
    }

    const double src_ratio = static_cast<double>(outSampleRate) / static_cast<double>(inSampleRate);
    if (src_ratio <= 0.0 || src_ratio > 256.0) {
        qCWarning(audioCategory) << "Invalid sample rate ratio:" << src_ratio;
        return;
    }

    // Number of frames in input
    int inputFrames = input.size() / (inChannels * sizeof(qint16));
    int maxOutputFrames = static_cast<int>(std::ceil(inputFrames * src_ratio * 1.2));

    // Convert input from Int16 -> float
    std::vector<float> inputBuffer(inputFrames * inChannels);
    {
        const qint16* int16Data = reinterpret_cast<const qint16*>(input.constData());
        const float scale = 1.0f / 32768.0f;
        for (int i = 0; i < inputFrames * inChannels; ++i) {
            inputBuffer[i] = int16Data[i] * scale;
        }
    }

    std::vector<float> outputBuffer(maxOutputFrames * inChannels);

    SRC_DATA srcData;
    memset(&srcData, 0, sizeof(srcData));
    srcData.data_in = inputBuffer.data();
    srcData.data_out= outputBuffer.data();
    srcData.input_frames = inputFrames;
    srcData.output_frames = maxOutputFrames;
    srcData.src_ratio = src_ratio;
    srcData.end_of_input = 0;

    int error = src_process(m_sampleRateConverter, &srcData);
    if (error) {
        qCWarning(audioCategory) << "SRC processing failed:" << src_strerror(error);
        return;
    }

    // Write the processed data as float to output
    int outCount = srcData.output_frames_gen * inChannels;
    output = QByteArray(reinterpret_cast<const char*>(outputBuffer.data()),
                        outCount * sizeof(float));
}

// Simple helper: if sample rates match, just convert Int16 -> float
QByteArray AudioThread::int16ToFloat(const QByteArray &input, int channels)
{
    QByteArray out;
    if (input.isEmpty()) return out;

    int frames = input.size() / (channels * sizeof(qint16));
    out.resize(frames * channels * sizeof(float));

    const qint16* inPtr = reinterpret_cast<const qint16*>(input.constData());
    float* outPtr       = reinterpret_cast<float*>(out.data());
    const float scale   = 1.0f / 32768.0f;

    for (int i = 0; i < frames * channels; ++i) {
        outPtr[i] = inPtr[i] * scale;
    }
    return out;
}

void AudioThread::applyPitchShifting(float* inputSamples, int numSamples, QByteArray& outputBuffer)
{
    if (!inputSamples || numSamples <= 0) {
        qCWarning(audioCategory) << "Invalid input for pitch shifting";
        return;
    }

    float pitchFactor;
    {
        QMutexLocker lock(&m_parametersMutex);
        pitchFactor = m_mainWindow->m_pitchFactor;
    }

    // Crossfade parameters
    const int minCrossfade = 50;
    const int maxCrossfade = 256;
    int crossfadeSamples = static_cast<int>(minCrossfade * std::abs(pitchFactor));
    crossfadeSamples = std::clamp(crossfadeSamples, minCrossfade, maxCrossfade);

    // Estimate output size
    int estimatedOutputSize = static_cast<int>(numSamples * pitchFactor) + crossfadeSamples;
    std::vector<SAMPLETYPE> outputSamples;
    outputSamples.reserve(std::max(estimatedOutputSize, 0));

    if (m_soundTouch.numUnprocessedSamples() > 8192) {
        qCWarning(audioCategory) << "SoundTouch buffer risk: clearing old samples.";
        m_soundTouch.clear();
    }

    try {
        m_soundTouch.putSamples(inputSamples, numSamples);

        const int chunkSize = 1024;
        std::vector<SAMPLETYPE> tempBuffer(chunkSize);

        while (m_soundTouch.numSamples() > 0) {
            int received = m_soundTouch.receiveSamples(tempBuffer.data(), chunkSize);
            if (received <= 0) break;
            outputSamples.insert(outputSamples.end(),
                                 tempBuffer.begin(),
                                 tempBuffer.begin() + received);
        }
    } catch (const std::exception& e) {
        qCCritical(audioCategory) << "SoundTouch processing failed:" << e.what();
        return;
    }

    // Crossfade with previous pitch output
    if (!m_previousPitchOutput.isEmpty() && !outputSamples.empty()) {
        const SAMPLETYPE* prevSamples = reinterpret_cast<const SAMPLETYPE*>(m_previousPitchOutput.constData());
        int prevSampleCount = m_previousPitchOutput.size() / sizeof(SAMPLETYPE);

        int fadeLength = std::min({crossfadeSamples,
                                   prevSampleCount,
                                   static_cast<int>(outputSamples.size())});

        for (int i = 0; i < fadeLength; ++i) {
            float fadeIn  = 0.5f * (1.0f - std::cos(M_PI * i / fadeLength));
            float fadeOut = 1.0f - fadeIn;
            int prevIndex = prevSampleCount - fadeLength + i;
            outputSamples[i] = static_cast<SAMPLETYPE>(
                prevSamples[prevIndex] * fadeOut +
                outputSamples[i]      * fadeIn
                );
        }
    }

    // Save overlap region for next crossfade
    if (!outputSamples.empty()) {
        int storeSize = std::min(static_cast<int>(outputSamples.size()), crossfadeSamples);
        m_previousPitchOutput = QByteArray(
            reinterpret_cast<const char*>(outputSamples.data() + (outputSamples.size() - storeSize)),
            storeSize * sizeof(SAMPLETYPE)
            );
    }

    // Write output to QByteArray
    outputBuffer = QByteArray(
        reinterpret_cast<const char*>(outputSamples.data()),
        outputSamples.size() * sizeof(SAMPLETYPE)
        );
}

void AudioThread::applyDistortion(float* samples, int numSamples, float gain)
{
    // Process float in [-1, +1]
    for (int i = 0; i < numSamples; ++i) {
        float x = samples[i] * gain;      // apply gain
        float y = std::tanh(x);           // soft-clip using tanh
        samples[i] = y;                   // store back
    }
}

void AudioThread::applyBandFilterFloat(float* samples, int numSamples,
                                       int filterIndex, float lowFreq,
                                       float highFreq, int sampleRate)
{
    // Put samples into a std::vector for Biquad
    std::vector<float> floatBuf(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        floatBuf[i] = samples[i];
    }

    switch (filterIndex) {
    case 1:
        m_biquadFilter.applyLowPass(floatBuf, lowFreq, sampleRate);
        break;
    case 2:
        m_biquadFilter.applyHighPass(floatBuf, highFreq, sampleRate);
        break;
    case 3:
        m_biquadFilter.applyBandPass(floatBuf, lowFreq, highFreq, sampleRate);
        break;
    case 4:
        m_biquadFilter.applyBandStop(floatBuf, lowFreq, highFreq, sampleRate);
        break;
    default:
        // No filter
        break;
    }

    // Copy back into raw pointer
    for (int i = 0; i < numSamples; ++i) {
        samples[i] = floatBuf[i];
    }
}

float AudioThread::computeLevel(const QByteArray &buffer)
{
    const float* samples = reinterpret_cast<const float*>(buffer.constData());
    int numSamples = buffer.size() / sizeof(float);
    if (numSamples == 0) return 0.f;

    float maxVal = 0.f;
    for (int i = 0; i < numSamples; ++i) {
        float val = std::fabs(samples[i]);
        if (val > maxVal) {
            maxVal = val;
        }
    }
    // For float audio in [-1, +1], maxVal is in [0,1].
    return maxVal;
}

// ----------------------------------------------------------
// 6. State Change Handlers
// ----------------------------------------------------------

void AudioThread::handleAudioSourceStateChanged(QAudio::State state)
{
    if (state == QAudio::StoppedState) {
        if (m_audioSource->error() != QAudio::NoError) {
            qCWarning(audioCategory) << "QAudioSource stopped unexpectedly with error:" << m_audioSource->error();
            m_running = false;
        }
    }
}

void AudioThread::handleAudioSinkStateChanged(QAudio::State state)
{
    if (state == QAudio::StoppedState) {
        if (m_audioSink->error() != QAudio::NoError) {
            qCWarning(audioCategory) << "QAudioSink stopped unexpectedly with error:" << m_audioSink->error();
            m_running = false;
        }
    }
}

// ----------------------------------------------------------
// 7. Filter Update Function
// ----------------------------------------------------------

void AudioThread::updateFilter(int filterIdx, float lowFreq, float highFreq, int sampleRate)
{
    QMutexLocker lock(&m_parametersMutex);
    switch (filterIdx) {
    case 1:
        m_biquadFilter.setupLowPass(lowFreq, sampleRate);
        break;
    case 2:
        m_biquadFilter.setupHighPass(highFreq, sampleRate);
        break;
    case 3:
        m_biquadFilter.setupBandPass((lowFreq + highFreq) / 2.0f,
                                     (highFreq - lowFreq),
                                     sampleRate);
        break;
    case 4:
        m_biquadFilter.setupNotch((lowFreq + highFreq) / 2.0f,
                                  (highFreq - lowFreq),
                                  sampleRate);
        break;
    default:
        // No filter
        break;
    }
}
