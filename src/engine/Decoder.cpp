#include "Decoder.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <vector>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace stemboks {

namespace {
QString ffError(int code) {
    char text[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(code, text, sizeof(text));
    return QString::fromUtf8(text);
}
struct Input {
    AVFormatContext* format = nullptr;
    AVCodecContext* codec = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* packet = nullptr;
    SwrContext* swr = nullptr;
    ~Input() {
        swr_free(&swr); av_packet_free(&packet); av_frame_free(&frame);
        avcodec_free_context(&codec); avformat_close_input(&format);
    }
    int open(const QString& path) {
        int ret = avformat_open_input(&format, path.toUtf8().constData(), nullptr, nullptr);
        return ret < 0 ? ret : avformat_find_stream_info(format, nullptr);
    }
};
QString tag(AVDictionary* dict, std::initializer_list<const char*> names) {
    for (const char* name : names)
        if (auto* entry = av_dict_get(dict, name, nullptr, 0)) return QString::fromUtf8(entry->value);
    return {};
}
}

AudioBufferPtr Decoder::decodeFile(const QString& path, int rate, QString* error) {
    return decodeStream(path, -1, rate, error);
}
AudioBufferPtr Decoder::decodeStream(const QString& path, int streamIndex, int rate, QString* error) {
    if (error) error->clear();
    auto fail = [error](int code) -> AudioBufferPtr { if (error) *error = ffError(code); return {}; };
    if (rate <= 0 || streamIndex < -1) return fail(AVERROR(EINVAL));
    Input in;
    int ret = in.open(path);
    if (ret < 0) return fail(ret);
    int index = -1;
    if (streamIndex == -1) index = av_find_best_stream(in.format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    else {
        int ordinal = 0;
        for (unsigned i = 0; i < in.format->nb_streams; ++i)
            if (in.format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && ordinal++ == streamIndex) {
                index = int(i); break;
            }
    }
    if (index < 0) return fail(AVERROR_STREAM_NOT_FOUND);
    auto* params = in.format->streams[index]->codecpar;
    const AVCodec* decoder = avcodec_find_decoder(params->codec_id);
    if (!decoder) return fail(AVERROR_DECODER_NOT_FOUND);
    in.codec = avcodec_alloc_context3(decoder);
    if (!in.codec) return fail(AVERROR(ENOMEM));
    if ((ret = avcodec_parameters_to_context(in.codec, params)) < 0 ||
        (ret = avcodec_open2(in.codec, decoder, nullptr)) < 0) return fail(ret);
    in.frame = av_frame_alloc(); in.packet = av_packet_alloc();
    if (!in.frame || !in.packet) return fail(AVERROR(ENOMEM));
    auto audio = std::make_shared<AudioBuffer>(); audio->sampleRate = rate;
    std::vector<float> converted;
    auto convert = [&](const AVFrame* frame) -> int {
        if (!in.swr && frame) {
            AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
            int r = swr_alloc_set_opts2(&in.swr, &stereo, AV_SAMPLE_FMT_FLT, rate,
                                       &frame->ch_layout, AVSampleFormat(frame->format), frame->sample_rate, 0, nullptr);
            if (r < 0 || (r = swr_init(in.swr)) < 0) return r;
        }
        if (!in.swr) return 0;
        int capacity = swr_get_out_samples(in.swr, frame ? frame->nb_samples : 0);
        if (capacity < 0) return capacity;
        converted.resize(size_t(std::max(1, capacity)) * 2);
        uint8_t* dest = reinterpret_cast<uint8_t*>(converted.data());
        int n = swr_convert(in.swr, &dest, capacity,
                            frame ? const_cast<const uint8_t**>(frame->extended_data) : nullptr,
                            frame ? frame->nb_samples : 0);
        if (n > 0) {
            qsizetype old = audio->interleaved.size(); audio->interleaved.resize(old + qsizetype(n) * 2);
            std::copy_n(converted.data(), size_t(n) * 2, audio->interleaved.data() + old);
        }
        return n;
    };
    auto receive = [&]() -> int {
        int r;
        while ((r = avcodec_receive_frame(in.codec, in.frame)) >= 0) {
            r = convert(in.frame); av_frame_unref(in.frame);
            if (r < 0) return r;
        }
        return r == AVERROR(EAGAIN) || r == AVERROR_EOF ? 0 : r;
    };
    while ((ret = av_read_frame(in.format, in.packet)) >= 0) {
        if (in.packet->stream_index == index) {
            ret = avcodec_send_packet(in.codec, in.packet);
            if (ret >= 0) ret = receive();
        }
        av_packet_unref(in.packet);
        if (ret < 0 && ret != AVERROR_INVALIDDATA && ret != AVERROR(EAGAIN)) return fail(ret);
    }
    if (ret != AVERROR_EOF) return fail(ret);
    ret = avcodec_send_packet(in.codec, nullptr);
    if (ret < 0 && ret != AVERROR_INVALIDDATA && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) return fail(ret);
    ret = receive();
    if (ret < 0 && ret != AVERROR_INVALIDDATA) return fail(ret);
    do { ret = convert(nullptr); } while (ret > 0);
    if (ret < 0) return fail(ret);
    return audio->isEmpty() ? fail(AVERROR_INVALIDDATA) : audio;
}
int Decoder::audioStreamCount(const QString& path) {
    Input in; if (in.open(path) < 0) return 0;
    int count = 0;
    for (unsigned i = 0; i < in.format->nb_streams; ++i)
        count += in.format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO;
    return count;
}
TrackInfo Decoder::readTags(const QString& path) {
    TrackInfo info; info.path = path;
    Input in; if (in.open(path) < 0) return info;
    int best = av_find_best_stream(in.format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    AVDictionary* streamTags = best >= 0 ? in.format->streams[best]->metadata : nullptr;
    auto get = [&](std::initializer_list<const char*> names) {
        QString value = tag(in.format->metadata, names); return value.isEmpty() ? tag(streamTags, names) : value;
    };
    info.title = get({"title"}); info.artist = get({"artist", "album_artist"});
    info.album = get({"album"}); info.genre = get({"genre"}); info.comment = get({"comment", "description"});
    info.bpm = get({"bpm", "tbpm", "tempo"}).toDouble(); info.keyText = get({"initialkey", "key", "tkey"});
    if (in.format->duration != AV_NOPTS_VALUE) info.durationSec = double(in.format->duration) / AV_TIME_BASE;
    info.bitrate = int(std::clamp<int64_t>(in.format->bit_rate, 0, INT_MAX));
    if (best >= 0) {
        auto* stream = in.format->streams[best]; info.sampleRate = stream->codecpar->sample_rate;
        if (info.durationSec <= 0 && stream->duration != AV_NOPTS_VALUE)
            info.durationSec = stream->duration * av_q2d(stream->time_base);
    }
    for (unsigned i = 0; i < in.format->nb_streams; ++i) {
        auto* stream = in.format->streams[i];
        if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
            info.artwork = QImage::fromData(stream->attached_pic.data, stream->attached_pic.size);
            if (!info.artwork.isNull()) break;
        }
    }
    return info;
}
QStringList Decoder::supportedExtensions() {
    return {"mp3", "wav", "flac", "aif", "aiff", "m4a", "aac", "ogg", "opus", "stem.mp4", "wma"};
}

} // namespace stemboks
