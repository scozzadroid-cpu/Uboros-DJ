#pragma once

#include <QString>

namespace mixxx {

/// Stems prepared for a track of a USB drive are stored on the drive itself,
/// next to the analyses: <drive>/MIXXX/STEMS/<hash>.mxstem, where <hash> is
/// the first 24 hex digits of SHA-1 of the path relative to the drive root
/// (same name as MIXXX/ANLZ/<hash>.mxa). The file is an NI stem MP4 whose
/// timeline matches the original file as Mixxx decodes it, so the track's
/// beat grid and cues apply unchanged. The extension is not an audio one,
/// so CDJs browsing folders don't list it.
///
/// Returns the sidecar path, or an empty string.
QString stemSidecarForTrack(const QString& trackLocation);

/// Whether stems can be played for this track: a stem file itself or a
/// track with a stem sidecar. Cheap enough for every row of a track list
/// (GUI thread only): the drive roots and the MIXXX/STEMS listings are
/// cached, a listing for 10 s.
bool trackHasStems(const QString& trackLocation);

} // namespace mixxx
