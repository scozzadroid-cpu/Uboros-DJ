#include "qml/qmllibraryproxy.h"

#include <QAbstractItemModel>
#include <QQmlEngine>
#ifdef __ANDROID__
#include <QCoreApplication>
#include <QJniObject>
#endif

#include "library/library.h"
#include "library/librarytablemodel.h"
#include "moc_qmllibraryproxy.cpp"
#include "qml/qmllibraryproxy.h"
#include "qml/qmllibrarytracklistmodel.h"
#include "qmltrackproxy.h"
#include "track/track.h"
#include "util/assert.h"

namespace mixxx {
namespace qml {

QmlLibraryProxy::QmlLibraryProxy(QObject* parent)
        : QObject(parent) {
}

QmlLibraryTrackListModel* QmlLibraryProxy::model() const {
    return make_qml_owned<QmlLibraryTrackListModel>(
            QList<QmlLibraryTrackListColumn*>{}, s_pLibrary->trackTableModel())
            .get();
}

void QmlLibraryProxy::analyze(const QmlTrackProxy* track) const {
    VERIFY_OR_DEBUG_ASSERT(track && track->internal()) {
        return;
    }
    emit s_pLibrary->analyzeTracks({track->internal()->getId()});
}

bool QmlLibraryProxy::importUsbDrives() const {
#ifdef __ANDROID__
    // Same folder as the one searched by RekordboxFeature
    const QString destRoot = QStringLiteral("/storage/emulated/0/RekordboxUSB");
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    QJniObject::callStaticMethod<void>("org/mixxx/UsbImporter",
            "start",
            "(Landroid/content/Context;Ljava/lang/String;)V",
            context.object<jobject>(),
            QJniObject::fromString(destRoot).object<jstring>());
    return true;
#else
    return false;
#endif
}

// static
QmlLibraryProxy* QmlLibraryProxy::create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine) {
    // The implementation of this method is mostly taken from the code example
    // that shows the replacement for `qmlRegisterSingletonInstance()` when
    // using `QML_SINGLETON`.
    // https://doc.qt.io/qt-6/qqmlengine.html#QML_SINGLETON

    // The instance has to exist before it is used. We cannot replace it.
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        qWarning() << "Library hasn't been registered yet";
        return nullptr;
    }
    return new QmlLibraryProxy(pQmlEngine);
}

} // namespace qml
} // namespace mixxx
