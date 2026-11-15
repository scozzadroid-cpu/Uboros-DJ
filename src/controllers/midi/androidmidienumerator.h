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

  private:
    QList<Controller*> m_devices;
};
