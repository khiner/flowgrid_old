#pragma once

#include <push2/Push2MidiCommunicator.h>

class Push2Listener {
public:
    virtual ~Push2Listener() = default;
    using Direction = Push2MidiCommunicator::Direction;

    virtual void masterEncoderRotated(float changeAmount) = 0;
    virtual void encoderRotated(int encoderIndex, float changeAmount) = 0;
    virtual void undoButtonPressed(bool shiftHeld) = 0;
    virtual void addTrackButtonPressed(bool shiftHeld) = 0;
    virtual void deleteButtonPressed() = 0;
    virtual void masterButtonPressed() = 0;
    virtual void addDeviceButtonPressed() = 0;
    virtual void aboveScreenButtonPressed(int buttonIndex) = 0;
    virtual void belowScreenButtonPressed(int buttonIndex) = 0;
    virtual void arrowPressed(Direction direction) = 0;
};
