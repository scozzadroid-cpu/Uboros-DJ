#pragma once

#include <QString>

namespace mixxx {
namespace rekordboxconstants {
/// Sub-version of a beat grid read from the rekordbox data of a USB drive
const QString beatsSubversion = QStringLiteral("Rekordbox USB drive");

/// A rekordbox grid that AnalyzerGridAlign measured against the audio:
/// "Rekordbox USB drive, aligned v2 +12.5 ms" (moved 12.5 ms later, the cues
/// with it) or "Rekordbox USB drive, unsure v2" (left as it was: no clear
/// beat in the audio, or the parts of the track disagree).
///
/// The shift is relative to the decoder timing table in rekordboxfeature.cpp:
/// whenever that table changes, change the version here, so that the grids
/// measured with the old table are measured again instead of being moved
/// twice (v1 = table with case A 26 ms, 2026-11-16 23:30).
const QString alignedPrefix = QStringLiteral("Rekordbox USB drive, aligned v2 ");
const QString unsureSubversion = QStringLiteral("Rekordbox USB drive, unsure v2");

inline bool isRekordboxBeats(const QString& subVersion) {
    return subVersion.startsWith(beatsSubversion);
}

/// The result of a measurement with the current timing table
inline bool isMeasured(const QString& subVersion) {
    return subVersion.startsWith(alignedPrefix) || subVersion == unsureSubversion;
}

/// The grid still has to be measured against the audio (never measured, or
/// measured with an older timing table)
inline bool needsAlignment(const QString& subVersion) {
    return isRekordboxBeats(subVersion) && !isMeasured(subVersion);
}

inline double alignmentShiftMs(const QString& subVersion) {
    if (!subVersion.startsWith(alignedPrefix)) {
        return 0.0;
    }
    bool ok = false;
    const double shift = subVersion.mid(alignedPrefix.size())
                                 .section(QLatin1Char(' '), 0, 0)
                                 .toDouble(&ok);
    return ok ? shift : 0.0;
}

inline QString alignedSubversion(double shiftMs) {
    return alignedPrefix +
            (shiftMs >= 0 ? QStringLiteral("+") : QString()) +
            QString::number(shiftMs, 'f', 1) + QStringLiteral(" ms");
}
} // namespace rekordboxconstants
} // namespace mixxx
