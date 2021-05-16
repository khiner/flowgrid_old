#include "Push2MidiCommunicator.h"

using P2 = Push2MidiCommunicator;

// From https://github.com/Ableton/push-interface/blob/master/doc/AbletonPush2MIDIDisplayInterface.asc#Encoders:
//
// The value 0xxxxxx or 1yyyyyy gives the amount of accumulated movement since the last message. The faster you move, the higher the value.
// The value is given as a 7 bit relative value encoded in two’s complement. 0xxxxxx indicates a movement to the right,
// with decimal values from 1 to 63 (in practice, values above 20 are unlikely). 1yyyyyy means movement to the left, with decimal values from 127 to 64.
// The total step count sent for a 360° turn is approx. 210, except for the detented tempo encoder, where one turn is 18 steps.
//
// This function returns a value between -1 and 1 normalized so that (roughly) the magnitude of a full rotation would sum to 1.
// i.e. Turning 210 'steps' to the left would total to ~-1, and turning 210 steps to the right would total ~1.
static float encoderCcMessageToRotationChange(const MidiMessage &message) {
    auto value = float(message.getControllerValue());
    if (value <= 63)
        return value / 210.0f;
    else
        return (value - 128.0f) / 210.0f;
}

static bool isAboveScreenEncoderCcNumber(int ccNumber) {
    return ccNumber >= P2::topKnob3 && ccNumber <= P2::topKnob10;
}

static bool isEncoderCcNumber(int ccNumber) {
    return ccNumber == P2::topKnob1 || ccNumber == P2::topKnob2 || ccNumber == P2::masterKnob || isAboveScreenEncoderCcNumber(ccNumber);
}

static bool isArrowButtonCcNumber(int ccNumber) {
    return ccNumber >= P2::left && ccNumber <= P2::down;
}

static bool isAboveScreenButtonCcNumber(int ccNumber) {
    return ccNumber >= P2::topDisplayButton1 && ccNumber <= P2::topDisplayButton8;
}

static bool isBelowScreenButtonCcNumber(int ccNumber) {
    return ccNumber >= P2::bottomDisplayButton1 && ccNumber <= P2::bottomDisplayButton8;
}

static bool isButtonPressControlMessage(const MidiMessage &message) {
    return message.getControllerValue() == 127;
}

static bool isButtonReleaseControlMessage(const MidiMessage &message) {
    return message.getControllerValue() == 0;
}

static bool isPadNoteNumber(int noteNumber) {
    return noteNumber >= P2::lowestPadNoteNumber && noteNumber <= P2::highestPadNoteNumber;
}

Push2MidiCommunicator::Push2MidiCommunicator(ViewState &view, Push2Colours &push2Colours) : view(view), push2Colours(push2Colours) {}

void Push2MidiCommunicator::initialize() {
    MidiCommunicator::initialize();
    push2Colours.addListener(this);
    registerAllIndexedColours();

    /*
     * https://github.com/Ableton/push-interface/blob/master/doc/AbletonPush2MIDIDisplayInterface.asc#Aftertouch
     * In channel pressure mode (default), the pad with the highest pressure determines the value sent.
     * The pressure range that produces aftertouch is given by the aftertouch threshold pad parameters.
     * The value curve is linear to the pressure and in range 0 to 127. See Pad Parameters.
     * In polyphonic key pressure mode, aftertouch for each pressed key is sent individually.
     * The value is defined by the pad velocity curve and in range 1…​127. See Velocity Curve.
     */
    static const uint8 setAftertouchModePolyphonic[]{0x00, 0x21, 0x1D, 0x01, 0x01, 0x1E, 0x01};
    auto polyphonicAftertouchSysExMessage = MidiMessage::createSysExMessage(setAftertouchModePolyphonic, 7);
    sendMessageChecked(polyphonicAftertouchSysExMessage);

    push2Listener->deviceConnected();
}

Push2MidiCommunicator::~Push2MidiCommunicator() {
    push2Colours.removeListener(this);
}

void Push2MidiCommunicator::handleIncomingMidiMessage(MidiInput *source, const MidiMessage &message) {
    // Only pass non-sysex note messages to listeners if we're in a non-control mode.
    // (Allow note-off messages through in case switch to control mode happened during note events.)
    if (message.isNoteOnOrOff() && isPadNoteNumber(message.getNoteNumber()) && (view.isInNoteMode() || message.isNoteOff())) {
        MidiCommunicator::handleIncomingMidiMessage(source, message);
    }

    MessageManager::callAsync([this, source, message]() {
        if (push2Listener == nullptr) return;

        if (message.isController()) {
            const auto ccNumber = message.getControllerNumber();
            if (isEncoderCcNumber(ccNumber)) {
                float changeAmount = encoderCcMessageToRotationChange(message);
                if (ccNumber == masterKnob) {
                    return push2Listener->masterEncoderRotated(changeAmount / 2.0f);
                }
                if (isAboveScreenEncoderCcNumber(ccNumber)) {
                    return push2Listener->encoderRotated(ccNumber - topKnob3, changeAmount / 2.0f);
                }
                return;
            }

            if (isButtonPressControlMessage(message)) {
                static const Array<int> repeatableButtonCcNumbers{undo, up, down, left, right};
                if (repeatableButtonCcNumbers.contains(ccNumber)) {
                    buttonHoldStopped();
                    currentlyHeldRepeatableButtonCcNumber = ccNumber;
                    startTimer(BUTTON_HOLD_WAIT_FOR_REPEAT_MS);
                }
                return handleButtonPressMidiCcNumber(ccNumber);
            }
            if (isButtonReleaseControlMessage(message)) {
                buttonHoldStopped();
                return handleButtonReleaseMidiCcNumber(ccNumber);
            }
        } else {
            push2Listener->handleIncomingMidiMessage(source, message);
        }
    });
}

static int directionForArrowButtonCcNumber(int ccNumber) {
    jassert(isArrowButtonCcNumber(ccNumber));

    switch (ccNumber) {
        case P2::left:
            return P2::leftArrowDirection;
        case P2::right:
            return P2::rightArrowDirection;
        case P2::up:
            return P2::upArrowDirection;
        default:
            return P2::downArrowDirection;
    }
}

void Push2MidiCommunicator::handleButtonPressMidiCcNumber(int ccNumber) {
    if (isAboveScreenButtonCcNumber(ccNumber))
        return push2Listener->aboveScreenButtonPressed(ccNumber - topDisplayButton1);
    else if (isBelowScreenButtonCcNumber(ccNumber))
        return push2Listener->belowScreenButtonPressed(ccNumber - bottomDisplayButton1);
    else if (isArrowButtonCcNumber(ccNumber))
        return push2Listener->arrowPressed(directionForArrowButtonCcNumber(ccNumber));
    switch (ccNumber) {
        case shift:
            push2Listener->shiftPressed();
            return;
        case undo:
            return push2Listener->undoButtonPressed();
        case delete_:
            return push2Listener->deleteButtonPressed();
        case duplicate:
            return push2Listener->duplicateButtonPressed();
        case addTrack:
            return push2Listener->addTrackButtonPressed();
        case addDevice:
            return push2Listener->addDeviceButtonPressed();
        case mix:
            return push2Listener->mixButtonPressed();
        case master:
            return push2Listener->masterButtonPressed();
        case note:
            return push2Listener->noteButtonPressed();
        case session:
            return push2Listener->sessionButtonPressed();
        default:
            return;
    }
}

void Push2MidiCommunicator::handleButtonReleaseMidiCcNumber(int ccNumber) {
    switch (ccNumber) {
        case shift:
            push2Listener->shiftReleased();
            return;
        default:
            return;
    }
}

void Push2MidiCommunicator::setAboveScreenButtonColour(int buttonIndex, const Colour &colour) {
    setButtonColour(topDisplayButton1 + buttonIndex, colour);
}

void Push2MidiCommunicator::setBelowScreenButtonColour(int buttonIndex, const Colour &colour) {
    setButtonColour(bottomDisplayButton1 + buttonIndex, colour);
}

void Push2MidiCommunicator::setAboveScreenButtonEnabled(int buttonIndex, bool enabled) {
    setColourButtonEnabled(topDisplayButton1 + buttonIndex, enabled);
}

void Push2MidiCommunicator::setBelowScreenButtonEnabled(int buttonIndex, bool enabled) {
    setColourButtonEnabled(bottomDisplayButton1 + buttonIndex, enabled);
}

void Push2MidiCommunicator::enableWhiteLedButton(int buttonCcNumber) const {
    sendMessageChecked(MidiMessage::controllerEvent(NO_ANIMATION_LED_CHANNEL, buttonCcNumber, 14));
}

void Push2MidiCommunicator::disableWhiteLedButton(int buttonCcNumber) const {
    sendMessageChecked(MidiMessage::controllerEvent(NO_ANIMATION_LED_CHANNEL, buttonCcNumber, 0));
}

void Push2MidiCommunicator::activateWhiteLedButton(int buttonCcNumber) const {
    sendMessageChecked(MidiMessage::controllerEvent(NO_ANIMATION_LED_CHANNEL, buttonCcNumber, 127));
}

void Push2MidiCommunicator::setColourButtonEnabled(int buttonCcNumber, bool enabled) {
    if (enabled)
        setButtonColour(buttonCcNumber, Colours::white);
    else
        disableWhiteLedButton(buttonCcNumber);
}

void Push2MidiCommunicator::setButtonColour(int buttonCcNumber, const Colour &colour) {
    auto colourIndex = push2Colours.findIndexForColourAddingIfNeeded(colour);
    sendMessageChecked(MidiMessage::controllerEvent(NO_ANIMATION_LED_CHANNEL, buttonCcNumber, colourIndex));
}

void Push2MidiCommunicator::disablePad(int noteNumber) {
    if (!isPadNoteNumber(noteNumber)) return;

    sendMessageChecked(MidiMessage::noteOn(NO_ANIMATION_LED_CHANNEL, noteNumber, uint8(0)));
}

void Push2MidiCommunicator::setPadColour(int noteNumber, const Colour &colour) {
    if (!isPadNoteNumber(noteNumber)) return;

    auto colourIndex = push2Colours.findIndexForColourAddingIfNeeded(colour);
    sendMessageChecked(MidiMessage::noteOn(NO_ANIMATION_LED_CHANNEL, noteNumber, colourIndex));
}

void Push2MidiCommunicator::sendMessageChecked(const MidiMessage &message) const {
    if (isOutputConnected())
        midiOutput->sendMessageNow(message);
}

void Push2MidiCommunicator::registerAllIndexedColours() {
    for (auto &pair : push2Colours.indexForColour) {
        colourAdded(Colour::fromString(pair.first), pair.second);
    }
}

void Push2MidiCommunicator::colourAdded(const Colour &colour, uint8 colourIndex) {
    jassert(colourIndex > 0 && colourIndex < CHAR_MAX - 1);

    uint32 argb = colour.getARGB();
    // 8 bytes: 2 for each of R, G, B, W. First byte contains the 7 LSBs; Second byte contains the 1 MSB.
    uint8 bgra[8];

    for (int i = 0; i < 4; i++) {
        auto c = static_cast<uint8>(argb >> (i * CHAR_BIT));
        bgra[i * 2] = static_cast<uint8>(c & 0x7F);
        bgra[i * 2 + 1] = static_cast<uint8>(c & 0x80) >> 7;
    }

    const uint8 setLedColourPaletteEntryCommand[]{0x00, 0x21, 0x1D, 0x01, 0x01, 0x03, colourIndex,
                                                  bgra[4], bgra[5], bgra[2], bgra[3], bgra[0], bgra[1], bgra[6], bgra[7]};
    auto setLedColourPaletteEntryMessage = MidiMessage::createSysExMessage(setLedColourPaletteEntryCommand, 15);
    sendMessageChecked(setLedColourPaletteEntryMessage);
    static const uint8 reapplyColorPaletteCommand[]{0x00, 0x21, 0x1D, 0x01, 0x01, 0x05};
    sendMessageChecked(MidiMessage::createSysExMessage(reapplyColorPaletteCommand, 6));
}

void Push2MidiCommunicator::buttonHoldStopped() {
    currentlyHeldRepeatableButtonCcNumber = 0;
    holdRepeatIsHappeningNow = false;
    stopTimer();
}

void Push2MidiCommunicator::timerCallback() {
    if (currentlyHeldRepeatableButtonCcNumber != 0) {
        handleButtonPressMidiCcNumber(currentlyHeldRepeatableButtonCcNumber);
        if (!holdRepeatIsHappeningNow) {
            holdRepeatIsHappeningNow = true;
            // after the initial wait for a long hold, start sending the message repeatedly during hold
            startTimerHz(BUTTON_HOLD_REPEAT_HZ);
        }
    } else {
        buttonHoldStopped();
    }
}

uint8 Push2MidiCommunicator::ccNumberForArrowButton(int direction) {
    switch (direction) {
        case upArrowDirection:
            return up;
        case downArrowDirection:
            return down;
        case leftArrowDirection:
            return left;
        case rightArrowDirection:
            return right;
        default:
            return 0;
    }
}
