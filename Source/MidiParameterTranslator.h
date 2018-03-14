#pragma once


#include <unordered_map>
#include <push2/Push2MidiCommunicator.h>
#include "AppState.h"

class MidiParameterTranslator {
public:
    typedef Push2MidiCommunicator Push2;
    typedef Push2::ControlLabel CL;
    explicit MidiParameterTranslator(AppState& appState):
            appState(appState),
            parameterForMidiCc {
                    {Push2::ccNumberForControlLabel.at(CL::masterKnob), appState.getMainParameters().getMasterVolumeParameter()},
            }
    {}

    AudioProcessorParameter* translate(const MidiMessage& midiMessage) const {
        if (midiMessage.isController()) {
            return parameterForMidiCc.at(midiMessage.getControllerNumber());
        }
        return nullptr;
    }
private:
    AppState& appState;
    std::unordered_map<int, AudioProcessorParameter*> parameterForMidiCc;
};
