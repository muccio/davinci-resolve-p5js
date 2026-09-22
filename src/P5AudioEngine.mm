#import "P5AudioEngine.h"

#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <Accelerate/Accelerate.h>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <iostream>

class P5AudioEngine::Impl {
public:
    std::string loadedPath;
    double sampleRate = 44100.0;
    std::vector<float> audioSamples; // Mono 32-bit float samples
    mutable std::mutex dataMutex;

    // FFT resources
    static constexpr int kFFTLog2N = 8; // 256 points
    static constexpr int kFFTN = 1 << kFFTLog2N; // 256
    static constexpr int kHalfN = kFFTN / 2;     // 128
    FFTSetup fftSetup = nullptr;
    std::vector<float> windowBuffer;

    Impl() {
        fftSetup = vDSP_create_fftsetup(kFFTLog2N, FFT_RADIX2);
        windowBuffer.resize(kFFTN);
        vDSP_hann_window(windowBuffer.data(), kFFTN, vDSP_HANN_NORM);
    }

    ~Impl() {
        if (fftSetup) {
            vDSP_destroy_fftsetup(fftSetup);
            fftSetup = nullptr;
        }
    }

    void reset() {
        std::lock_guard<std::mutex> lock(dataMutex);
        loadedPath.clear();
        audioSamples.clear();
        sampleRate = 44100.0;
    }

    bool load(const std::string& path) {
        if (path.empty()) {
            reset();
            return false;
        }

        NSString *nsPath = [NSString stringWithUTF8String:path.c_str()];
        if (![nsPath isAbsolutePath]) {
            nsPath = [[[NSFileManager defaultManager] currentDirectoryPath] stringByAppendingPathComponent:nsPath];
        }
        nsPath = [nsPath stringByStandardizingPath];

        if (![[NSFileManager defaultManager] fileExistsAtPath:nsPath]) {
            NSLog(@"[P5AudioEngine] File does not exist: %@", nsPath);
            reset();
            return false;
        }

        NSURL *fileURL = [NSURL fileURLWithPath:nsPath];
        ExtAudioFileRef audioFile = nullptr;
        OSStatus status = ExtAudioFileOpenURL((__bridge CFURLRef)fileURL, &audioFile);
        if (status != noErr || !audioFile) {
            NSLog(@"[P5AudioEngine] ExtAudioFileOpenURL failed with error: %d", (int)status);
            reset();
            return false;
        }

        // Set client format to Mono Float32, 44100 Hz
        AudioStreamBasicDescription clientFormat;
        memset(&clientFormat, 0, sizeof(clientFormat));
        clientFormat.mSampleRate = 44100.0;
        clientFormat.mFormatID = kAudioFormatLinearPCM;
        clientFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        clientFormat.mBitsPerChannel = 32;
        clientFormat.mChannelsPerFrame = 1;
        clientFormat.mBytesPerFrame = sizeof(float);
        clientFormat.mFramesPerPacket = 1;
        clientFormat.mBytesPerPacket = sizeof(float);

        status = ExtAudioFileSetProperty(audioFile,
                                         kExtAudioFileProperty_ClientDataFormat,
                                         sizeof(clientFormat),
                                         &clientFormat);
        if (status != noErr) {
            NSLog(@"[P5AudioEngine] ExtAudioFileSetProperty ClientDataFormat error: %d", (int)status);
            ExtAudioFileDispose(audioFile);
            reset();
            return false;
        }

        // Query total frame count
        SInt64 totalFrames = 0;
        UInt32 propSize = sizeof(totalFrames);
        ExtAudioFileGetProperty(audioFile, kExtAudioFileProperty_FileLengthFrames, &propSize, &totalFrames);

        std::vector<float> loadedSamples;
        constexpr UInt32 kChunkFrames = 32768;
        std::vector<float> chunk(kChunkFrames);

        AudioBufferList bufferList;
        bufferList.mNumberBuffers = 1;
        bufferList.mBuffers[0].mNumberChannels = 1;
        bufferList.mBuffers[0].mDataByteSize = kChunkFrames * sizeof(float);
        bufferList.mBuffers[0].mData = chunk.data();

        while (true) {
            UInt32 framesToRead = kChunkFrames;
            bufferList.mBuffers[0].mDataByteSize = framesToRead * sizeof(float);

            status = ExtAudioFileRead(audioFile, &framesToRead, &bufferList);
            if (status != noErr || framesToRead == 0) {
                break;
            }
            loadedSamples.insert(loadedSamples.end(), chunk.begin(), chunk.begin() + framesToRead);
        }

        ExtAudioFileDispose(audioFile);

        {
            std::lock_guard<std::mutex> lock(dataMutex);
            loadedPath = path;
            sampleRate = clientFormat.mSampleRate;
            audioSamples = std::move(loadedSamples);
        }

        NSLog(@"[P5AudioEngine] Loaded %lu samples from %@", (unsigned long)audioSamples.size(), nsPath);
        return true;
    }

    P5AudioMetrics getMetrics(double timeInSeconds) {
        P5AudioMetrics metrics;
        metrics.waveform.resize(kHalfN, 0.0f);
        metrics.spectrum.resize(kHalfN, 0.0f);

        std::lock_guard<std::mutex> lock(dataMutex);
        if (audioSamples.empty()) {
            return metrics;
        }

        int64_t centerSample = (int64_t)(timeInSeconds * sampleRate);
        int64_t startSample = centerSample - (kFFTN / 2);

        std::vector<float> windowedInput(kFFTN, 0.0f);
        float peakVal = 0.0f;
        double sumSquares = 0.0;

        for (int i = 0; i < kFFTN; ++i) {
            int64_t idx = startSample + i;
            float val = 0.0f;
            if (idx >= 0 && idx < (int64_t)audioSamples.size()) {
                val = audioSamples[idx];
            }
            float absVal = std::fabs(val);
            if (absVal > peakVal) peakVal = absVal;
            sumSquares += val * val;

            // Apply Hanning window
            windowedInput[i] = val * windowBuffer[i];

            // Waveform (sampled down to kHalfN)
            if (i < kHalfN) {
                metrics.waveform[i] = val;
            }
        }

        // RMS Level
        float rms = std::sqrt(sumSquares / (double)kFFTN);
        metrics.level = std::min(1.0f, rms * 2.5f); // Scale nicely for p5 sketches
        metrics.peak = std::min(1.0f, peakVal);

        // Perform FFT via Apple Accelerate vDSP
        DSPSplitComplex splitComplex;
        std::vector<float> realp(kHalfN);
        std::vector<float> imagp(kHalfN);
        splitComplex.realp = realp.data();
        splitComplex.imagp = imagp.data();

        // Pack into even-odd split complex
        vDSP_ctoz((const DSPComplex*)windowedInput.data(), 2, &splitComplex, 1, kHalfN);

        // Compute 1D In-Place FFT
        vDSP_fft_zrip(fftSetup, &splitComplex, 1, kFFTLog2N, FFT_FORWARD);

        // Compute magnitudes: sqrt(real^2 + imag^2)
        std::vector<float> magnitudes(kHalfN);
        vDSP_zvabs(&splitComplex, 1, magnitudes.data(), 1, kHalfN);

        // Normalize magnitudes (scale by 1.0 / (2 * N))
        float scale = 1.0f / (float)(kFFTN);
        vDSP_vsmul(magnitudes.data(), 1, &scale, magnitudes.data(), 1, kHalfN);

        // Copy spectrum bins and calculate band energies
        float bassSum = 0.0f;
        float midSum = 0.0f;
        float trebleSum = 0.0f;

        int bassLimit = kHalfN / 8;     // ~0 - 2.7 kHz
        int midLimit = kHalfN / 2;      // ~2.7 - 11 kHz

        for (int i = 0; i < kHalfN; ++i) {
            float mag = std::min(1.0f, magnitudes[i] * 4.0f);
            metrics.spectrum[i] = mag;

            if (i < bassLimit) {
                bassSum += mag;
            } else if (i < midLimit) {
                midSum += mag;
            } else {
                trebleSum += mag;
            }
        }

        metrics.bass = std::min(1.0f, bassSum / (float)std::max(1, bassLimit));
        metrics.mid = std::min(1.0f, midSum / (float)std::max(1, midLimit - bassLimit));
        metrics.treble = std::min(1.0f, trebleSum / (float)std::max(1, kHalfN - midLimit));

        return metrics;
    }
};

P5AudioEngine::P5AudioEngine()
    : m_impl(std::make_unique<Impl>()) {
}

P5AudioEngine::~P5AudioEngine() = default;

bool P5AudioEngine::loadFile(const std::string& filePath) {
    return m_impl->load(filePath);
}

void P5AudioEngine::closeFile() {
    m_impl->reset();
}

bool P5AudioEngine::hasFile() const {
    std::lock_guard<std::mutex> lock(m_impl->dataMutex);
    return !m_impl->audioSamples.empty();
}

std::string P5AudioEngine::getLoadedPath() const {
    std::lock_guard<std::mutex> lock(m_impl->dataMutex);
    return m_impl->loadedPath;
}

P5AudioMetrics P5AudioEngine::getMetricsAtTime(double timeInSeconds) {
    return m_impl->getMetrics(timeInSeconds);
}
