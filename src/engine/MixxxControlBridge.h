#pragma once
#include <QObject>
#include <memory>
namespace mixxx { class CoreServices; }
namespace stemboks {

class MixxxControlBridge final : public QObject {
    Q_OBJECT
public:
    explicit MixxxControlBridge(::mixxx::CoreServices* core, QObject* parent = nullptr);
    ~MixxxControlBridge() override;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace stemboks
