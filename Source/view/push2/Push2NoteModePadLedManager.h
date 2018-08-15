#pragma once

#include "JuceHeader.h"
#include "Project.h"
#include "push2/Push2MidiCommunicator.h"

class Push2NoteModePadLedManager : public MidiInputCallback {
public:
    Push2NoteModePadLedManager(Project& project, Push2MidiCommunicator& push2) : project(project), push2(push2) {}

    void handleIncomingMidiMessage(MidiInput *source, const MidiMessage &message) override {
        if (message.isNoteOff())
            push2.disablePad(message.getNoteNumber());
        else
            push2.setPadColour(message.getNoteNumber(), Colours::white);
    }

protected:
    Project& project;
    Push2MidiCommunicator& push2;
};
