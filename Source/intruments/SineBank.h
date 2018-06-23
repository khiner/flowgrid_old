#pragma once

#include <audio_sources/ToneSourceWithParameters.h>
#include "Instrument.h"
#include "JuceHeader.h"

class SineBank : public Instrument {
public:
    explicit SineBank(AudioProcessorValueTreeState &treeState) :
            treeState(&treeState),
            toneSource1(treeState, "1"),
            toneSource2(treeState, "2"),
            toneSource3(treeState, "3"),
            toneSource4(treeState, "4") {

        mixerAudioSource.addInputSource(toneSource1.get(), false);
        mixerAudioSource.addInputSource(toneSource2.get(), false);
        mixerAudioSource.addInputSource(toneSource3.get(), false);
        mixerAudioSource.addInputSource(toneSource4.get(), false);

        for (int sliderIndex = 0; sliderIndex < 8; sliderIndex++) {
            auto slider = std::make_unique<Slider>();
            auto sliderAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(treeState, getParameterId(sliderIndex), *slider);
            sliders.push_back(std::move(slider));
            sliderAttachments.push_back(std::move(sliderAttachment));
        }
    }

    int getNumParameters() override {
        return 8;
    }

    String getParameterId(int parameterIndex) override {
        switch (parameterIndex) {
            case 0: return toneSource1.getAmpParamId();
            case 1: return toneSource1.getFreqParamId();
            case 2: return toneSource2.getAmpParamId();
            case 3: return toneSource2.getFreqParamId();
            case 4: return toneSource3.getAmpParamId();
            case 5: return toneSource3.getFreqParamId();
            case 6: return toneSource4.getAmpParamId();
            case 7: return toneSource4.getFreqParamId();
            default: return "";
        }
    }

    Slider* getSlider(int parameterIndex) override {
        return sliders[parameterIndex].get();
    }

    AudioSource* getAudioSource() override {
        return &mixerAudioSource;
    }

private:
    AudioProcessorValueTreeState *treeState;
    std::vector<std::unique_ptr<Slider> > sliders;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> > sliderAttachments;

    ToneSourceWithParameters toneSource1;
    ToneSourceWithParameters toneSource2;
    ToneSourceWithParameters toneSource3;
    ToneSourceWithParameters toneSource4;

    MixerAudioSource mixerAudioSource;
};

