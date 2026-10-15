#pragma once
#include <QMainWindow>

#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace stemboks {

class DeckWidget; class WaveformWidget; class LibraryWidget;
class PrefsWidget; class StemsManagerWidget; class SecondScreen;

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
    void keyReleaseEvent(QKeyEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
private:
    struct DeckUi { DeckWidget* bar = nullptr; WaveformWidget* wave = nullptr; };
    DeckUi m_ui[4];
    LibraryWidget* m_library;
    PrefsWidget* m_prefs;
    StemsManagerWidget* m_stems;
    SecondScreen* m_second = nullptr;
    QWidget *m_mixPage = nullptr, *m_navLarge = nullptr, *m_navSplit = nullptr;
    QStackedWidget* m_body;
    QTimer* m_timer;
    QWidget *m_deckBars = nullptr, *m_mixerStrip = nullptr, *m_waveColumns = nullptr;
    QVBoxLayout* m_waveLayout = nullptr;
    QStackedWidget* m_barStacks[2];
    QStackedWidget* m_waveStacks[2];
    int m_selectedDeck = 0;
    int m_loadGeneration[4] = {};
    bool m_keyboardCue[2] = {};
    QString m_samplerPaths[8];
    bool m_applyingSettings = false;
    void selectDeck(int deck);
    void saveHotcues();
    void buildNav(); void buildMix(); void buildMixerStrip(QWidget* parent);
    void showMix(); void applySettingsToRegistry();
    void updateSecondScreen();
};

} // namespace stemboks
