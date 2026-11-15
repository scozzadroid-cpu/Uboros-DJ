#include "controllers/midi/androidmidicontroller.h"

#include <jni.h>
#include <time.h>

#include <QCoreApplication>
#include <QJniEnvironment>
#include <algorithm>

#include "controllers/midi/midiutils.h"
#include "moc_androidmidicontroller.cpp"
#include "util/time.h"

namespace {

constexpr const char* kMidiHelperClass = "org/mixxx/MidiHelper";

// Maximum time to wait for MidiManager.openDevice() to call back.
constexpr jint kOpenDeviceTimeoutMillis = 5000;

// Limit the number of packets read per poll() call, so that a misbehaving
// device can't stall the controller thread. Remaining packets are read in
// the next poll cycle.
constexpr int kMaxPacketsPerPoll = 256;

// Upper bound for incoming SysEx messages. Longer messages are dropped.
constexpr int kMaxSysexLength = 64 * 1024;

// MidiDeviceInfo.TYPE_* values
constexpr int kDeviceTypeUsb = 1;
constexpr int kDeviceTypeBluetooth = 3;

constexpr unsigned char kSysexStart = 0xF0;
constexpr unsigned char kSysexEnd = 0xF7;

/// Number of data bytes following a status byte (status < 0xF8, != 0xF0/0xF7).
int dataBytesForStatus(unsigned char status) {
    if (status < 0xF0) {
        switch (status & 0xF0) {
        case 0xC0: // Program Change
        case 0xD0: // Channel Pressure
            return 1;
        default:
            return 2;
        }
    }
    switch (status) {
    case 0xF1: // MIDI Time Code Quarter Frame
    case 0xF3: // Song Select
        return 1;
    case 0xF2: // Song Position Pointer
        return 2;
    default: // 0xF4, 0xF5 (undefined), 0xF6 (Tune Request), real-time
        return 0;
    }
}

/// Converts an AMidi timestamp (CLOCK_MONOTONIC in ns, same as
/// System.nanoTime()) to the mixxx::Time time base.
mixxx::Duration convertTimestamp(int64_t timestampNanos) {
    const mixxx::Duration now = mixxx::Time::elapsed();
    if (timestampNanos <= 0) {
        return now;
    }
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return now;
    }
    const int64_t nowNanos = static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
    const int64_t ageNanos = nowNanos - timestampNanos;
    // Ignore implausible values (in the future or older than 1 s)
    if (ageNanos < 0 || ageNanos > 1000000000LL ||
            mixxx::Duration::fromNanos(ageNanos) > now) {
        return now;
    }
    return now - mixxx::Duration::fromNanos(ageNanos);
}

} // namespace

AndroidMidiController::AndroidMidiController(const AndroidMidiDeviceInfo& deviceInfo)
        : MidiController(deviceInfo.name),
          m_deviceInfo(deviceInfo),
          m_pDevice(nullptr),
          m_pInputPort(nullptr),
          m_pOutputPort(nullptr),
          m_receiveErrorReported(false),
          m_receiveBuffer{},
          m_status(0),
          m_data{0, 0},
          m_dataCount(0),
          m_dataExpected(0),
          m_inSysex(false) {
    // Mixxx input = Android output port (device -> app) and vice versa
    setInputDevice(m_deviceInfo.outputPortNumber >= 0);
    setOutputDevice(m_deviceInfo.inputPortNumber >= 0);
}

AndroidMidiController::~AndroidMidiController() {
    if (isOpen()) {
        close();
    }
    releaseDevice();
}

PhysicalTransportProtocol AndroidMidiController::getPhysicalTransportProtocol() const {
    switch (m_deviceInfo.type) {
    case kDeviceTypeUsb:
        return PhysicalTransportProtocol::USB;
    case kDeviceTypeBluetooth:
        return PhysicalTransportProtocol::BlueTooth;
    default:
        return PhysicalTransportProtocol::UNKNOWN;
    }
}

int AndroidMidiController::open(const QString& resourcePath) {
    if (isOpen()) {
        qCWarning(m_logBase) << "Android MIDI device" << getName() << "already open";
        return -1;
    }
    if (!m_deviceInfo.deviceInfo.isValid()) {
        qCWarning(m_logBase) << "Android MIDI device" << getName() << "has no device info";
        return -1;
    }

    resetParser();
    m_receiveErrorReported = false;

    qCInfo(m_logBase) << "AndroidMidiController: Opening" << getName()
                      << "id" << m_deviceInfo.deviceId
                      << "input port" << m_deviceInfo.inputPortNumber
                      << "output port" << m_deviceInfo.outputPortNumber;

    // Attaches the current thread to the JVM if needed
    QJniEnvironment env;
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    // This blocks until MidiManager.openDevice() has called back. The
    // callback is delivered on a dedicated Java HandlerThread, so this must
    // not (and does not) run on the Android main thread.
    m_javaDevice = QJniObject::callStaticObjectMethod(kMidiHelperClass,
            "openDevice",
            "(Landroid/content/Context;Landroid/media/midi/MidiDeviceInfo;I)"
            "Landroid/media/midi/MidiDevice;",
            context.object<jobject>(),
            m_deviceInfo.deviceInfo.object<jobject>(),
            kOpenDeviceTimeoutMillis);
    if (!m_javaDevice.isValid()) {
        qCWarning(m_logBase) << "Unable to open Android MIDI device" << getName();
        return -2;
    }

    media_status_t status = AMidiDevice_fromJava(
            env.jniEnv(), m_javaDevice.object<jobject>(), &m_pDevice);
    if (status != AMEDIA_OK || !m_pDevice) {
        qCWarning(m_logBase) << "AMidiDevice_fromJava failed for" << getName()
                             << "error" << static_cast<int>(status);
        m_pDevice = nullptr;
        releaseDevice();
        return -2;
    }

    if (isInputDevice()) {
        status = AMidiOutputPort_open(m_pDevice,
                m_deviceInfo.outputPortNumber,
                &m_pOutputPort);
        if (status != AMEDIA_OK || !m_pOutputPort) {
            qCWarning(m_logBase) << "AMidiOutputPort_open failed for" << getName()
                                 << "port" << m_deviceInfo.outputPortNumber
                                 << "error" << static_cast<int>(status);
            m_pOutputPort = nullptr;
            releaseDevice();
            return -2;
        }
    }
    if (isOutputDevice()) {
        status = AMidiInputPort_open(m_pDevice,
                m_deviceInfo.inputPortNumber,
                &m_pInputPort);
        if (status != AMEDIA_OK || !m_pInputPort) {
            qCWarning(m_logBase) << "AMidiInputPort_open failed for" << getName()
                                 << "port" << m_deviceInfo.inputPortNumber
                                 << "error" << static_cast<int>(status);
            m_pInputPort = nullptr;
            releaseDevice();
            return -2;
        }
    }

    startEngine();
    // The ports are open at this point. Mark the device open before applying
    // the mapping, otherwise the initial LED states sent by the mapping's
    // output handlers and init() are dropped ("not open for output").
    setOpen(true);
    applyMapping(resourcePath);
    return 0;
}

int AndroidMidiController::close() {
    if (!isOpen()) {
        qCWarning(m_logBase) << "Android MIDI device" << getName() << "already closed";
        return -1;
    }

    stopEngine();
    MidiController::close();

    releaseDevice();
    resetParser();

    setOpen(false);
    return 0;
}

void AndroidMidiController::releaseDevice() {
    // AMidiDevice_release() and MidiDevice.close() need a JVM-attached thread
    QJniEnvironment env;
    if (m_pOutputPort) {
        AMidiOutputPort_close(m_pOutputPort);
        m_pOutputPort = nullptr;
    }
    if (m_pInputPort) {
        AMidiInputPort_close(m_pInputPort);
        m_pInputPort = nullptr;
    }
    if (m_pDevice) {
        media_status_t status = AMidiDevice_release(m_pDevice);
        if (status != AMEDIA_OK) {
            qCWarning(m_logBase) << "AMidiDevice_release failed for" << getName()
                                 << "error" << static_cast<int>(status);
        }
        m_pDevice = nullptr;
    }
    if (m_javaDevice.isValid()) {
        QJniObject::callStaticMethod<void>(kMidiHelperClass,
                "closeDevice",
                "(Landroid/media/midi/MidiDevice;)V",
                m_javaDevice.object<jobject>());
        m_javaDevice = QJniObject();
    }
}

bool AndroidMidiController::poll() {
    if (!m_pOutputPort) {
        return false;
    }

    int numPackets = 0;
    while (numPackets < kMaxPacketsPerPoll) {
        int32_t opcode = 0;
        std::size_t numBytes = 0;
        int64_t timestampNanos = 0;
        const ssize_t result = AMidiOutputPort_receive(m_pOutputPort,
                &opcode,
                m_receiveBuffer.data(),
                m_receiveBuffer.size(),
                &numBytes,
                &timestampNanos);
        if (result < 0) {
            // E.g. the device has been unplugged. Report only once.
            if (!m_receiveErrorReported) {
                qCWarning(m_logInput) << "AMidiOutputPort_receive failed for"
                                      << getName() << "error" << static_cast<qint64>(result);
                m_receiveErrorReported = true;
            }
            break;
        }
        if (result == 0) {
            // No more pending packets
            break;
        }
        m_receiveErrorReported = false;
        ++numPackets;
        if (opcode == AMIDI_OPCODE_DATA && numBytes > 0) {
            parseIncomingBytes(m_receiveBuffer.data(),
                    std::min(numBytes, m_receiveBuffer.size()),
                    convertTimestamp(timestampNanos));
        }
        // AMIDI_OPCODE_FLUSH carries no data
    }
    return numPackets > 0;
}

void AndroidMidiController::resetParser() {
    m_status = 0;
    m_data[0] = 0;
    m_data[1] = 0;
    m_dataCount = 0;
    m_dataExpected = 0;
    m_inSysex = false;
    m_sysexBuffer.clear();
}

void AndroidMidiController::parseIncomingBytes(const uint8_t* pData,
        std::size_t length,
        mixxx::Duration timestamp) {
    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char byte = pData[i];

        if (byte >= 0xF8) {
            // System real-time messages may appear anywhere, even inside
            // SysEx, and do not affect running status.
            receivedShortMessage(byte, 0, 0, timestamp);
            continue;
        }

        if (byte == kSysexStart) {
            if (m_inSysex) {
                qCWarning(m_logInput) << "Buggy MIDI device: SysEx interrupted!";
            }
            m_inSysex = true;
            m_sysexBuffer.clear();
            m_sysexBuffer.append(static_cast<char>(byte));
            m_status = 0;
            m_dataCount = 0;
            continue;
        }

        if (byte == kSysexEnd) {
            if (m_inSysex) {
                m_inSysex = false;
                if (m_sysexBuffer.size() < kMaxSysexLength) {
                    m_sysexBuffer.append(static_cast<char>(byte));
                    receive(m_sysexBuffer, timestamp);
                } else {
                    qCWarning(m_logInput) << "Dropping oversized SysEx message of"
                                          << m_sysexBuffer.size() << "bytes";
                }
                m_sysexBuffer.clear();
            }
            // A stray EOX is ignored
            continue;
        }

        if (byte & 0x80) {
            // Channel voice or system common status byte
            if (m_inSysex) {
                // Abort (drop) the current SysEx message
                qCWarning(m_logInput) << "Buggy MIDI device: SysEx interrupted!";
                m_inSysex = false;
                m_sysexBuffer.clear();
            }
            m_status = byte;
            m_dataCount = 0;
            m_dataExpected = dataBytesForStatus(byte);
            if (m_dataExpected == 0) {
                receivedShortMessage(byte, 0, 0, timestamp);
                // System common messages cancel running status
                m_status = 0;
            }
            continue;
        }

        // Data byte
        if (m_inSysex) {
            if (m_sysexBuffer.size() < kMaxSysexLength) {
                m_sysexBuffer.append(static_cast<char>(byte));
            }
            continue;
        }
        if (m_status == 0) {
            // No status (yet) to apply this data byte to
            continue;
        }
        m_data[m_dataCount++] = byte;
        if (m_dataCount >= m_dataExpected) {
            receivedShortMessage(m_status,
                    m_data[0],
                    m_dataExpected > 1 ? m_data[1] : static_cast<unsigned char>(0),
                    timestamp);
            m_dataCount = 0;
            if (m_status >= 0xF0) {
                // No running status for system common messages
                m_status = 0;
            }
        }
    }
}

void AndroidMidiController::sendShortMsg(unsigned char status,
        unsigned char byte1,
        unsigned char byte2) {
    if (!m_pInputPort) {
        return;
    }

    const uint8_t buffer[3] = {status, byte1, byte2};
    std::size_t length = 1;
    if (status >= 0x80 && status < 0xF8 && status != kSysexStart && status != kSysexEnd) {
        length += dataBytesForStatus(status);
    }

    const ssize_t result = AMidiInputPort_send(m_pInputPort, buffer, length);
    if (result == static_cast<ssize_t>(length)) {
        qCDebug(m_logOutput) << QStringLiteral("outgoing: ")
                             << MidiUtils::formatMidiOpCode(getName(),
                                        status,
                                        byte1,
                                        byte2,
                                        MidiUtils::channelFromStatus(status),
                                        MidiUtils::opCodeFromStatus(status));
    } else {
        // Use two qWarnings() to ensure line break works on all operating systems
        qCWarning(m_logOutput) << "Error sending short message"
                               << MidiUtils::formatMidiOpCode(getName(),
                                          status,
                                          byte1,
                                          byte2,
                                          MidiUtils::channelFromStatus(status),
                                          MidiUtils::opCodeFromStatus(status));
        qCWarning(m_logOutput) << "AMidiInputPort_send result:" << static_cast<qint64>(result);
    }
}

bool AndroidMidiController::sendBytes(const QByteArray& data) {
    if (!m_pInputPort || data.isEmpty()) {
        return false;
    }

    // AMidiInputPort_send() splits the data into packets as needed
    const ssize_t result = AMidiInputPort_send(m_pInputPort,
            reinterpret_cast<const uint8_t*>(data.constData()),
            static_cast<std::size_t>(data.size()));
    if (result == static_cast<ssize_t>(data.size())) {
        qCDebug(m_logOutput) << QStringLiteral("outgoing: ")
                             << MidiUtils::formatSysexMessage(getName(), data);
        return true;
    }
    // Use two qWarnings() to ensure line break works on all operating systems
    qCWarning(m_logOutput) << "Error sending SysEx message:"
                           << MidiUtils::formatSysexMessage(getName(), data);
    qCWarning(m_logOutput) << "AMidiInputPort_send result:" << static_cast<qint64>(result)
                           << "of" << data.size() << "bytes";
    return false;
}
