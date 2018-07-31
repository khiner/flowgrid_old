#pragma once

#include "JuceHeader.h"
#include "DefaultAudioProcessor.h"

class ParameterTypesTestProcessor : public DefaultAudioProcessor {
public:
    explicit ParameterTypesTestProcessor() :
            DefaultAudioProcessor(getPluginDescription()),
            boolParameter(new AudioParameterBool("bool", "Bool", false, "val")),
            switchParameter(new AudioParameterInt("switch", "Switch", 0, 1, 1, "val")),

            intParameter(new AudioParameterInt("int", "Int", 0, 10, 5, "val")),
            choiceParameter(new AudioParameterChoice("choice", "Choice", {"choice 1", "choice 2 (default)", "choice 3"}, 2, "val")),
            floatParameter(new AudioParameterFloat("float", "Float", NormalisableRange<float>(0.0f, 1.0f), 0.5f, "dB",
                                                   AudioProcessorParameter::genericParameter, defaultStringFromDbValue, nullptr)) {
        addParameter(boolParameter);
        addParameter(switchParameter);
        addParameter(intParameter);
        addParameter(choiceParameter);
        addParameter(floatParameter);
    }

    static const String name() { return "Parameter Types Test"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), false, false);
    }

    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override {}

private:
    AudioParameterBool *boolParameter;
    AudioParameterInt *switchParameter;
    AudioParameterInt *intParameter;
    AudioParameterChoice *choiceParameter;
    AudioParameterFloat *floatParameter;
};
