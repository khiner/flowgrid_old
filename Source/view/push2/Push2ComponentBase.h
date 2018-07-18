#pragma once

#include <push2/Push2MidiCommunicator.h>
#include <Project.h>
#include "JuceHeader.h"

class Push2ComponentBase : public Component {
public:
    Push2ComponentBase(Project& project, Push2MidiCommunicator& push2MidiCommunicator)
            : project(project), push2MidiCommunicator(push2MidiCommunicator) {}
protected:
    Project& project;
    Push2MidiCommunicator& push2MidiCommunicator;
};
