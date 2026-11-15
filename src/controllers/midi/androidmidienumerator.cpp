#include "controllers/midi/androidmidienumerator.h"

#include <jni.h>

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <algorithm>

#include "controllers/midi/androidmidicontroller.h"
#include "moc_androidmidienumerator.cpp"

namespace {

constexpr const char* kMidiHelperClass = "org/mixxx/MidiHelper";
constexpr const char* kMidiDeviceInfoSignature = "(Landroid/media/midi/MidiDeviceInfo;)";

// MidiDeviceInfo.PortInfo.TYPE_INPUT / TYPE_OUTPUT
constexpr jint kPortTypeInput = 1;
constexpr jint kPortTypeOutput = 2;

QString getStringProperty(const QJniObject& info, const char* method) {
    const QByteArray signature =
            QByteArray(kMidiDeviceInfoSignature) + "Ljava/lang/String;";
    return QJniObject::callStaticObjectMethod(kMidiHelperClass,
            method,
            signature.constData(),
            info.object<jobject>())
            .toString();
}

jint getIntProperty(const QJniObject& info, const char* method) {
    const QByteArray signature = QByteArray(kMidiDeviceInfoSignature) + "I";
    return QJniObject::callStaticMethod<jint>(kMidiHelperClass,
            method,
            signature.constData(),
            info.object<jobject>());
}

QString getPortName(const QJniObject& info, jint portType, jint portNumber) {
    return QJniObject::callStaticObjectMethod(kMidiHelperClass,
            "getPortName",
            "(Landroid/media/midi/MidiDeviceInfo;II)Ljava/lang/String;",
            info.object<jobject>(),
            portType,
            portNumber)
            .toString();
}

std::optional<uint16_t> toUsbId(jint id) {
    if (id < 0 || id > 0xFFFF) {
        return std::nullopt;
    }
    return static_cast<uint16_t>(id);
}

} // namespace

AndroidMidiEnumerator::AndroidMidiEnumerator()
        : MidiEnumerator() {
}

AndroidMidiEnumerator::~AndroidMidiEnumerator() {
    qDebug() << "Deleting Android MIDI devices...";
    while (!m_devices.isEmpty()) {
        delete m_devices.takeLast();
    }
}

QList<Controller*> AndroidMidiEnumerator::queryDevices() {
    qDebug() << "Scanning Android MIDI devices:";

    while (!m_devices.isEmpty()) {
        delete m_devices.takeLast();
    }

    QJniEnvironment env;
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    QJniObject deviceArray = QJniObject::callStaticObjectMethod(kMidiHelperClass,
            "getDevices",
            "(Landroid/content/Context;)[Landroid/media/midi/MidiDeviceInfo;",
            context.object<jobject>());
    if (!deviceArray.isValid()) {
        qWarning() << "Unable to list Android MIDI devices";
        return m_devices;
    }

    const auto array = deviceArray.object<jobjectArray>();
    const jsize numDevices = env->GetArrayLength(array);
    for (jsize i = 0; i < numDevices; ++i) {
        jobject localInfo = env->GetObjectArrayElement(array, i);
        if (env.checkAndClearExceptions() || !localInfo) {
            continue;
        }
        // Creates a global reference
        QJniObject info(localInfo);
        env->DeleteLocalRef(localInfo);

        AndroidMidiDeviceInfo deviceInfo;
        deviceInfo.deviceInfo = info;
        deviceInfo.deviceId = getIntProperty(info, "getId");
        deviceInfo.type = getIntProperty(info, "getType");
        deviceInfo.name = getStringProperty(info, "getName");
        deviceInfo.manufacturer = getStringProperty(info, "getManufacturer");
        deviceInfo.product = getStringProperty(info, "getProduct");
        deviceInfo.serialNumber = getStringProperty(info, "getSerialNumber");
        deviceInfo.vendorId = toUsbId(getIntProperty(info, "getUsbVendorId"));
        deviceInfo.productId = toUsbId(getIntProperty(info, "getUsbProductId"));

        // Android "input" ports receive data from the app, "output" ports
        // send data to the app.
        const int numInputPorts = getIntProperty(info, "getInputPortCount");
        const int numOutputPorts = getIntProperty(info, "getOutputPortCount");
        const int numPortPairs = std::max(numInputPorts, numOutputPorts);

        qDebug() << " Found Android MIDI device" << deviceInfo.name
                 << "id" << deviceInfo.deviceId << "type" << deviceInfo.type
                 << "inputs" << numInputPorts << "outputs" << numOutputPorts;

        // Pair input port N with output port N. Most controllers (e.g.
        // class compliant USB-MIDI devices) have exactly one of each.
        for (int port = 0; port < numPortPairs; ++port) {
            AndroidMidiDeviceInfo portInfo = deviceInfo;
            portInfo.inputPortNumber = port < numInputPorts ? port : -1;
            portInfo.outputPortNumber = port < numOutputPorts ? port : -1;
            if (numPortPairs > 1) {
                QString portName;
                if (portInfo.outputPortNumber >= 0) {
                    portName = getPortName(info, kPortTypeOutput, portInfo.outputPortNumber);
                }
                if (portName.isEmpty() && portInfo.inputPortNumber >= 0) {
                    portName = getPortName(info, kPortTypeInput, portInfo.inputPortNumber);
                }
                if (portName.isEmpty()) {
                    portName = QString::number(port + 1);
                }
                portInfo.name = QStringLiteral("%1 %2").arg(deviceInfo.name, portName);
            }
            qDebug() << "  Linking input port" << portInfo.inputPortNumber
                     << "and output port" << portInfo.outputPortNumber
                     << "as" << portInfo.name;
            m_devices.push_back(new AndroidMidiController(portInfo));
        }
    }
    return m_devices;
}
