#pragma once
#include <QMainWindow>

class DeckWidget; class WaveformWidget; class LibraryWidget;
class PrefsWidget; class StemsManagerWidget; class SecondScreen; class QTimer;

// Root window in the shape of stembox-racine.xml: 46px nav (EXIT/MENU/BROWSE/
// TAG LIST/SEARCH; in SPLIT mode two nav bars), body with stacked pages:
// MIX (large waveforms + 2 CDJ deck bars + mixer strip), LIBRARY, MENU prefs,
// STEMS MANAGER. Decks 3/4 appear in SPLIT mode as the second column pair.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void loadTrackToDeck(int deck, const QString& path);
protected:
    void closeEvent(QCloseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
private:
    struct DeckUi { DeckWidget* bar = nullptr; WaveformWidget* wave = nullptr; };
    DeckUi m_ui[4];
    LibraryWidget* m_library;
    PrefsWidget* m_prefs;
    StemsManagerWidget* m_stems;
    SecondScreen* m_second = nullptr;
    QWidget *m_mixPage, *m_navLarge, *m_navSplit;
    class QStackedWidget* m_body;
    QTimer* m_timer;
    void buildNav(); void buildMix(); void buildMixerStrip(QWidget* parent);
    void showMix(); void applySettingsToRegistry();
    void updateSecondScreen();
};
