#include "qml/qmlrekordboxexport.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QUrl>
#include <QXmlStreamWriter>
#include <cmath>

#include "library/library.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "moc_qmlrekordboxexport.cpp"
#include "qml/qmllibraryproxy.h"
#include "track/beats.h"
#include "track/track.h"
#include "util/logger.h"

namespace {
using mixxx::qml::QmlLibraryProxy;
const mixxx::Logger kLogger("RekordboxExport");

QSqlDatabase libraryDatabase() {
    Library* pLibrary = QmlLibraryProxy::get();
    return pLibrary->trackCollectionManager()->internalCollection()->database();
}

// rekordbox wants file://localhost/<absolute path>, percent-encoded
QString locationUrl(const QString& path) {
    QString p = QDir::fromNativeSeparators(path);
    if (!p.startsWith(QLatin1Char('/'))) {
        p.prepend(QLatin1Char('/')); // C:/Music -> /C:/Music
    }
    return QStringLiteral("file://localhost") +
            QString::fromLatin1(QUrl::toPercentEncoding(p, "/:"));
}

// Same colors as rekordbox memory/hot cues
struct Rgb {
    int r, g, b;
};
Rgb hotcueColor(int index) {
    static const Rgb palette[] = {{40, 226, 20}, {48, 90, 255}, {224, 100, 27},
            {195, 175, 4}, {222, 68, 207}, {16, 177, 118}, {180, 50, 50}, {0, 180, 220}};
    return palette[index % 8];
}
} // namespace

QmlRekordboxExport::QmlRekordboxExport(QObject* parent)
        : QObject(parent) {
}

QmlRekordboxExport* QmlRekordboxExport::create(QQmlEngine* pQmlEngine, QJSEngine*) {
    if (!mixxx::qml::QmlLibraryProxy::get()) {
        qWarning() << "RekordboxExport: library not registered yet";
        return nullptr;
    }
    return new QmlRekordboxExport(pQmlEngine);
}

void QmlRekordboxExport::setStatus(const QString& status) {
    m_status = status;
    kLogger.info() << status;
    emit statusChanged(status);
}

QStringList QmlRekordboxExport::setNames() const {
    QStringList names;
    QSqlQuery query(libraryDatabase());
    if (query.exec(QStringLiteral("SELECT name FROM crates ORDER BY name COLLATE NOCASE"))) {
        while (query.next()) {
            names << query.value(0).toString();
        }
    }
    if (query.exec(QStringLiteral("SELECT name FROM Playlists WHERE hidden=0 "
                                  "ORDER BY name COLLATE NOCASE"))) {
        while (query.next()) {
            names << query.value(0).toString();
        }
    }
    return names;
}

int QmlRekordboxExport::exportXml(
        const QString& destination, bool copyFiles, const QString& setName) {
    if (m_busy) {
        return 0;
    }
    m_busy = true;
    emit busyChanged(true);
    int exported = 0;
    auto finish = [&]() {
        m_busy = false;
        emit busyChanged(false);
        return exported;
    };

    QDir dest(destination);
    if (!dest.mkpath(QStringLiteral("."))) {
        setStatus(tr("Cannot create %1").arg(destination));
        return finish();
    }
    const QString contentsDir = dest.filePath(QStringLiteral("Contents"));
    if (copyFiles) {
        dest.mkpath(QStringLiteral("Contents"));
    }

    QSqlDatabase db = libraryDatabase();
    TrackCollectionManager* pCollection = mixxx::qml::QmlLibraryProxy::get()->trackCollectionManager();

    // Tracks to export: a crate, a playlist or everything
    QString where = QStringLiteral("l.mixxx_deleted=0");
    QSqlQuery sel(db);
    if (!setName.isEmpty()) {
        where += QStringLiteral(
                " AND (l.id IN (SELECT track_id FROM crate_tracks WHERE crate_id="
                "(SELECT id FROM crates WHERE name=:n)) OR l.id IN (SELECT track_id FROM "
                "PlaylistTracks WHERE playlist_id=(SELECT id FROM Playlists WHERE name=:n)))");
    }
    sel.prepare(QStringLiteral(
            "SELECT l.id, l.artist, l.title, l.album, l.genre, l.year, l.tracknumber, "
            "l.comment, l.duration, l.bitrate, l.samplerate, l.bpm, l.key, l.rating, "
            "l.timesplayed, l.datetime_added, tl.location, tl.filesize "
            "FROM library l JOIN track_locations tl ON l.location=tl.id WHERE ") +
            where + QStringLiteral(" ORDER BY l.id"));
    if (!setName.isEmpty()) {
        sel.bindValue(QStringLiteral(":n"), setName);
    }
    if (!sel.exec()) {
        setStatus(tr("Library query failed: %1").arg(sel.lastError().text()));
        return finish();
    }

    QFile file(dest.filePath(QStringLiteral("rekordbox.xml")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setStatus(tr("Cannot write %1").arg(file.fileName()));
        return finish();
    }

    // Collect first (the XML needs Entries before the tracks)
    struct Row {
        int id;
        QString artist, title, album, genre, year, number, comment, key, added, path;
        double duration, bpm;
        int bitrate, samplerate, rating, plays;
        qint64 size;
        QString outPath;
    };
    QList<Row> rows;
    while (sel.next()) {
        Row r;
        r.id = sel.value(0).toInt();
        r.artist = sel.value(1).toString();
        r.title = sel.value(2).toString();
        r.album = sel.value(3).toString();
        r.genre = sel.value(4).toString();
        r.year = sel.value(5).toString();
        r.number = sel.value(6).toString();
        r.comment = sel.value(7).toString();
        r.duration = sel.value(8).toDouble();
        r.bitrate = sel.value(9).toInt();
        r.samplerate = sel.value(10).toInt();
        r.bpm = sel.value(11).toDouble();
        r.key = sel.value(12).toString();
        r.rating = sel.value(13).toInt();
        r.plays = sel.value(14).toInt();
        r.added = sel.value(15).toString().left(10);
        r.path = sel.value(16).toString();
        r.size = sel.value(17).toLongLong();
        if (QFileInfo::exists(r.path)) {
            rows.append(r);
        }
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument(QStringLiteral("1.0"), true);
    xml.writeStartElement(QStringLiteral("DJ_PLAYLISTS"));
    xml.writeAttribute(QStringLiteral("Version"), QStringLiteral("1.0.0"));
    xml.writeStartElement(QStringLiteral("PRODUCT"));
    xml.writeAttribute(QStringLiteral("Name"), QStringLiteral("Ubøros DJ"));
    xml.writeAttribute(QStringLiteral("Version"), QStringLiteral("1.0"));
    xml.writeAttribute(QStringLiteral("Company"), QStringLiteral("Mixxx"));
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("COLLECTION"));
    xml.writeAttribute(QStringLiteral("Entries"), QString::number(rows.size()));

    QSqlQuery cues(db);
    cues.prepare(QStringLiteral("SELECT type, position, length, hotcue, label FROM cues "
                                "WHERE track_id=:t ORDER BY position"));
    int done = 0;
    for (Row& r : rows) {
        if (++done % 25 == 0) {
            setStatus(tr("Exporting %1 / %2").arg(done).arg(rows.size()));
        }
        QString outPath = r.path;
        if (copyFiles) {
            outPath = QDir(contentsDir).filePath(QFileInfo(r.path).fileName());
            if (!QFileInfo::exists(outPath) && !QFile::copy(r.path, outPath)) {
                kLogger.warning() << "copy failed" << r.path;
                continue;
            }
        }
        xml.writeStartElement(QStringLiteral("TRACK"));
        xml.writeAttribute(QStringLiteral("TrackID"), QString::number(r.id));
        xml.writeAttribute(QStringLiteral("Name"), r.title);
        xml.writeAttribute(QStringLiteral("Artist"), r.artist);
        xml.writeAttribute(QStringLiteral("Album"), r.album);
        xml.writeAttribute(QStringLiteral("Genre"), r.genre);
        xml.writeAttribute(QStringLiteral("Kind"),
                QFileInfo(r.path).suffix().toUpper() + QStringLiteral(" File"));
        xml.writeAttribute(QStringLiteral("Size"), QString::number(r.size));
        xml.writeAttribute(QStringLiteral("TotalTime"), QString::number(qRound(r.duration)));
        xml.writeAttribute(QStringLiteral("TrackNumber"), r.number);
        xml.writeAttribute(QStringLiteral("Year"), r.year);
        xml.writeAttribute(QStringLiteral("AverageBpm"), QString::number(r.bpm, 'f', 2));
        xml.writeAttribute(QStringLiteral("DateAdded"), r.added);
        xml.writeAttribute(QStringLiteral("BitRate"), QString::number(r.bitrate));
        xml.writeAttribute(QStringLiteral("SampleRate"), QString::number(r.samplerate));
        xml.writeAttribute(QStringLiteral("Comments"), r.comment);
        xml.writeAttribute(QStringLiteral("PlayCount"), QString::number(r.plays));
        // Mixxx 0..5 stars -> rekordbox 0..255
        xml.writeAttribute(QStringLiteral("Rating"), QString::number(qBound(0, r.rating, 5) * 51));
        xml.writeAttribute(QStringLiteral("Tonality"), r.key);
        xml.writeAttribute(QStringLiteral("Location"), locationUrl(outPath));

        // Beat grid: first beat and tempo (constant grid)
        double firstBeatSec = 0.0;
        if (r.bpm > 0 && r.samplerate > 0) {
            TrackPointer pTrack = pCollection->getTrackById(TrackId(QVariant(r.id)));
            if (pTrack) {
                mixxx::BeatsPointer pBeats = pTrack->getBeats();
                if (pBeats) {
                    const auto first = pBeats->firstBeat();
                    if (first.isValid()) {
                        firstBeatSec = first.value() / r.samplerate;
                        if (firstBeatSec < 0) {
                            firstBeatSec = 0;
                        }
                    }
                }
            }
            xml.writeStartElement(QStringLiteral("TEMPO"));
            xml.writeAttribute(QStringLiteral("Inizio"), QString::number(firstBeatSec, 'f', 3));
            xml.writeAttribute(QStringLiteral("Bpm"), QString::number(r.bpm, 'f', 2));
            xml.writeAttribute(QStringLiteral("Metro"), QStringLiteral("4/4"));
            xml.writeAttribute(QStringLiteral("Battito"), QStringLiteral("1"));
            xml.writeEndElement();
        }

        // Cues: hot cues (Type 0, Num 0..7), main cue (memory cue), loops (Type 4)
        cues.bindValue(QStringLiteral(":t"), r.id);
        if (cues.exec()) {
            while (cues.next()) {
                const int type = cues.value(0).toInt();
                const double pos = cues.value(1).toDouble() / 2.0; // samples -> frames
                const double len = cues.value(2).toDouble() / 2.0;
                const int hot = cues.value(3).toInt();
                const QString label = cues.value(4).toString();
                if (r.samplerate <= 0 || (type != 1 && type != 2 && type != 4)) {
                    continue;
                }
                const double start = pos / r.samplerate;
                xml.writeStartElement(QStringLiteral("POSITION_MARK"));
                xml.writeAttribute(QStringLiteral("Name"), label);
                xml.writeAttribute(QStringLiteral("Type"), type == 4 ? QStringLiteral("4") : QStringLiteral("0"));
                xml.writeAttribute(QStringLiteral("Start"), QString::number(start, 'f', 3));
                if (type == 4) {
                    xml.writeAttribute(QStringLiteral("End"),
                            QString::number(start + len / r.samplerate, 'f', 3));
                }
                xml.writeAttribute(QStringLiteral("Num"),
                        QString::number(type == 2 || hot < 0 ? -1 : hot));
                if (type == 1 && hot >= 0) {
                    const Rgb c = hotcueColor(hot);
                    xml.writeAttribute(QStringLiteral("Red"), QString::number(c.r));
                    xml.writeAttribute(QStringLiteral("Green"), QString::number(c.g));
                    xml.writeAttribute(QStringLiteral("Blue"), QString::number(c.b));
                }
                xml.writeEndElement();
            }
        }
        xml.writeEndElement(); // TRACK
        r.outPath = outPath;
        ++exported;
    }
    xml.writeEndElement(); // COLLECTION

    // Crates and playlists as a flat folder of playlists
    QSet<int> exportedIds;
    for (const Row& r : rows) {
        if (!r.outPath.isEmpty()) {
            exportedIds.insert(r.id);
        }
    }
    struct Set {
        QString name;
        QList<int> ids;
    };
    QList<Set> sets;
    QSqlQuery sq(db);
    auto loadSets = [&](const QString& listSql, const QString& itemsSql) {
        QSqlQuery names(db);
        if (!names.exec(listSql)) {
            return;
        }
        while (names.next()) {
            Set s;
            s.name = names.value(1).toString();
            if (!setName.isEmpty() && s.name != setName) {
                continue;
            }
            sq.prepare(itemsSql);
            sq.bindValue(QStringLiteral(":i"), names.value(0));
            if (sq.exec()) {
                while (sq.next()) {
                    if (exportedIds.contains(sq.value(0).toInt())) {
                        s.ids << sq.value(0).toInt();
                    }
                }
            }
            if (!s.ids.isEmpty()) {
                sets << s;
            }
        }
    };
    loadSets(QStringLiteral("SELECT id, name FROM crates ORDER BY name COLLATE NOCASE"),
            QStringLiteral("SELECT track_id FROM crate_tracks WHERE crate_id=:i"));
    loadSets(QStringLiteral("SELECT id, name FROM Playlists WHERE hidden=0 ORDER BY name COLLATE NOCASE"),
            QStringLiteral("SELECT track_id FROM PlaylistTracks WHERE playlist_id=:i ORDER BY position"));

    xml.writeStartElement(QStringLiteral("PLAYLISTS"));
    xml.writeStartElement(QStringLiteral("NODE"));
    xml.writeAttribute(QStringLiteral("Type"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("Name"), QStringLiteral("ROOT"));
    xml.writeAttribute(QStringLiteral("Count"), QString::number(sets.size()));
    for (const Set& s : sets) {
        xml.writeStartElement(QStringLiteral("NODE"));
        xml.writeAttribute(QStringLiteral("Name"), s.name);
        xml.writeAttribute(QStringLiteral("Type"), QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("KeyType"), QStringLiteral("0"));
        xml.writeAttribute(QStringLiteral("Entries"), QString::number(s.ids.size()));
        for (int id : s.ids) {
            xml.writeEmptyElement(QStringLiteral("TRACK"));
            xml.writeAttribute(QStringLiteral("Key"), QString::number(id));
        }
        xml.writeEndElement();
    }
    xml.writeEndElement(); // ROOT
    xml.writeEndElement(); // PLAYLISTS
    xml.writeEndElement(); // DJ_PLAYLISTS
    xml.writeEndDocument();
    file.close();

    setStatus(tr("%1 tracks exported to %2").arg(exported).arg(file.fileName()));
    return finish();
}
