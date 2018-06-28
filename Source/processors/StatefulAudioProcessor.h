#pragma  once

#include "DefaultAudioProcessor.h"
#include <unordered_map>

class StatefulAudioProcessor : public DefaultAudioProcessor {
public:
    StatefulAudioProcessor(int inputChannelCount, int outputChannelCount, UndoManager &undoManager) :
            DefaultAudioProcessor(inputChannelCount, outputChannelCount), state(*this, &undoManager) {
    }

    virtual const String &getParameterIdentifier(int index) = 0;

    AudioProcessorParameter *getParameterByIdentifier(const String& parameterId) {
        return state.getParameter(parameterId);
    }

    AudioProcessorParameter *getParameterByIndex(int parameterIndex) {
        const String& id = getParameterIdentifier(parameterIndex);
        return id != "" ? getParameterByIdentifier(id) : nullptr;
    }

    AudioProcessorValueTreeState *getState() {
        return &state;
    }

    void attachSlider(Slider* slider, const String& parameterId) {
        sliderAttachmentForSliderComponentId.insert(std::make_pair(slider, std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(*getState(), parameterId, *slider)));
    }

    void detachSlider(Slider* slider) {
        if (slider != nullptr) {
            sliderAttachmentForSliderComponentId.erase(slider);
        }
    }

protected:
    AudioProcessorValueTreeState state;
    std::unordered_map<Slider *, std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> > sliderAttachmentForSliderComponentId;
};
