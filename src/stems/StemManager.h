#pragma once
#include "../engine/Types.h"
#include <QObject>
#include <QQueue>

namespace stemboks {

// Stem discovery + separation queue.
// Playback sources, in priority order (mirrors observed behavior):
//  1. "<track>.stem.mp4" / .stem.m4a multi-stream file (NI Stems: stream0=mix,1..4=stems)
//  2. Sidecar folder  "<track-stem>.stems/{drums,bass,other,vocals}.wav|flac|mp3"
//     next to the track, or in the configured stems folder.
// Separation: ONNX Runtime HTDemucs (model path from settings
// [Stembox] stems_model_path; downloadable filename stems-hdemucs-mmi-v1.onnx).
// Without ONNX or a model, jobs fail with a clear status string.
class StemManager : public QObject {
    Q_OBJECT
public:
    static StemManager& instance();

    static bool stemsExistFor(const QString& trackPath);
    static QString stemsFolderFor(const QString& trackPath); // sidecar dir (may not exist)
    // Load 4 stem buffers for a track (any source). Returns false if none.
    static bool loadStems(const QString& trackPath, int targetRate,
                          std::array<AudioBufferPtr, kNumStems>* out);

    struct Job { QString trackPath; int progress = 0; QString status; };
    void enqueue(const QString& trackPath);
    QVector<Job> jobs() const { return m_jobs; }
    void clearFinished();
    bool separationAvailable(QString* whyNot = nullptr) const;
    QString modelPath() const;

signals:
    void jobsChanged();
    void jobFinished(const QString& trackPath, bool ok);

private:
    StemManager() = default;
    void pump();
    QVector<Job> m_jobs;
    bool m_running = false;
};

} // namespace stemboks
