#pragma once

#include "controllers/midi/midienumerator.h"

/// Handles discovery and enumeration of MIDI controllers listed by the
/// Android MidiManager. The devices are accessed with the NDK AMidi API.
class AndroidMidiEnumerator : public MidiEnumerator {
    Q_OBJECT
  public:
    AndroidMidiEnumerator();
    ~AndroidMidiEnumerator() override;

    QList<Controller*> queryDevices() override;

    /// Counter that changes whenever Android reports an added or removed
    /// MIDI device. Cheap to call; used for hotplug polling.
    static int deviceGeneration();

  private:
    QList<Controller*> m_devices;
    /// Controllers of unplugged devices. They are kept alive (closed) until
    /// shutdown because other components may still hold pointers to them.
    QList<Controller*> m_removedDevices;
};
