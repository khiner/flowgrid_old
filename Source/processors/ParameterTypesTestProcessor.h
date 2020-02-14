#pragma once

#include "JuceHeader.h"
#include "DefaultAudioProcessor.h"

class ParameterTypesTestProcessor : public DefaultAudioProcessor {
public:
    explicit ParameterTypesTestProcessor() :
            DefaultAudioProcessor(getPluginDescription()),
            boolParameter(new AudioParameterBool("bool", "Bool", false, "val")),
            twoIntParameter(new AudioParameterInt("int2", "Two Ints", 0, 1, 1, "val")),
            threeIntParameter(new AudioParameterInt("int3", "Three Ints", 0, 2, 1, "val")),
            intParameter(new AudioParameterInt("int", "Int", 0, 10, 5, "val")),
            twoChoiceParameter(new AudioParameterChoice("choice2", "Two Choices", {"choice 1", "choice 2 (def)"}, 1, "val")),
            threeChoiceParameter(new AudioParameterChoice("choice3", "Three Choices", {"choice 1", "choice 2 (def)", "choice 3"}, 1, "val")),
            choiceParameter(new AudioParameterChoice("choices", "Choices", {"choice 1", "choice 2", "choice 3 (def)", "choice 4", "choice 5"}, 2, "val")),
            floatParameter(new AudioParameterFloat("float", "Float", NormalisableRange<float>(0.0f, 1.0f), 0.5f, "dB",
                                                   AudioProcessorParameter::genericParameter, defaultStringFromDbValue, nullptr)) {
        addParameter(boolParameter);
        addParameter(twoIntParameter);
        addParameter(threeIntParameter);
        addParameter(intParameter);
        addParameter(twoChoiceParameter);
        addParameter(threeChoiceParameter);
        addParameter(choiceParameter);
        addParameter(floatParameter);
    }

    static const String name() { return "Parameter Types Test"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), false, false);
    }

    void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {}

private:
    AudioParameterBool *boolParameter;
    AudioParameterInt *twoIntParameter;
    AudioParameterInt *threeIntParameter;
    AudioParameterInt *intParameter;
    AudioParameterChoice *twoChoiceParameter;
    AudioParameterChoice *threeChoiceParameter;
    AudioParameterChoice *choiceParameter;
    AudioParameterFloat *floatParameter;
};
