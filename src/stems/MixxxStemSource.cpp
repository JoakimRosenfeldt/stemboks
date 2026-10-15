#include "MixxxStemSource.h"

#include "StemManager.h"
#include "../control/AppSettings.h"
#include "../engine/Decoder.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
}

namespace stemboks {
namespace {

constexpr int rate = 44100;
const std::array<QString, 4> names = {"drums", "bass", "other", "vocals"};

void require(bool condition, const QString& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}

void check(int result, const char* action) {
    if (result >= 0) return;
    char message[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(result, message, sizeof(message));
    throw std::runtime_error(std::string(action) + ": " + message);
}

QString nativeContainer(const QString& path) {
    if (path.endsWith(".stem.mp4", Qt::CaseInsensitive) || path.endsWith(".stem.m4a", Qt::CaseInsensitive)) return path;
    const QFileInfo info(path);
    for (const QString& suffix : {QStringLiteral(".stem.mp4"), QStringLiteral(".stem.m4a")}) {
        for (const QString& candidate : QStringList{info.dir().filePath(info.completeBaseName() + suffix), path + suffix})
            if (QFileInfo(candidate).isFile()) return candidate;
    }
    return {};
}

QStringList sidecarFiles(const QString& path) {
    const QDir directory(StemManager::stemsFolderFor(path));
    QStringList result;
    for (const QString& name : names) {
        QString found;
        for (const QString& extension : {QStringLiteral("wav"), QStringLiteral("flac"), QStringLiteral("mp3")}) {
            const QString file = directory.filePath(name + '.' + extension);
            if (QFileInfo(file).isFile()) { found = file; break; }
        }
        if (found.isEmpty()) return {};
        result.append(found);
    }
    return result;
}

QByteArray fingerprint(const QString& source, const QStringList& files) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArrayLiteral("stemboks-mixxx-stems-float-m4a-v4\0"));
    const QFileInfo sourceInfo(source);
    hash.addData(sourceInfo.absoluteFilePath().toUtf8());
    hash.addData(QByteArray::number(sourceInfo.size()));
    hash.addData(QByteArray::number(sourceInfo.lastModified().toMSecsSinceEpoch()));
    for (const QString& path : files) {
        QFile file(path);
        require(file.open(QIODevice::ReadOnly), "Cannot read stem: " + path);
        hash.addData(path.toUtf8());
        hash.addData(QByteArray::number(file.size()));
        require(hash.addData(&file), "Cannot hash stem: " + path);
    }
    return hash.result().toHex();
}

struct Encoder {
    AVCodecContext* codec = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* packet = nullptr;
    AVStream* stream = nullptr;
    ~Encoder() {
        av_packet_free(&packet);
        av_frame_free(&frame);
        avcodec_free_context(&codec);
    }
    void drain(AVFormatContext* format) {
        for (;;) {
            const int result = avcodec_receive_packet(codec, packet);
            if (result == AVERROR_EOF || result == AVERROR(EAGAIN)) return;
            check(result, "Read PCM packet");
            av_packet_rescale_ts(packet, codec->time_base, stream->time_base);
            packet->stream_index = stream->index;
            const int written = av_interleaved_write_frame(format, packet);
            av_packet_unref(packet);
            check(written, "Write stem packet");
        }
    }
};

struct Output {
    AVFormatContext* format = nullptr;
    ~Output() {
        if (!format) return;
        if (format->pb) avio_closep(&format->pb);
        avformat_free_context(format);
    }
};

void writeContainer(const QString& path, const std::array<AudioBufferPtr, 4>& stems,
        qint64 frames, const TrackInfo& tags) {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_PCM_F32LE);
    require(codec != nullptr, "FFmpeg float PCM encoder is unavailable");
    Output output;
    check(avformat_alloc_output_context2(&output.format, nullptr, "mov", path.toUtf8().constData()), "Create stem container");
    require(output.format != nullptr, "Cannot create stem container");
    // Mixxx's stem detection checks the MP4/M4A MIME type before its manifest.
    check(av_opt_set(output.format->priv_data, "brand", "M4A ", 0), "Set stem container brand");
    constexpr int block = 4096;
    std::array<Encoder, 5> encoders;
    for (Encoder& encoder : encoders) {
        encoder.codec = avcodec_alloc_context3(codec);
        encoder.frame = av_frame_alloc();
        encoder.packet = av_packet_alloc();
        encoder.stream = avformat_new_stream(output.format, nullptr);
        require(encoder.codec && encoder.frame && encoder.packet && encoder.stream, "Cannot allocate stem encoder");
        encoder.codec->sample_rate = rate;
        encoder.codec->sample_fmt = AV_SAMPLE_FMT_FLT;
        encoder.codec->bits_per_raw_sample = 32;
        encoder.codec->time_base = AVRational{1, rate};
        av_channel_layout_default(&encoder.codec->ch_layout, 2);
        if (output.format->oformat->flags & AVFMT_GLOBALHEADER) encoder.codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        check(avcodec_open2(encoder.codec, codec, nullptr), "Open PCM encoder");
        encoder.stream->time_base = encoder.codec->time_base;
        check(avcodec_parameters_from_context(encoder.stream->codecpar, encoder.codec), "Configure stem stream");
        encoder.frame->format = encoder.codec->sample_fmt;
        encoder.frame->sample_rate = rate;
        encoder.frame->nb_samples = block;
        check(av_channel_layout_copy(&encoder.frame->ch_layout, &encoder.codec->ch_layout), "Configure stem channels");
        check(av_frame_get_buffer(encoder.frame, 0), "Allocate stem frame");
    }
    for (const auto& tag : {std::pair{"title", tags.title}, std::pair{"artist", tags.artist},
                 std::pair{"album", tags.album}, std::pair{"genre", tags.genre}})
        if (!tag.second.isEmpty()) av_dict_set(&output.format->metadata, tag.first, tag.second.toUtf8().constData(), 0);
    check(avio_open(&output.format->pb, path.toUtf8().constData(), AVIO_FLAG_WRITE), "Open stem output");
    check(avformat_write_header(output.format, nullptr), "Write stem header");
    for (qint64 start = 0; start < frames; start += block) {
        const int count = int(std::min<qint64>(block, frames - start));
        for (size_t stream = 0; stream < encoders.size(); ++stream) {
            Encoder& encoder = encoders[stream];
            check(av_frame_make_writable(encoder.frame), "Reuse stem frame");
            encoder.frame->nb_samples = count;
            encoder.frame->pts = start;
            for (int channel = 0; channel < 2; ++channel) {
                auto* samples = reinterpret_cast<float*>(encoder.frame->data[0]);
                for (int frame = 0; frame < count; ++frame) {
                    const qint64 index = (start + frame) * 2 + channel;
                    double sample = 0;
                    if (stream == 0) {
                        for (const auto& stem : stems) if (index < stem->interleaved.size()) sample += stem->interleaved[index];
                    } else if (index < stems[stream - 1]->interleaved.size()) {
                        sample = stems[stream - 1]->interleaved[index];
                    }
                    require(std::isfinite(sample) && std::abs(sample) <= std::numeric_limits<float>::max(), "Stem contains non-finite audio");
                    // MOV float PCM preserves stem headroom, including values above 1.
                    samples[frame * 2 + channel] = float(sample);
                }
            }
            check(avcodec_send_frame(encoder.codec, encoder.frame), "Encode stem frame");
            encoder.drain(output.format);
        }
    }
    for (Encoder& encoder : encoders) {
        check(avcodec_send_frame(encoder.codec, nullptr), "Finish stem encoder");
        encoder.drain(output.format);
    }
    check(av_write_trailer(output.format), "Finish stem container");
    check(avio_closep(&output.format->pb), "Close stem container");
}

quint32 sizeAt(const QByteArray& data, qsizetype offset) {
    require(offset >= 0 && offset + 8 <= data.size(), "Invalid MP4 metadata atom");
    const quint32 size = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + offset));
    require(size >= 8 && quint64(offset) + size <= quint64(data.size()), "Invalid MP4 metadata size");
    return size;
}

void setSize(QByteArray& data, qsizetype offset, quint64 size) {
    require(size <= std::numeric_limits<quint32>::max(), "MP4 metadata is too large");
    qToBigEndian<quint32>(quint32(size), reinterpret_cast<uchar*>(data.data() + offset));
}

QByteArray atom(const char* type, const QByteArray& payload) {
    QByteArray result(8, '\0');
    setSize(result, 0, quint64(payload.size()) + 8);
    std::copy_n(type, 4, result.data() + 4);
    result.append(payload);
    return result;
}

QByteArray trackMetadata(const TrackInfo& tags) {
    const auto text = [](const QString& value) {
        QByteArray header(8, '\0');
        header[3] = 1; // UTF-8 metadata value, followed by the locale.
        return atom("data", header + value.toUtf8());
    };
    QByteArray items;
    const std::array<std::pair<const char*, QString>, 5> fields{{
        {"\251nam", tags.title}, {"\251ART", tags.artist}, {"\251alb", tags.album},
        {"\251gen", tags.genre}, {"\251cmt", tags.comment}}};
    for (const auto& [name, value] : fields) if (!value.isEmpty()) items += atom(name, text(value));
    const auto freeform = [&items, &text](const char* name, const QString& value) {
        if (value.isEmpty()) return;
        items += atom("----", atom("mean", QByteArray(4, '\0') + "com.apple.iTunes")
                + atom("name", QByteArray(4, '\0') + name) + text(value));
    };
    if (std::isfinite(tags.bpm) && tags.bpm > 0) freeform("BPM", QString::number(tags.bpm, 'g', 15));
    freeform("initialkey", tags.keyText);
    QByteArray handler(8, '\0');
    handler += QByteArrayLiteral("mdirappl");
    handler += QByteArray(9, '\0');
    return atom("meta", QByteArray(4, '\0') + atom("hdlr", handler) + atom("ilst", items));
}

void publishWithManifest(const QString& staged, const QString& destination, const TrackInfo& tags) {
    QFile input(staged);
    require(input.open(QIODevice::ReadOnly), "Cannot reopen stem container");
    qint64 moovOffset = -1;
    QByteArray moov;
    for (qint64 offset = 0; offset + 8 <= input.size();) {
        require(input.seek(offset), "Cannot seek stem container");
        const QByteArray header = input.read(16);
        require(header.size() >= 8, "Truncated MP4 atom");
        quint64 size = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(header.constData()));
        if (size == 1) {
            require(header.size() == 16, "Truncated MP4 extended atom");
            size = qFromBigEndian<quint64>(reinterpret_cast<const uchar*>(header.constData() + 8));
        } else if (size == 0) size = quint64(input.size() - offset);
        require(size >= 8 && size <= quint64(input.size() - offset), "Invalid MP4 atom size");
        if (header.mid(4, 4) == "moov") {
            // Inserting metadata is safe because the encoder leaves moov after mdat.
            require(size < 64 * 1024 * 1024 && offset + qint64(size) == input.size(), "Unexpected MP4 metadata placement");
            require(input.seek(offset), "Cannot seek MP4 metadata");
            moov = input.read(qint64(size));
            require(quint64(moov.size()) == size, "Truncated MP4 metadata");
            moovOffset = offset;
            break;
        }
        offset += qint64(size);
    }
    require(moovOffset >= 0, "Stem container has no MP4 metadata");
    sizeAt(moov, 0);
    QJsonArray entries;
    const std::array<QString, 4> colors = {"#319ade", "#d29922", "#35ad72", "#d152b1"};
    for (size_t i = 0; i < names.size(); ++i) entries.append(QJsonObject{{"name", names[i]}, {"color", colors[i]}});
    const QByteArray metadata = atom("stem", QJsonDocument(QJsonObject{{"version", 1}, {"stems", entries}}).toJson(QJsonDocument::Compact))
            + trackMetadata(tags);
    bool inserted = false;
    for (qsizetype offset = 8; offset < moov.size();) {
        const quint32 size = sizeAt(moov, offset);
        if (moov.mid(offset + 4, 4) == "udta") {
            moov.insert(offset + size, metadata);
            setSize(moov, offset, quint64(size) + metadata.size());
            inserted = true;
            break;
        }
        offset += size;
    }
    if (!inserted) moov.append(atom("udta", metadata));
    setSize(moov, 0, moov.size());
    QSaveFile output(destination);
    require(output.open(QIODevice::WriteOnly), "Cannot create cached stem container");
    require(input.seek(0), "Cannot rewind stem container");
    for (qint64 remaining = moovOffset; remaining > 0;) {
        const QByteArray block = input.read(std::min<qint64>(remaining, 1024 * 1024));
        require(!block.isEmpty() && output.write(block) == block.size(), "Cannot copy cached stem audio");
        remaining -= block.size();
    }
    require(output.write(moov) == moov.size() && output.commit(), "Cannot publish cached stem container");
}

} // namespace

QString prepareMixxxPlaybackPath(const QString& sourcePath, QString* error) {
    if (error) error->clear();
    try {
        const QString native = nativeContainer(sourcePath);
        if (!native.isEmpty()) return native;
        const QStringList files = sidecarFiles(sourcePath);
        if (files.isEmpty()) return sourcePath;
        const QByteArray key = fingerprint(sourcePath, files);
        const QDir cache(QDir(AppSettings::cacheDir()).filePath("mixxx-stems"));
        require(QDir().mkpath(cache.absolutePath()), "Cannot create Mixxx stem cache");
        const QString destination = cache.filePath(QString::fromLatin1(key) + ".stem.m4a");
        if (QFileInfo(destination).size() > 0 && Decoder::audioStreamCount(destination) == 5) return destination;
        std::array<AudioBufferPtr, 4> stems;
        qint64 shortest = std::numeric_limits<qint64>::max(), longest = 0;
        for (size_t i = 0; i < stems.size(); ++i) {
            QString detail;
            stems[i] = Decoder::decodeFile(files[int(i)], rate, &detail);
            require(stems[i] && !stems[i]->isEmpty() && stems[i]->sampleRate == rate, "Cannot decode stem " + files[int(i)] + ": " + detail);
            shortest = std::min(shortest, stems[i]->frames());
            longest = std::max(longest, stems[i]->frames());
        }
        require(longest - shortest <= 4096, "Stem sidecars have different durations");
        const TrackInfo tags = Decoder::readTags(sourcePath);
        require(tags.durationSec <= 0 || std::abs(tags.durationSec - double(longest) / rate) <= 0.25,
                "Stem sidecars do not match the source duration");
        QTemporaryFile staged(cache.filePath("preparing-XXXXXX.mp4"));
        require(staged.open(), "Cannot create temporary stem container");
        const QString stagedPath = staged.fileName();
        staged.close();
        writeContainer(stagedPath, stems, longest, tags);
        require(fingerprint(sourcePath, files) == key, "Stem files changed while preparing playback; load again");
        publishWithManifest(stagedPath, destination, tags);
        return destination;
    } catch (const std::exception& exception) {
        if (error) *error = QString::fromUtf8(exception.what());
        return {};
    }
}

} // namespace stemboks
