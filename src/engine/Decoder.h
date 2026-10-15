#pragma once
#include "Types.h"

namespace stemboks {

// Offline FFmpeg decoding for separation and tag import.
namespace Decoder {
    // Decode whole file to stereo float at targetRate. Empty on failure (error set).
    AudioBufferPtr decodeFile(const QString& path, int targetRate, QString* error = nullptr);
    // Decode one audio stream index of a multi-stream file (e.g. NI .stem.mp4). -1 = default.
    AudioBufferPtr decodeStream(const QString& path, int streamIndex, int targetRate, QString* error = nullptr);
    int audioStreamCount(const QString& path);
    TrackInfo readTags(const QString& path);
    QStringList supportedExtensions();
}

} // namespace stemboks
