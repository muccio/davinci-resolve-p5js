#pragma once

#include <string>
#include <vector>
#include <memory>

/**
 * P5AudioMetrics
 * 
 * Audio features extracted at a specific frame time,
 * passed to p5.js for audio-reactive sketches.
 */
struct P5AudioMetrics {
    float level = 0.0f;       // RMS amplitude (0.0 to 1.0)
    float peak = 0.0f;        // Peak amplitude (0.0 to 1.0)
    float bass = 0.0f;        // Low frequency band energy (0.0 to 1.0)
    float mid = 0.0f;         // Mid frequency band energy (0.0 to 1.0)
    float treble = 0.0f;      // High frequency band energy (0.0 to 1.0)
    std::vector<float> waveform; // Normalized PCM waveform samples [-1.0, 1.0]
    std::vector<float> spectrum; // Normalized frequency bins [0.0, 1.0]
};

/**
 * P5AudioEngine
 * 
 * Frame-accurate, deterministic audio analyzer for DaVinci Resolve.
 * Decodes audio from any supported media file (.wav, .mp3, .m4a, .aac, .mp4, .mov)
 * and calculates volume and FFT frequency spectra at any timeline time.
 */
class P5AudioEngine {
public:
    P5AudioEngine();
    ~P5AudioEngine();

    // Prevent copying
    P5AudioEngine(const P5AudioEngine&) = delete;
    P5AudioEngine& operator=(const P5AudioEngine&) = delete;

    /**
     * Load an audio or video file.
     * @param filePath Absolute or relative path to media file
     * @return true if opened and decoded successfully
     */
    bool loadFile(const std::string& filePath);

    /**
     * Unload active audio file and reset state.
     */
    void closeFile();

    /**
     * Check if a valid audio track is loaded.
     */
    bool hasFile() const;

    /**
     * Get path of currently loaded file.
     */
    std::string getLoadedPath() const;

    /**
     * Extract audio metrics at timeline time in seconds.
     * @param timeInSeconds Timeline time (e.g. frame / fps)
     * @return Calculated metrics (or zero/silent metrics if no audio is loaded)
     */
    P5AudioMetrics getMetricsAtTime(double timeInSeconds);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
