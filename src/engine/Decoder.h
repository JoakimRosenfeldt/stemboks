#pragma once
#include "Types.h"

// FFmpeg-backed decoding to RAM + tag reading + recording encode.
namespace Decoder {
    // Decode whole file to stereo float at targetRate. Empty on failure (error set).
    AudioBufferPtr decodeFile(const QString& path, int targetRate, QString* error = nullptr);
    // Decode one audio stream index of a multi-stream file (e.g. NI .stem.mp4). -1 = default.
    AudioBufferPtr decodeStream(const QString& path, int streamIndex, int targetRate, QString* error = nullptr);
    int audioStreamCount(const QString& path);
    TrackInfo readTags(const QString& path);
    QStringList supportedExtensions();
}

// Streaming WAV/MP3 writer for mix recording (FFmpeg encoder).
class Recorder {
public:
    Recorder();
    ~Recorder();
    bool open(const QString& path, int sampleRate, bool mp3, int bitDepth /*16,24,32*/);
    void write(const float* interleaved, int frames);
    void close();
    bool isOpen() const { return m_open; }
    double secondsRecorded() const;
private:
    struct Impl; Impl* m_impl = nullptr;
    bool m_open = false;
    qint64 m_frames = 0; int m_rate = 44100;
};
