#include <push2/Push2Display.h>
#include "Push2ProcessorViewComponent.h"

Push2ProcessorViewComponent::Push2ProcessorViewComponent() {
    setSize(Push2Display::WIDTH, Push2Display::HEIGHT);
}

void Push2ProcessorViewComponent::setStatefulAudioProcessor(StatefulAudioProcessor *statefulAudioProcessor) {
    removeAllChildren();

    sliderAttachments.clear();
    labels.clear();
    sliders.clear();

    for (int sliderIndex = 0; sliderIndex < statefulAudioProcessor->getNumParameters(); sliderIndex++) {
        auto slider = std::make_unique<Slider>();
        const String &parameterId = statefulAudioProcessor->getParameterID(sliderIndex);
        auto sliderAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(*(statefulAudioProcessor->getState()), parameterId, *slider);

        auto label = std::make_unique<Label>();
        label->setText(statefulAudioProcessor->getState()->getParameter(parameterId)->name, dontSendNotification);
        addAndMakeVisible(*slider);
        addAndMakeVisible(*label);
        slider->setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
        slider->setBounds(sliderIndex * Push2Display::WIDTH / 8, 20, Push2Display::WIDTH / 8, Push2Display::WIDTH / 8);
        slider->setTextBoxStyle(Slider::TextEntryBoxPosition::TextBoxBelow, false, Push2Display::HEIGHT, Push2Display::HEIGHT / 5);
        label->attachToComponent(slider.get(), false);
        label->setJustificationType(Justification::centred);
        labels.push_back(std::move(label));
        sliders.push_back(std::move(slider));
        sliderAttachments.push_back(std::move(sliderAttachment));
    }
}
