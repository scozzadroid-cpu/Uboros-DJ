#include "qml/qmllibrarysourcetree.h"

#include <qalgorithms.h>
#include <qvariant.h>

#include <QAbstractListModel>
#include <QVariant>

#include "library/library.h"
#include "library/librarytablemodel.h"
#include "moc_qmllibrarysourcetree.cpp"
#include "plugins/pluginhost.h"
#include "qml_owned_ptr.h"
#include "qmllibraryproxy.h"
#include "qmllibrarytracklistmodel.h"
#include "qmlsidebarmodelproxy.h"

namespace mixxx {
namespace qml {

QmlLibrarySourceTree::QmlLibrarySourceTree(QQuickItem* parent)
        : QQuickItem(parent),
          m_model(new QmlSidebarModelProxy(this)) {
}
QmlLibrarySourceTree::~QmlLibrarySourceTree() = default;

Q_INVOKABLE QmlLibraryTrackListModel* QmlLibrarySourceTree::allTracks() const {
    return make_qml_owned<QmlLibraryTrackListModel>(
            m_defaultColumns, QmlLibraryProxy::get()->trackTableModel());
};

void QmlLibrarySourceTree::append_source(
        QQmlListProperty<QmlLibrarySource>* list, QmlLibrarySource* source) {
    reinterpret_cast<QList<QmlLibrarySource*>*>(list->data)->append(source);
    QmlLibrarySourceTree* librarySourceTree = qobject_cast<QmlLibrarySourceTree*>(list->object);
    if (librarySourceTree && librarySourceTree->isComponentComplete()) {
        librarySourceTree->m_model->update(librarySourceTree->m_sources);
    }
}

void QmlLibrarySourceTree::clear_source(QQmlListProperty<QmlLibrarySource>* p) {
    reinterpret_cast<QList<QmlLibrarySource*>*>(p->data)->clear();
    QmlLibrarySourceTree* librarySourceTree = qobject_cast<QmlLibrarySourceTree*>(p->object);
    if (librarySourceTree) {
        librarySourceTree->m_model->update(librarySourceTree->m_sources);
    }
}
void QmlLibrarySourceTree::replace_source(QQmlListProperty<QmlLibrarySource>* p,
        qsizetype idx,
        QmlLibrarySource* v) {
    return reinterpret_cast<QList<QmlLibrarySource*>*>(p->data)->replace(idx, v);
    QmlLibrarySourceTree* librarySourceTree = qobject_cast<QmlLibrarySourceTree*>(p->object);
    if (librarySourceTree && librarySourceTree->isComponentComplete()) {
        librarySourceTree->m_model->update(librarySourceTree->m_sources);
    }
}
void QmlLibrarySourceTree::removeLast_source(QQmlListProperty<QmlLibrarySource>* p) {
    return reinterpret_cast<QList<QmlLibrarySource*>*>(p->data)->removeLast();
    QmlLibrarySourceTree* librarySourceTree = qobject_cast<QmlLibrarySourceTree*>(p->object);
    if (librarySourceTree && librarySourceTree->isComponentComplete()) {
        librarySourceTree->m_model->update(librarySourceTree->m_sources);
    }
}

QQmlListProperty<QmlLibrarySource> QmlLibrarySourceTree::sources() {
    return QQmlListProperty<QmlLibrarySource>(this,
            &m_sources,
            &QmlLibrarySourceTree::append_source,
            &QmlLibrarySourceTree::count_source,
            &QmlLibrarySourceTree::at_source,
            &QmlLibrarySourceTree::clear_source,
            &QmlLibrarySourceTree::replace_source,
            &QmlLibrarySourceTree::removeLast_source);
}

void QmlLibrarySourceTree::componentComplete() {
    // Entries of the plugins (src/plugins; none without UBOROS_PLUGINS_DIR),
    // above the USB drives (the Rekordbox source)
    const auto& plugins = plugins::PluginHost::instance().plugins();
    if (!plugins.isEmpty()) {
        qsizetype at = m_sources.size();
        for (qsizetype i = 0; i < m_sources.size(); ++i) {
            if (qobject_cast<QmlLibraryRekordboxSource*>(m_sources.at(i))) {
                at = i;
                break;
            }
        }
        for (plugins::Plugin* pPlugin : plugins) {
            const auto pluginSources = pPlugin->createLibrarySources(this, m_defaultColumns);
            for (QmlLibrarySource* pSource : pluginSources) {
                m_sources.insert(at++, pSource);
            }
        }
    }
    m_model->update(m_sources);
}

} // namespace qml
} // namespace mixxx
