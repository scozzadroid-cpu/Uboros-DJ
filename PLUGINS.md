# Plugins (code that lives outside of this tree)

Some features must not be part of this project (private builds, services we do not
distribute). They are **plugins**: a separate folder with its own git repository, compiled
into the app only when the folder is given to CMake. This tree only has the generic
extension points below; a plugin never needs a change in the files of this tree, so both
can be developed at the same time (the plugin against `src/plugins/pluginhost.h`).

Without `-DUBOROS_PLUGINS_DIR` nothing of a plugin exists in the app: no source, no QML,
no Java, no define. The host itself costs nothing (an empty list, one QML singleton).

## Build

    cd ~/mixxx-android
    ./build.sh build-amidi                      # normal app, no plugin
    ./build.sh build-private -DUBOROS_PLUGINS_DIR=$HOME/mixxx-android/plugins/soundcloud -DENABLE_SELF_UPDATE=OFF

Use a separate build folder per variant (they never invalidate each other). Several plugins:
`-DUBOROS_PLUGINS_DIR="/a;/b"`. `-DUBOROS_APPLICATION_ID=org.x.y` gives the APK another
applicationId (it installs next to the normal app; the plugin's `plugin.cmake` may set it).

## A plugin folder

    plugin.cmake          calls uboros_add_plugin() (cmake/UborosPlugins.cmake)
    src/*.cpp, *.h        C++ (compiled into mixxx-qml-lib, AUTOMOC on, `#include "moc_<file>.cpp"`)
    qml/*.qml             copied to assets/qml/Plugins/<name>/ (Android)
    android/java/         extra Java sources of the APK (optional)
    android/plugin.gradle Gradle file applied to the APK: dependencies... (optional)

`uboros_add_plugin(NAME x SOURCES ... [LIBRARIES ...] [DEFINITIONS ...] [QML_DIR qml]
[ANDROID_JAVA_DIR android/java] [ANDROID_GRADLE android/plugin.gradle])`. The C++ must define
`mixxx::plugins::Plugin* uborosCreatePlugin_<name>();` (global namespace).

## What the host offers (`src/plugins/pluginhost.h`)

- `Plugin::init(UserSettingsPointer)`: once at start-up. Stay idle: no thread, timer or network
  until the user uses the plugin. Register QML types here (e.g. `qmlRegisterSingletonInstance("Mixxx", 1, 0, ...)`).
- `Plugin::createLibrarySources(parent, defaultColumns)`: entries of the library sidebar, shown
  above the USB drives. A source is a `QmlLibrarySource` wrapping a `LibraryFeature`.
- `PluginHost::setLabelHidden(label, hidden)`: hides/shows a sidebar entry by its title (e.g.
  "SoundCloud" until the user has signed in).
- `Plugin::dashboardSections()`: QML files (relative to the QML root, e.g.
  `Plugins/soundcloud/Dashboard.qml`) loaded as sections at the end of the Dashboard
  (`Mixxx.Plugins.dashboardSections`). Import the interface components with `import "../.." as Skin`.

The files of this tree touched for this: `CMakeLists.txt` (2 lines + 2 sources),
`cmake/UborosPlugins.cmake`, `packaging/android/build.gradle` (3 lines at the end),
`src/qml/qmllibrarysourcetree.cpp` (plugin sources before Rekordbox), `src/qml/qmlapplication.cpp`
(1 line: init), `res/qml/Library/Browser.qml` (hidden labels), `res/qml/DashboardMaintenance.qml` (sections).
