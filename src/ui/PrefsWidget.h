#pragma once
#include <QWidget>

class QStackedWidget; class QListWidget;

// MENU screen: sidebar (Stembox / Controls / Pads / Mixer / Recording /
// Display / Settings / About) with segmented controls exactly as the
// original. All state persisted via AppSettings and mirrored to the
// ControlRegistry under app.* / mixer.* where the engine needs it.
class PrefsWidget : public QWidget {
    Q_OBJECT
public:
    explicit PrefsWidget(QWidget* parent = nullptr);
signals:
    void closed();
private:
    QListWidget* m_sidebar;
    QStackedWidget* m_stack;
    QWidget* pageStembox(); QWidget* pageControls(); QWidget* pagePads();
    QWidget* pageMixer(); QWidget* pageRecording(); QWidget* pageDisplay();
    QWidget* pageSettings(); QWidget* pageAbout();
};
