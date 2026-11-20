#pragma once

#include <QFileInfo>
#include <QLatin1String>
#include <QString>

namespace mixxx {

/// The name of the removable drive ("1234-ABCD") of a file that lives on
/// "/storage/<name>/..." when that drive is not mounted right now, otherwise an
/// empty string. The internal storage ("/storage/emulated/...", "/storage/self")
/// is never reported. One stat of the volume folder.
inline QString missingStorageVolume(const QString& location) {
    const QLatin1String prefix("/storage/");
    if (!location.startsWith(prefix)) {
        return QString();
    }
    const QString volume = location.mid(prefix.size()).section(QLatin1Char('/'), 0, 0);
    if (volume.isEmpty() || volume == QLatin1String("emulated") ||
            volume == QLatin1String("self")) {
        return QString();
    }
    return QFileInfo::exists(QString(prefix) + volume) ? QString() : volume;
}

} // namespace mixxx
