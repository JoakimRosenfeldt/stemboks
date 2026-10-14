#pragma once
#include <QWidget>

class QListWidget; class QLabel;

// STEMS MANAGER: destination (sidecar folder vs stems folder), model status,
// queue of separation jobs with progress, add-files buttons.
class StemsManagerWidget : public QWidget {
    Q_OBJECT
public:
    explicit StemsManagerWidget(QWidget* parent = nullptr);
signals:
    void closed();
private:
    QListWidget* m_list; QLabel* m_modelStatus;
    void refresh();
};
