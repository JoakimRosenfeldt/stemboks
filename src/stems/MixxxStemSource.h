#pragma once

#include <QString>

namespace stemboks {

// Run off the GUI thread. Returns the original path without stems, a native
// Mixxx stem container when available, or an empty string with an error.
QString prepareMixxxPlaybackPath(const QString& sourcePath, QString* error = nullptr);

} // namespace stemboks
