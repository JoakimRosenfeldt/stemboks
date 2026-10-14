#include "LibraryWidget.h"
#include "UiHelpers.h"
#include "../engine/AudioEngine.h"
#include "../engine/Decoder.h"
#include <QDirIterator>
#include "../stems/StemManager.h"
#include "../library/RekordboxUsb.h"
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QPainter>
#include <QStackedWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QInputDialog>
#include <QStorageInfo>
#include <QSet>
#include <QTimer>
#include <QSignalBlocker>
#include <QDateTime>
#include <QMessageBox>
#include <algorithm>

bool importScannedLibraryTracks(const QVector<TrackInfo>& tracks, const QVector<bool>& stemsReady);
namespace {
struct ScannedTracks { QVector<TrackInfo> tracks; QVector<bool> stemsReady; };
}

LibraryWidget::LibraryWidget(QWidget* parent) : QWidget(parent) {
    setObjectName("LibraryContainer");
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget; root->addWidget(m_stack);
    buildSource(); buildBrowse(); buildSearch();
    connect(&Library::instance(), &Library::changed, this, [this] {
        refreshSource();
        if (m_page == Browse || m_page == TagList) refreshGroups();
        if (m_page == Search) showTracks(Library::instance().allTracks(m_searchEdit->text()), m_searchTable);
    });
    connect(&AppSettings::instance(), &AppSettings::changed, this, [this](const QString& group, const QString& key) {
        if (group == "[Stembox]" && key == "keyboard_azerty") buildKeyboard(AppSettings::instance().getInt(group, key) != 0);
    });
    refreshSource();
}

void LibraryWidget::buildSource() {
    auto* page = new QWidget; auto* root = new QVBoxLayout(page); root->setContentsMargins(0, 0, 0, 0);
    auto* header = new QWidget; header->setObjectName("StemboxSourceHeaderRow"); header->setFixedHeight(70);
    auto* hl = new QHBoxLayout(header); hl->addWidget(Ui::label("SOURCE", "StemboxSourceHeader"), 1);
    auto* close = Ui::button("BACK"); connect(close, &QPushButton::clicked, this, &LibraryWidget::closed); hl->addWidget(close);
    auto* stems = Ui::button("STEMS MANAGER", "StemboxStemsManagerBtn"); hl->addWidget(stems);
    connect(stems, &QPushButton::clicked, this, &LibraryWidget::openStemsManager); root->addWidget(header);
    auto* row = new QHBoxLayout; m_sourceList = new QListWidget; row->addWidget(m_sourceList, 1);
    auto* inspector = new QWidget; inspector->setMinimumWidth(250); auto* il = new QVBoxLayout(inspector);
    m_srcName = Ui::label("Source: STEMBOKS LIBRARY", "StemboxAboutLine");
    m_srcSongs = Ui::label("Songs: 0", "StemboxAboutLine"); m_srcPlaylists = Ui::label("Playlists: 0", "StemboxAboutLine");
    m_srcTotal = Ui::label("Total: 00:00", "StemboxAboutLine");
    for (auto* l : {m_srcName, m_srcSongs, m_srcPlaylists, m_srcTotal}) { l->setMinimumHeight(46); il->addWidget(l); }
    il->addWidget(Ui::label("Date: " + QDate::currentDate().toString(Qt::ISODate), "StemboxAboutLine"));
    il->addStretch();
    auto* open = Ui::button("Open library"); il->addWidget(open);
    connect(open, &QPushButton::clicked, this, [this, open] {
        const auto* item = m_sourceList->currentItem(); m_sourceRoot = item ? item->data(Qt::UserRole).toString() : QString();
        if (!m_sourceRoot.isEmpty() && RekordboxUsb::isRekordboxUsb(m_sourceRoot)
            && !m_rekordboxImported.contains(m_sourceRoot)) {
            m_rekordboxImported.insert(m_sourceRoot);
            const QString root = m_sourceRoot;
            open->setEnabled(false); open->setText("Reading rekordbox USB...");
            auto* watcher = new QFutureWatcher<RekordboxUsb::Library>(this);
            connect(watcher, &QFutureWatcher<RekordboxUsb::Library>::finished, this, [this, open, watcher, root] {
                const auto rb = watcher->result(); watcher->deleteLater();
                open->setEnabled(true); open->setText("Open library");
                QVector<TrackInfo> tracks; QVector<bool> stems;
                QHash<int, QString> pathByRow;
                for (auto it = rb.rowIdToTrack.constBegin(); it != rb.rowIdToTrack.constEnd(); ++it)
                    if (it.value() >= 0 && it.value() < rb.tracks.size())
                        pathByRow[it.key()] = RekordboxUsb::trackAbsolutePath(root, rb.tracks[it.value()]);
                for (const auto& t : rb.tracks) {
                    TrackInfo info;
                    info.path = RekordboxUsb::trackAbsolutePath(root, t);
                    if (info.path.isEmpty()) continue;
                    info.title = t.title; info.artist = t.artist; info.album = t.album; info.genre = t.genre;
                    info.durationSec = t.durationSec; info.bpm = t.bpm; info.keyText = t.keyText;
                    tracks.append(info); stems.append(StemManager::stemsExistFor(info.path));
                }
                if (!tracks.isEmpty()) {
                    importScannedLibraryTracks(tracks, stems);
                    auto& lib = Library::instance();
                    for (const auto& p : rb.playlists) {
                        const qint64 pid = lib.createPlaylist("RB: " + p.name);
                        if (pid < 0) continue;
                        for (int rowId : p.trackRowIds) {
                            const QString path = pathByRow.value(rowId);
                            const qint64 tid = path.isEmpty() ? -1 : lib.trackIdForPath(path);
                            if (tid >= 0) lib.addToPlaylist(pid, tid);
                        }
                    }
                }
                refreshSource(); refreshGroups();
                showPage(Browse);
            });
            watcher->setFuture(QtConcurrent::run([root] {
                RekordboxUsb::Library lib; RekordboxUsb::parse(root, &lib); return lib;
            }));
            return;
        }
        showPage(Browse);
    });
    auto* add = Ui::button("Add folder"); il->addWidget(add);
    connect(add, &QPushButton::clicked, this, [this, add] {
        const QString dir = QFileDialog::getExistingDirectory(this, "Add music folder"); if (dir.isEmpty()) return;
        add->setEnabled(false); add->setText("Scanning...");
        auto* watcher = new QFutureWatcher<ScannedTracks>(this);
        connect(watcher, &QFutureWatcher<ScannedTracks>::finished, this, [this, add, watcher] {
            const auto scanned = watcher->result();
            if (!importScannedLibraryTracks(scanned.tracks, scanned.stemsReady))
                QMessageBox::warning(this, "Library scan", "Could not save scanned tracks to the library.");
            add->setText("Add folder"); add->setEnabled(true); watcher->deleteLater(); refreshSource(); refreshGroups();
        });
        watcher->setFuture(QtConcurrent::run([dir] {
            ScannedTracks scanned; auto extensions = Decoder::supportedExtensions() + QStringList{"stem.m4a", "mp4"};
            for (auto& extension : extensions) { extension = extension.toLower(); extension.remove("*."); if (extension.startsWith('.')) extension.remove(0, 1); }
            QDirIterator files(dir, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
            while (files.hasNext()) {
                const auto path = files.next(), name = QFileInfo(path).fileName();
                if (!std::any_of(extensions.cbegin(), extensions.cend(), [&](const QString& ext) { return name.endsWith('.' + ext, Qt::CaseInsensitive); })) continue;
                auto info = Decoder::readTags(path); info.path = path; info.artwork = QImage();
                scanned.tracks.append(info); scanned.stemsReady.append(StemManager::stemsExistFor(path));
            }
            return scanned;
        }));
    });
    row->addWidget(inspector); root->addLayout(row, 1); m_stack->addWidget(page);
    connect(m_sourceList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (!item) return; m_srcName->setText("Source: " + item->text());
        const QString path = item->data(Qt::UserRole).toString();
        const auto tracks = Library::instance().allTracks(); int count = 0; double seconds = 0;
        for (const auto& t : tracks) if (path.isEmpty() || t.path.startsWith(path + '/')) { ++count; seconds += t.durationSec; }
        m_srcSongs->setText(QString("Songs: %1").arg(count)); m_srcTotal->setText("Total: " + Ui::time(seconds));
        m_srcPlaylists->setText(QString("Playlists: %1").arg(Library::instance().playlists().size()));
    });
    connect(m_sourceList, &QListWidget::itemDoubleClicked, open, [open](QListWidgetItem*) { open->click(); });
}

void LibraryWidget::refreshSource() {
    const auto* selected = m_sourceList->currentItem(); const auto path = selected ? selected->data(Qt::UserRole).toString() : QString();
    m_sourceList->clear();
    const auto volumes = Library::removableVolumes();
    QPixmap usbTile(58, 58); usbTile.fill(QColor("#1a79eb"));
    { QPainter tp(&usbTile); tp.setPen(Qt::white); tp.setFont(QFont("Open Sans", 13, QFont::Bold)); tp.drawText(usbTile.rect(), Qt::AlignCenter, "USB"); }
    m_sourceList->setIconSize(QSize(58, 58));
    if (volumes.isEmpty()) {
        auto* empty = new QListWidgetItem(QIcon(usbTile), "No Device", m_sourceList); empty->setSizeHint(QSize(0, 66));
        empty->setFlags(Qt::NoItemFlags);
    }
    for (const auto& volume : volumes) {
        QString label = QStorageInfo(volume).displayName();
        if (RekordboxUsb::isRekordboxUsb(volume)) label += "  (rekordbox)";
        auto* item = new QListWidgetItem(QIcon(usbTile), label, m_sourceList);
        item->setData(Qt::UserRole, volume); item->setSizeHint(QSize(0, 66));
    }
    auto* library = new QListWidgetItem("STEMBOKS LIBRARY   ›", m_sourceList); library->setSizeHint(QSize(0, 66));
    m_sourceList->setCurrentItem(library);
    for (int i = 0; i < m_sourceList->count(); ++i) if (!path.isEmpty() && m_sourceList->item(i)->data(Qt::UserRole).toString() == path) m_sourceList->setCurrentRow(i);
}

void LibraryWidget::buildBrowse() {
    auto* page = new QWidget; auto* root = new QHBoxLayout(page); root->setContentsMargins(0, 0, 0, 0);
    m_navList = new QListWidget; m_navList->setFixedWidth(66); m_navList->setStyleSheet("QListWidget::item { padding:10px 2px; font-size:9px; }");
    for (const auto& category : QStringList{"USB", "PLAYLIST", "ARTIST", "ALBUM", "KEY", "FOLDER", "HISTORY", "MATCHING"}) {
        auto* item = new QListWidgetItem(category, m_navList); item->setSizeHint(QSize(66, 60));
    }
    root->addWidget(m_navList);
    auto* content = new QVBoxLayout; auto* header = new QHBoxLayout;
    auto* back = Ui::button("‹ SOURCE"); header->addWidget(back); connect(back, &QPushButton::clicked, this, [this] { showPage(Source); });
    m_browseHeader = Ui::label("PLAYLIST", "StemboxSourceHeader"); header->addWidget(m_browseHeader, 1);
    auto* create = Ui::button("+ Playlist"); header->addWidget(create);
    connect(create, &QPushButton::clicked, this, [this] {
        bool ok; const QString name = QInputDialog::getText(this, "Create playlist", "Name", QLineEdit::Normal, {}, &ok).trimmed();
        if (ok && !name.isEmpty()) { Library::instance().createPlaylist(name); refreshGroups(); }
    });
    auto* size = Ui::button("A / A / A"); header->addWidget(size);
    connect(size, &QPushButton::clicked, this, [this] { m_fontStep = (m_fontStep + 1) % 3; refreshGroups(); });
    content->addLayout(header);
    auto* targets = new QHBoxLayout; targets->addWidget(Ui::label("LOAD TO", "StemboxCaption"));
    for (int i = 0; i < 4; ++i) {
        auto* b = Ui::button(QString::number(i + 1)); b->setCheckable(true); targets->addWidget(b); m_targets.append(b);
        connect(b, &QPushButton::clicked, this, [this, i] { setTargetDeck(i); });
    }
    auto* remove = Ui::button("Remove from tag list"); m_tagRemove = remove; targets->addWidget(remove); targets->addStretch();
    connect(remove, &QPushButton::clicked, this, [this] {
        const int row = m_table->currentRow(); if (row < 0) return;
        Library::instance().toggleTag(m_table->item(row, 0)->data(Qt::UserRole + 1).toLongLong()); refreshGroups();
    });
    content->addLayout(targets); auto* rows = new QHBoxLayout;
    m_groupList = new QListWidget; m_groupList->setMaximumWidth(240); rows->addWidget(m_groupList);
    m_table = new QTableWidget; rows->addWidget(m_table, 1); content->addLayout(rows, 1); root->addLayout(content, 1); m_stack->addWidget(page);
    connect(m_navList, &QListWidget::currentTextChanged, this, [this](const QString& category) {
        if (category == "USB") { showPage(Source); return; }
        m_page = Browse; m_category = category; refreshGroups();
    });
    connect(m_groupList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        auto& l = Library::instance(); QVector<LibraryTrack> tracks;
        if (m_page == TagList) tracks = l.tagList();
        else if (m_category == "PLAYLIST") tracks = item && item->data(Qt::UserRole).toLongLong() >= 0 ? l.playlistTracks(item->data(Qt::UserRole).toLongLong()) : l.allTracks();
        else if (m_category == "ARTIST") tracks = item ? l.tracksByArtist(item->text()) : l.allTracks();
        else if (m_category == "ALBUM") tracks = item ? l.tracksByAlbum(item->text()) : l.allTracks();
        else if (m_category == "KEY") tracks = item ? l.tracksByKey(item->text()) : l.allTracks();
        else if (m_category == "HISTORY") tracks = l.history();
        else {
            tracks = l.allTracks();
            if (m_category == "FOLDER" && item) {
                const QString dir = item->text(); tracks.erase(std::remove_if(tracks.begin(), tracks.end(), [&](const LibraryTrack& t) { return QFileInfo(t.path).absolutePath() != dir; }), tracks.end());
            } else if (m_category == "MATCHING") {
                const auto ref = AudioEngine::instance().deck(0)->track();
                tracks.erase(std::remove_if(tracks.begin(), tracks.end(), [&](const LibraryTrack& t) {
                    return !ref || !Ui::compatible(t.keyText, ref->analysis.camelot()) || ref->analysis.bpm <= 0 || std::abs(t.bpm / ref->analysis.bpm - 1) > 0.06;
                }), tracks.end());
            }
        }
        if (!m_sourceRoot.isEmpty()) tracks.erase(std::remove_if(tracks.begin(), tracks.end(), [this](const LibraryTrack& t) { return !t.path.startsWith(m_sourceRoot + '/'); }), tracks.end());
        showTracks(tracks, m_table);
    });
    setTargetDeck(0); m_category = "PLAYLIST";
}

void LibraryWidget::refreshGroups() {
    const QString selection = m_groupList->currentItem() ? m_groupList->currentItem()->text() : QString();
    m_browseHeader->setText(m_page == TagList ? "TAG LIST" : m_category); m_tagRemove->setVisible(m_page == TagList);
    m_groupList->setVisible(m_page != TagList && m_category != "HISTORY" && m_category != "MATCHING");
    m_groupList->clear(); auto& library = Library::instance();
    if (m_page == TagList) { showTracks(library.tagList(), m_table); return; }
    if (m_category == "PLAYLIST") {
        auto* all = new QListWidgetItem("All tracks", m_groupList); all->setData(Qt::UserRole, -1);
        for (const auto& list : library.playlists()) if (list.name != "__taglist__") {
            auto* item = new QListWidgetItem(list.name, m_groupList); item->setData(Qt::UserRole, list.id);
        }
    } else if (m_category == "ARTIST") m_groupList->addItems(library.artists());
    else if (m_category == "ALBUM") m_groupList->addItems(library.albums());
    else if (m_category == "KEY") m_groupList->addItems(library.keys());
    else if (m_category == "FOLDER") {
        QSet<QString> directories; for (const auto& t : library.allTracks()) directories.insert(QFileInfo(t.path).absolutePath());
        QStringList dirs = directories.values(); dirs.sort(); m_groupList->addItems(dirs);
    } else m_groupList->addItem(m_category);
    int row = 0; for (int i = 0; i < m_groupList->count(); ++i) if (m_groupList->item(i)->text() == selection) row = i;
    if (m_groupList->count()) m_groupList->setCurrentRow(row); else showTracks({}, m_table);
}

void LibraryWidget::buildSearch() {
    auto* page = new QWidget; auto* root = new QVBoxLayout(page); auto* header = new QHBoxLayout;
    auto* back = Ui::button("‹ BACK"); header->addWidget(back); connect(back, &QPushButton::clicked, this, [this] { showPage(Source); });
    m_searchEdit = new QLineEdit; m_searchEdit->setPlaceholderText("Search tracks, artists, albums"); header->addWidget(m_searchEdit, 1);
    auto* size = Ui::button("A / A / A"); header->addWidget(size);
    connect(size, &QPushButton::clicked, this, [this] { m_fontStep = (m_fontStep + 1) % 3; showTracks(Library::instance().allTracks(m_searchEdit->text()), m_searchTable); });
    root->addLayout(header); m_searchTable = new QTableWidget; root->addWidget(m_searchTable, 1);
    m_keyboard = new QWidget; root->addWidget(m_keyboard); buildKeyboard(AppSettings::instance().getInt("[Stembox]", "keyboard_azerty") != 0);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& query) { showTracks(Library::instance().allTracks(query), m_searchTable); });
    m_stack->addWidget(page);
}

void LibraryWidget::buildKeyboard(bool azerty) {
    qDeleteAll(m_keyboard->findChildren<QPushButton*>());
    if (auto* layout = m_keyboard->layout()) {
        while (auto* item = layout->takeAt(0)) { delete item->widget(); delete item; } delete layout;
    }
    auto* grid = new QVBoxLayout(m_keyboard); grid->setSpacing(4); grid->setContentsMargins(4, 4, 4, 4);
    m_keyboard->setFixedHeight(4 * 48 + 8);
    const QStringList rows = azerty ? QStringList{"AZERTYUIOP", "QSDFGHJKLM", "WXCVBN"} : QStringList{"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
    for (int row = 0; row < rows.size(); ++row) {
        auto* rowLayout = new QHBoxLayout; rowLayout->setSpacing(4);
        for (int col = 0; col < rows[row].size(); ++col) {
            const QString ch(rows[row][col]); auto* b = Ui::button(ch, "StemboxKeyboardKey"); b->setFixedHeight(44); rowLayout->addWidget(b, 1);
            connect(b, &QPushButton::clicked, this, [this, ch] { m_searchEdit->insert(ch.toLower()); });
        }
        grid->addLayout(rowLayout);
    }
    auto* actionLayout = new QHBoxLayout; actionLayout->setSpacing(4); grid->addLayout(actionLayout);
    const QStringList actions{"123", "CLEAR", "SPACE", "⌫"};
    for (int i = 0; i < actions.size(); ++i) {
        auto* b = Ui::button(actions[i], "StemboxKeyboardKey"); b->setFixedHeight(44); actionLayout->addWidget(b, 1);
        connect(b, &QPushButton::clicked, this, [this, i, azerty] {
            if (i == 1) m_searchEdit->clear(); else if (i == 2) m_searchEdit->insert(" "); else if (i == 3) m_searchEdit->backspace();
            else {
                const auto buttons = m_keyboard->findChildren<QPushButton*>(); const bool digits = m_keyboard->property("digits").toBool();
                const QString letters = azerty ? "AZERTYUIOPQSDFGHJKLMWXCVBN" : "QWERTYUIOPASDFGHJKLZXCVBNM";
                for (int j = 0; j < letters.size() && j < buttons.size(); ++j) buttons[j]->setText(digits ? QString(letters[j]) : QString("1234567890.-/@_+!?&():;=,%")[j % 24]);
                m_keyboard->setProperty("digits", !digits);
            }
        });
    }
    // Read the button text so switching the numeric keyboard also changes insertion.
    const auto keys = m_keyboard->findChildren<QPushButton*>();
    for (auto* b : keys) if (b->text().size() == 1 && b->text() != "⌫") {
        disconnect(b, &QPushButton::clicked, this, nullptr);
        connect(b, &QPushButton::clicked, this, [this, b] { m_searchEdit->insert(b->text().toLower()); });
    }
}

void LibraryWidget::showTracks(const QVector<LibraryTrack>& tracks, QTableWidget* table) {
    const QString selected = table->currentRow() >= 0 && table->item(table->currentRow(), 0) ? table->item(table->currentRow(), 0)->data(Qt::UserRole).toString() : QString();
    table->setSortingEnabled(false); table->clear(); table->setColumnCount(6); table->setRowCount(tracks.size());
    table->setHorizontalHeaderLabels({"#", "TRACK", "TIME", "BPM", "KEY", "STEMS"}); table->verticalHeader()->hide();
    table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->setAlternatingRowColors(true); table->setShowGrid(false);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int col : {0, 2, 3, 4, 5}) table->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    QFont font = table->font(); font.setPointSize(QVector<int>{12, 15, 18}[m_fontStep]); table->setFont(font);
    for (int row = 0; row < tracks.size(); ++row) {
        const auto& t = tracks[row]; const QStringList values{QString::number(row + 1), t.title + " — " + t.artist, Ui::time(t.durationSec), QString::number(t.bpm, 'f', 1), t.keyText, t.stemsReady ? "●" : ""};
        for (int col = 0; col < values.size(); ++col) { auto* item = new QTableWidgetItem(values[col]); item->setToolTip(col == 1 ? t.path : values[col]); table->setItem(row, col, item); }
        table->item(row, 0)->setData(Qt::UserRole, t.path); table->item(row, 0)->setData(Qt::UserRole + 1, t.id);
        table->setRowHeight(row, 36 + m_fontStep * 4); if (t.path == selected) table->selectRow(row);
    }
    if (!table->property("wired").toBool()) {
        table->setProperty("wired", true);
        connect(table, &QTableWidget::cellDoubleClicked, this, [this, table](int row, int) { trackActivated(table, row); });
        table->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(table, &QTableWidget::customContextMenuRequested, this, [this, table](const QPoint& pos) {
            auto* clicked = table->itemAt(pos); if (!clicked) return; const int row = clicked->row(); table->selectRow(row);
            const QString path = table->item(row, 0)->data(Qt::UserRole).toString(); const auto id = table->item(row, 0)->data(Qt::UserRole + 1).toLongLong();
            QMenu menu; for (int i = 0; i < 4; ++i) menu.addAction(QString("Load deck %1").arg(i + 1), this, [this, path, i] { emit loadToDeck(i, path); });
            menu.addSeparator(); menu.addAction(m_page == TagList ? "Remove from Tag List" : "Add to Tag List", this, [id, remove = m_page == TagList] {
                auto& l = Library::instance(); bool found = false; for (const auto& t : l.tagList()) if (t.id == id) found = true;
                if (remove || !found) l.toggleTag(id);
            });
            auto* playlists = menu.addMenu("Add to playlist");
            for (const auto& p : Library::instance().playlists()) if (p.name != "__taglist__") playlists->addAction(p.name, this, [id, p] { Library::instance().addToPlaylist(p.id, id); });
            menu.addAction("Prepare stems", this, [path] { StemManager::instance().enqueue(path); });
            auto* preview = AudioEngine::instance().deck(3);
            auto* previewAction = menu.addAction("Preview on deck 4", this, [this, path, preview] {
                if (preview->playing()) return;
                auto& r = ControlRegistry::instance();
                if (!r.getBool(deckKey(3, "preview.active"))) {
                    r.setSilent(deckKey(3, "preview.previousVolume"), r.get(deckKey(3, "level.volume"), 1));
                    r.setSilent(deckKey(3, "preview.previousPfl"), r.get(deckKey(3, "level.pfl")));
                }
                r.setSilent(deckKey(3, "preview.active"), 1); r.set(deckKey(3, "level.volume"), 0); r.set(deckKey(3, "level.pfl"), 1);
                r.set(deckKey(3, "load.preview"), 1); emit loadToDeck(3, path);
            });
            previewAction->setEnabled(!preview->playing());
            menu.addAction("Stop preview", this, [preview] { preview->pause(); }); menu.exec(table->viewport()->mapToGlobal(pos));
        });
    }
}

void LibraryWidget::setTargetDeck(int deck) {
    if (deck < 0 || deck >= 4) return; m_targetDeck = deck;
    for (int i = 0; i < m_targets.size(); ++i) m_targets[i]->setChecked(i == deck);
}
void LibraryWidget::trackActivated(QTableWidget* table, int row) {
    if (row >= 0 && table->item(row, 0)) emit loadToDeck(m_targetDeck, table->item(row, 0)->data(Qt::UserRole).toString());
}
void LibraryWidget::loadSelected(int deck) {
    auto* table = m_page == Search ? m_searchTable : m_table;
    if (m_page == Hidden || m_page == Source || table->currentRow() < 0) return;
    setTargetDeck(deck); trackActivated(table, table->currentRow());
}
void LibraryWidget::showPage(Page page) {
    m_page = page;
    if (page == Hidden) { emit closed(); return; }
    if (page == Source) { refreshSource(); m_stack->setCurrentIndex(0); }
    else if (page == Search) { m_stack->setCurrentIndex(2); showTracks(Library::instance().allTracks(m_searchEdit->text()), m_searchTable); m_searchEdit->setFocus(); }
    else { m_stack->setCurrentIndex(1); if (m_category.isEmpty()) m_category = "PLAYLIST"; refreshGroups(); }
}
