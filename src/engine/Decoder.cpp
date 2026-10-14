#include "Decoder.h"
#include <QFile>
#include <algorithm>
#include <climits>
#include <cmath>
#include <mutex>
#include <vector>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/error.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

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
        int ret = avformat_open_input(&format, QFile::encodeName(path).constData(), nullptr, nullptr);
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
        if (ret < 0) return fail(ret);
    }
    if (ret != AVERROR_EOF) return fail(ret);
    if ((ret = avcodec_send_packet(in.codec, nullptr)) < 0 || (ret = receive()) < 0) return fail(ret);
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

struct Recorder::Impl {
    std::mutex mutex;
    AVFormatContext* format = nullptr;
    AVCodecContext* codec = nullptr;
    AVStream* stream = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* packet = nullptr;
    SwrContext* swr = nullptr;
    AVAudioFifo* fifo = nullptr;
    uint8_t** converted = nullptr;
    int capacity = 0, block = 0;
    int64_t pts = 0;
    bool header = false, failed = false;
    ~Impl() { release(); }
    void release() {
        if (converted) { av_freep(&converted[0]); av_freep(&converted); }
        av_audio_fifo_free(fifo); fifo = nullptr; swr_free(&swr);
        av_frame_free(&frame); av_packet_free(&packet); avcodec_free_context(&codec);
        if (format) { if (!(format->oformat->flags & AVFMT_NOFILE)) avio_closep(&format->pb); avformat_free_context(format); }
        format = nullptr; stream = nullptr; header = false; failed = false; pts = 0;
    }
    bool drain() {
        int ret;
        while ((ret = avcodec_receive_packet(codec, packet)) >= 0) {
            av_packet_rescale_ts(packet, codec->time_base, stream->time_base); packet->stream_index = stream->index;
            ret = av_interleaved_write_frame(format, packet); av_packet_unref(packet);
            if (ret < 0) return false;
        }
        return ret == AVERROR(EAGAIN) || ret == AVERROR_EOF;
    }
    bool encode(bool final) {
        int available;
        while ((available = av_audio_fifo_size(fifo)) >= block || (final && available > 0)) {
            int count = std::min(block, available);
            if (av_frame_make_writable(frame) < 0) return false;
            frame->nb_samples = count;
            if (count < block && codec->frame_size && !(codec->codec->capabilities & AV_CODEC_CAP_SMALL_LAST_FRAME)) {
                frame->nb_samples = block;
                av_samples_set_silence(frame->extended_data, count, block-count, 2, codec->sample_fmt);
            }
            if (av_audio_fifo_read(fifo, reinterpret_cast<void**>(frame->extended_data), count) != count) return false;
            frame->pts = pts; pts += frame->nb_samples;
            if (avcodec_send_frame(codec, frame) < 0 || !drain()) return false;
        }
        return true;
    }
    bool convert(const float* input, int count) {
        const uint8_t* source = reinterpret_cast<const uint8_t*>(input);
        int n = swr_convert(swr, converted, capacity, input ? &source : nullptr, count);
        return n >= 0 && av_audio_fifo_write(fifo, reinterpret_cast<void**>(converted), n) == n && encode(false);
    }
};
Recorder::Recorder() : m_impl(new Impl) {}
Recorder::~Recorder() { close(); delete m_impl; }
bool Recorder::open(const QString& path, int rate, bool mp3, int depth) {
    close(); std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_frames = 0; m_rate = rate;
    if (rate <= 0 || (!mp3 && depth != 16 && depth != 24 && depth != 32)) return false;
    const AVCodec* encoder = nullptr;
    if (mp3) {
        for (const char* name : {"libmp3lame", "mp3float", "mp3"}) {
            encoder = avcodec_find_encoder_by_name(name);
            if (encoder && encoder->id == AV_CODEC_ID_MP3) break;
            encoder = nullptr;
        }
    } else encoder = avcodec_find_encoder(depth == 16 ? AV_CODEC_ID_PCM_S16LE : depth == 24 ? AV_CODEC_ID_PCM_S24LE : AV_CODEC_ID_PCM_F32LE);
    if (!encoder) return false;
    auto& s = *m_impl;
    auto fail = [&]() { s.release(); return false; };
    QByteArray filename = QFile::encodeName(path);
    if (avformat_alloc_output_context2(&s.format, nullptr, mp3 ? "mp3" : "wav", filename.constData()) < 0 || !s.format) return fail();
    s.codec = avcodec_alloc_context3(encoder); s.stream = avformat_new_stream(s.format, nullptr);
    s.frame = av_frame_alloc(); s.packet = av_packet_alloc();
    if (!s.codec || !s.stream || !s.frame || !s.packet) return fail();
    s.codec->sample_rate = rate; s.codec->sample_fmt = depth == 32 ? AV_SAMPLE_FMT_FLT : depth == 24 ? AV_SAMPLE_FMT_S32 : AV_SAMPLE_FMT_S16;
    // FFmpeg 7.0 exposes these lists; 7.1 and 8 use the supported-config query.
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 13, 100)
    const void* formats = nullptr; const void* rates = nullptr;
    avcodec_get_supported_config(s.codec, encoder, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, &formats, nullptr);
    avcodec_get_supported_config(s.codec, encoder, AV_CODEC_CONFIG_SAMPLE_RATE, 0, &rates, nullptr);
    const auto* sampleFormats = static_cast<const AVSampleFormat*>(formats);
    const auto* sampleRates = static_cast<const int*>(rates);
#else
    const auto* sampleFormats = encoder->sample_fmts;
    const auto* sampleRates = encoder->supported_samplerates;
#endif
    if (sampleFormats && sampleFormats[0] != AV_SAMPLE_FMT_NONE) s.codec->sample_fmt = sampleFormats[0];
    if (sampleRates) {
        s.codec->sample_rate = sampleRates[0];
        for (const int* p = sampleRates; *p; ++p)
            if (std::abs(*p-rate) < std::abs(s.codec->sample_rate-rate)) s.codec->sample_rate = *p;
    }
    av_channel_layout_default(&s.codec->ch_layout, 2);
    s.codec->time_base = {1, s.codec->sample_rate}; s.codec->bit_rate = mp3 ? 192000 : 0;
    if (s.format->oformat->flags & AVFMT_GLOBALHEADER) s.codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (avcodec_open2(s.codec, encoder, nullptr) < 0 || avcodec_parameters_from_context(s.stream->codecpar, s.codec) < 0) return fail();
    s.stream->time_base = s.codec->time_base;
    s.block = s.codec->frame_size ? s.codec->frame_size : 4096;
    s.frame->format = s.codec->sample_fmt; s.frame->sample_rate = s.codec->sample_rate; s.frame->nb_samples = s.block;
    if (av_channel_layout_copy(&s.frame->ch_layout, &s.codec->ch_layout) < 0 || av_frame_get_buffer(s.frame, 0) < 0) return fail();
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    if (swr_alloc_set_opts2(&s.swr, &s.codec->ch_layout, s.codec->sample_fmt, s.codec->sample_rate,
                          &stereo, AV_SAMPLE_FMT_FLT, rate, 0, nullptr) < 0 || swr_init(s.swr) < 0) return fail();
    s.capacity = int(av_rescale_rnd(4096, s.codec->sample_rate, rate, AV_ROUND_UP)) + 256;
    if (av_samples_alloc_array_and_samples(&s.converted, nullptr, 2, s.capacity, s.codec->sample_fmt, 0) < 0) return fail();
    s.fifo = av_audio_fifo_alloc(s.codec->sample_fmt, 2, s.capacity + s.block);
    if (!s.fifo) return fail();
    if (!(s.format->oformat->flags & AVFMT_NOFILE) && avio_open(&s.format->pb, filename.constData(), AVIO_FLAG_WRITE) < 0) return fail();
    if (avformat_write_header(s.format, nullptr) < 0) return fail();
    s.header = true; m_open = true; return true;
}
void Recorder::write(const float* samples, int frames) {
    if (!samples || frames <= 0) return;
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (!m_open || m_impl->failed) return;
    for (int offset = 0; offset < frames;) {
        int count = std::min(4096, frames-offset);
        if (!m_impl->convert(samples + 2*offset, count)) { m_impl->failed = true; m_open = false; break; }
        offset += count; m_frames += count;
    }
}
void Recorder::close() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto& s = *m_impl;
    if (s.header) {
        if (!s.failed) {
            while (swr_get_delay(s.swr, s.codec->sample_rate) > 0) {
                int before = av_audio_fifo_size(s.fifo);
                if (!s.convert(nullptr, 0)) break;
                if (av_audio_fifo_size(s.fifo) == before) break;
            }
            if (s.encode(true) && avcodec_send_frame(s.codec, nullptr) >= 0) s.drain();
        }
        av_write_trailer(s.format);
    }
    s.release(); m_open = false;
}
double Recorder::secondsRecorded() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_rate > 0 ? double(m_frames) / m_rate : 0.0;
}
