#pragma once

#include <push2/Push2MidiCommunicator.h>

class Push2Listener {
public:
    virtual ~Push2Listener() = default;
    using Direction = Push2MidiCommunicator::Direction;

    virtual void shiftPressed() = 0;
    virtual void shiftReleased() = 0;
    virtual void masterEncoderRotated(float changeAmount) = 0;
    virtual void encoderRotated(int encoderIndex, float changeAmount) = 0;
    virtual void undoButtonPressed() = 0;
    virtual void addTrackButtonPressed() = 0;
    virtual void deleteButtonPressed() = 0;
    virtual void masterButtonPressed() = 0;
    virtual void addDeviceButtonPressed() = 0;
    virtual void aboveScreenButtonPressed(int buttonIndex) = 0;
    virtual void belowScreenButtonPressed(int buttonIndex) = 0;
    virtual void arrowPressed(Direction direction) = 0;
};
