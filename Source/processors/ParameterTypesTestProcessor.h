#pragma once

#include "JuceHeader.h"
#include "DefaultAudioProcessor.h"

class ParameterTypesTestProcessor : public DefaultAudioProcessor {
public:
    explicit ParameterTypesTestProcessor() :
            DefaultAudioProcessor(getPluginDescription()),
            boolParameter(new AudioParameterBool("bool", "Bool", false, "val")),
            floatParameter(new AudioParameterFloat("float", "Float", NormalisableRange<float>(0.0f, 1.0f), 0.5f, "dB",
                                                   AudioProcessorParameter::genericParameter, defaultStringFromDbValue, nullptr)),
            intParameter(new AudioParameterInt("int", "Int", 0, 10, 5, "val")),
            choiceParameter(new AudioParameterChoice("choice", "Choice", {"choice 1", "choice 2 (default)", "choice 3"}, 2, "val")) {
        addParameter(boolParameter);
        addParameter(floatParameter);
        addParameter(intParameter);
        addParameter(choiceParameter);
    }

    ~ParameterTypesTestProcessor() override {
        boolParameter->removeListener(this);
    }

    static const String name() { return "Parameter Types Test"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), false, false);
    }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
    }

    void parameterChanged(AudioProcessorParameter *parameter, float newValue) override {
    }

    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override {
    }

private:
    AudioParameterBool *boolParameter;
    AudioParameterInt *intParameter;
    AudioParameterChoice *choiceParameter;
    AudioParameterFloat *floatParameter;
};
