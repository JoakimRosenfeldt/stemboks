#include "StemsManagerWidget.h"
#include "UiHelpers.h"
#include "../stems/StemManager.h"
#include "../engine/Decoder.h"
#include <QButtonGroup>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QListWidget>
#include <QVBoxLayout>

StemsManagerWidget::StemsManagerWidget(QWidget* parent) : QWidget(parent) {
    setObjectName("LibraryContainer"); auto* root = new QVBoxLayout(this);
    auto* header = new QHBoxLayout; auto* back = Ui::button("‹ BACK"); header->addWidget(back);
    connect(back, &QPushButton::clicked, this, &StemsManagerWidget::closed);
    header->addWidget(Ui::label("STEMS MANAGER", "StemboxSourceHeader"), 1); root->addLayout(header);
    m_modelStatus = Ui::label("", "StemboxAboutLine"); m_modelStatus->setWordWrap(true); root->addWidget(m_modelStatus);
    auto* model = Ui::button("Choose separation model, ONNX"); root->addWidget(model);
    connect(model, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Choose HTDemucs ONNX model", StemManager::instance().modelPath(), "ONNX model (*.onnx)");
        if (!path.isEmpty()) { Ui::setting("[Stembox]", "stems_model_path", path); refresh(); }
    });
    auto* destinations = new QHBoxLayout; auto* sidecar = Ui::button("SIDECAR FOLDER"); auto* folder = Ui::button("STEMS FOLDER");
    auto* group = new QButtonGroup(this); group->setExclusive(true);
    sidecar->setProperty("segment", true); folder->setProperty("segment", true); sidecar->setCheckable(true); folder->setCheckable(true);
    group->addButton(sidecar, 0); group->addButton(folder, 1); destinations->addWidget(sidecar); destinations->addWidget(folder); root->addLayout(destinations);
    const QString saved = AppSettings::instance().get("[Stembox]", "stems_folder"); folder->setChecked(!saved.isEmpty()); sidecar->setChecked(saved.isEmpty());
    auto* path = Ui::button(saved.isEmpty() ? "Choose stems folder" : saved); root->addWidget(path);
    const auto choose = [this, path, sidecar, folder] {
        const QString directory = QFileDialog::getExistingDirectory(this, "Choose stems destination", AppSettings::instance().get("[Stembox]", "stems_folder"));
        if (directory.isEmpty()) { sidecar->setChecked(AppSettings::instance().get("[Stembox]", "stems_folder").isEmpty()); folder->setChecked(!sidecar->isChecked()); return; }
        Ui::setting("[Stembox]", "stems_folder", directory); path->setText(directory); folder->setChecked(true);
    };
    connect(path, &QPushButton::clicked, this, choose);
    connect(group, &QButtonGroup::idClicked, this, [choose, path](int id) {
        if (id == 0) { Ui::setting("[Stembox]", "stems_folder", QString()); path->setText("Choose stems folder"); } else choose();
    });
    auto* actions = new QHBoxLayout; auto* files = Ui::button("Add files"); auto* directory = Ui::button("Add folder"); auto* clear = Ui::button("Clear finished");
    actions->addWidget(files); actions->addWidget(directory); actions->addStretch(); actions->addWidget(clear); root->addLayout(actions);
    connect(files, &QPushButton::clicked, this, [this] {
        for (const auto& path : QFileDialog::getOpenFileNames(this, "Prepare stems", {}, "Audio (*.wav *.mp3 *.flac *.aiff *.aif *.m4a *.mp4 *.ogg)")) StemManager::instance().enqueue(path);
    });
    connect(directory, &QPushButton::clicked, this, [this] {
        const auto folder = QFileDialog::getExistingDirectory(this, "Prepare stems for folder"); if (folder.isEmpty()) return;
        auto extensions = Decoder::supportedExtensions(); for (auto& extension : extensions) { extension = extension.toLower(); extension.remove("*."); if (extension.startsWith('.')) extension.remove(0, 1); }
        QDirIterator files(folder, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (files.hasNext()) { const auto path = files.next(); if (extensions.contains(QFileInfo(path).suffix().toLower())) StemManager::instance().enqueue(path); }
    });
    connect(clear, &QPushButton::clicked, this, [] { StemManager::instance().clearFinished(); });
    m_list = new QListWidget; root->addWidget(m_list, 1);
    connect(&StemManager::instance(), &StemManager::jobsChanged, this, &StemsManagerWidget::refresh);
    connect(&AppSettings::instance(), &AppSettings::changed, this, [this](const QString&, const QString& key) { if (key.startsWith("stems_")) refresh(); }); refresh();
}
void StemsManagerWidget::refresh() {
    QString why; const bool available = StemManager::instance().separationAvailable(&why);
    m_modelStatus->setText(available ? "Model ready: " + StemManager::instance().modelPath() : "Separation unavailable: " + why);
    m_list->clear(); for (const auto& job : StemManager::instance().jobs()) {
        auto* item = new QListWidgetItem(QString("%1\n%2   %3%").arg(QFileInfo(job.trackPath).fileName(), job.status).arg(job.progress), m_list);
        item->setToolTip(job.trackPath); item->setSizeHint(QSize(0, 66));
    }
}
