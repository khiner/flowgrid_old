#pragma once

#include <list>
#include "../../JuceLibraryCode/JuceHeader.h"

class ToneSourceWithParameters: AudioProcessorValueTreeState::Listener {
public:
    explicit ToneSourceWithParameters(AudioProcessorValueTreeState& treeState, const String &idSuffix):
            source(new ToneGeneratorAudioSource), treeState(treeState),
            ampParamId("amp_" + idSuffix), freqParamId("freq_" + idSuffix) {
        treeState.createAndAddParameter(ampParamId, "Amp1", "Amp1",
                                        NormalisableRange<float>(0.0f, 1.0f),
                                        0.5f,
                                        [](float value) { return String(value*1000) + "ms"; },
                                        [](const String& text) { return text.getFloatValue()/1000.0f; });
        treeState.createAndAddParameter(freqParamId, "Freq1", "Freq1",
                                        NormalisableRange<float> (440.0f, 10000.0f, 0.0f, 0.3f, false),
                                        880.0f,
                                        [](float value) { return String(value*1000) + "ms"; },
                                        [](const String& text) { return text.getFloatValue()/1000.0f; });

        treeState.addParameterListener(ampParamId, this);
        treeState.addParameterListener(freqParamId, this);
    }

    void parameterChanged(const String& parameterID, float newValue) override {
        if (parameterID == ampParamId) {
            source->setAmplitude(newValue);
        } else if (parameterID == freqParamId) {
            source->setFrequency(newValue);
        }
        if (slider != nullptr) {
            slider->setValue(newValue);
        }
    };

    StringRef getAmpParamId() const {
        return ampParamId;
    }

    StringRef getFreqParamdId() const {
        return freqParamId;
    }

    inline ToneGeneratorAudioSource* get() {
        return source.get();
    }

    void setSlider(Slider* slider) {
        this->slider = slider;
    }

private:
    std::unique_ptr<ToneGeneratorAudioSource> source;
    AudioProcessorValueTreeState& treeState;

    String ampParamId;
    String freqParamId;

    Slider* slider;
};


