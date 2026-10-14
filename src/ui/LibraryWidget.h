#pragma once
#include "../library/Library.h"
#include <QWidget>

class QStackedWidget; class QListWidget; class QTableWidget; class QLabel; class QLineEdit;

// Library area replacing the waveform zone: SOURCE screen (USB devices +
// STEMBOKS LIBRARY, inspector, STEMS MANAGER button), drill-down browse
// (Playlist/Artist/Album/Key/Folder/History/Matching), track table with
// preview, SEARCH screen with on-screen QWERTY/AZERTY keyboard, TAG LIST.
// Loads tracks into decks via DeckLoader callbacks wired by MainWindow.
class LibraryWidget : public QWidget {
    Q_OBJECT
public:
    explicit LibraryWidget(QWidget* parent = nullptr);
    enum Page { Hidden, Source, Browse, Search, TagList };
    void showPage(Page p);
    Page page() const { return m_page; }
signals:
    void loadToDeck(int deck, const QString& path);
    void openStemsManager();
    void closed();
private:
    Page m_page = Hidden;
    QStackedWidget* m_stack;
    // source page
    QListWidget* m_sourceList; QLabel *m_srcName, *m_srcSongs, *m_srcPlaylists, *m_srcTotal;
    // browse page
    QListWidget* m_navList;     // 66px left rail categories (text buttons)
    QListWidget* m_groupList;   // playlists/artists/...
    QTableWidget* m_table;
    QLabel* m_browseHeader;
    // search page
    QLineEdit* m_searchEdit; QTableWidget* m_searchTable; QWidget* m_keyboard;
    QString m_category;         // Playlist/Artist/Album/Key/Folder/History/Matching
    int m_fontStep = 1;
    void buildSource(); void buildBrowse(); void buildSearch(); void buildKeyboard(bool azerty);
    void refreshSource(); void refreshGroups(); void showTracks(const QVector<LibraryTrack>& tracks, QTableWidget* table);
    void trackActivated(QTableWidget* table, int row);
};
