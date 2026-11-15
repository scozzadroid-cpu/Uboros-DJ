#pragma once

#include <amidi/AMidi.h>

#include <QJniObject>
#include <array>
#include <cstdint>
#include <optional>

#include "controllers/midi/midicontroller.h"

/// Description of one MIDI port pair of a device listed by
/// android.media.midi.MidiManager.
///
/// Note that Android names ports from the device's point of view: an "input
/// port" is written to by the app (Mixxx output), an "output port" is read
/// by the app (Mixxx input).
struct AndroidMidiDeviceInfo {
    /// android.media.midi.MidiDeviceInfo
    QJniObject deviceInfo;
    int deviceId = -1;
    /// MidiDeviceInfo.TYPE_USB = 1, TYPE_VIRTUAL = 2, TYPE_BLUETOOTH = 3
    int type = 0;
    QString name;
    QString manufacturer;
    QString product;
    QString serialNumber;
    std::optional<uint16_t> vendorId;
    std::optional<uint16_t> productId;
    /// Android input port number (Mixxx -> device), -1 if none
    int inputPortNumber = -1;
    /// Android output port number (device -> Mixxx), -1 if none
    int outputPortNumber = -1;
};

/// AMidi (NDK, API 29+) based implementation of MidiController for Android.
///
/// The device is opened through the Java MidiManager (see
/// org.mixxx.MidiHelper) and then bridged to the native API with
/// AMidiDevice_fromJava(). Incoming data is read in poll(), which is called
/// by the ControllerManager poll timer on the controller thread, the same way
/// PortMidiController works.
class AndroidMidiController : public MidiController {
    Q_OBJECT
  public:
    explicit AndroidMidiController(const AndroidMidiDeviceInfo& deviceInfo);
    ~AndroidMidiController() override;

    PhysicalTransportProtocol getPhysicalTransportProtocol() const override;

    const AndroidMidiDeviceInfo& deviceInfo() const {
        return m_deviceInfo;
    }

    QString getVendorString() const override {
        return m_deviceInfo.manufacturer;
    }
    QString getProductString() const override {
        return m_deviceInfo.product.isEmpty() ? m_deviceInfo.name : m_deviceInfo.product;
    }
    std::optional<uint16_t> getVendorId() const override {
        return m_deviceInfo.vendorId;
    }
    std::optional<uint16_t> getProductId() const override {
        return m_deviceInfo.productId;
    }
    QString getSerialNumber() const override {
        return m_deviceInfo.serialNumber;
    }
    std::optional<uint8_t> getUsbInterfaceNumber() const override {
        return std::nullopt;
    }

  private slots:
    bool poll() override;

  protected:
    void sendShortMsg(unsigned char status,
            unsigned char byte1,
            unsigned char byte2) override;

  private:
    int open(const QString& resourcePath) override;
    int close() override;

    bool sendBytes(const QByteArray& data) override;

    bool isPolling() const override {
        return true;
    }

    /// Closes the native ports, releases the AMidiDevice and closes the Java
    /// MidiDevice. Safe to call multiple times.
    void releaseDevice();

    /// Feeds a chunk of the incoming MIDI 1.0 byte stream into the parser,
    /// which emits short messages and complete SysEx messages.
    void parseIncomingBytes(const uint8_t* pData,
            std::size_t length,
            mixxx::Duration timestamp);
    void resetParser();

    const AndroidMidiDeviceInfo m_deviceInfo;

    /// android.media.midi.MidiDevice, valid while open
    QJniObject m_javaDevice;
    AMidiDevice* m_pDevice;
    /// Mixxx -> device
    AMidiInputPort* m_pInputPort;
    /// device -> Mixxx
    AMidiOutputPort* m_pOutputPort;
    bool m_receiveErrorReported;

    // Android's MidiReceiver packets carry at most 1024 bytes of payload
    // (MidiPortImpl.MAX_PACKET_DATA_SIZE).
    static constexpr std::size_t kReceiveBufferSize = 1024;
    std::array<uint8_t, kReceiveBufferSize> m_receiveBuffer;

    // Byte stream parser state
    unsigned char m_status;
    unsigned char m_data[2];
    int m_dataCount;
    int m_dataExpected;
    bool m_inSysex;
    QByteArray m_sysexBuffer;
};
