#pragma once

#include "JuceHeader.h"

class Push2Listener : public MidiInputCallback {
public:
    ~Push2Listener() override = default;

    virtual void deviceConnected() = 0;
    virtual void shiftPressed() = 0;
    virtual void shiftReleased() = 0;
    virtual void masterEncoderRotated(float changeAmount) = 0;
    virtual void encoderRotated(int encoderIndex, float changeAmount) = 0;
    virtual void undoButtonPressed() = 0;
    virtual void addTrackButtonPressed() = 0;
    virtual void deleteButtonPressed() = 0;
    virtual void masterButtonPressed() = 0;
    virtual void addDeviceButtonPressed() = 0;
    virtual void mixButtonPressed() = 0;
    virtual void aboveScreenButtonPressed(int buttonIndex) = 0;
    virtual void belowScreenButtonPressed(int buttonIndex) = 0;
    virtual void arrowPressed(int direction) = 0;
    virtual void noteButtonPressed() = 0;
    virtual void sessionButtonPressed() = 0;
};
