#pragma once

#include "JuceHeader.h"
#include "processors/StatefulAudioProcessorWrapper.h"

class ParameterDisplayComponent : public Component {
public:
    ParameterDisplayComponent() {
        addChildComponent(parameterName);
        addChildComponent(parameterLabel);
        addChildComponent(valueLabel);
    }

    ~ParameterDisplayComponent() override {
        detachParameterComponent();
    }

    void setParameter(StatefulAudioProcessorWrapper::Parameter *parameterWrapper) {
        if (this->parameterWrapper == parameterWrapper)
            return;
        for (auto *child : getChildren()) {
            child->setVisible(false);
        }

        detachParameterComponent();
        parameterComponent = nullptr;

        this->parameterWrapper = parameterWrapper;

        if (this->parameterWrapper == nullptr)
            return;

        auto *parameter = parameterWrapper->sourceParameter;

        parameterName.setText(parameter->getName(128), dontSendNotification);
        parameterName.setJustificationType(Justification::centred);
        parameterName.setVisible(true);

        parameterLabel.setText(parameter->getLabel(), dontSendNotification);
        parameterLabel.setVisible(!parameter->getLabel().isEmpty());
        parameterLabel.setJustificationType(Justification::centred);

        if (parameter->isBoolean()) {
            // The AU, AUv3 and VST (only via a .vstxml file) SDKs support
            // marking a parameter as boolean. If you want consistency across
            // all  formats then it might be best to use a
            // SwitchParameterComponent instead.
            auto *button = new TextButton();
            button->setClickingTogglesState(true);
            parameterWrapper->attachButton(button);
            parameterComponent.reset(button);
        } else if (!parameter->getAllValueStrings().isEmpty()) {
            if (parameter->getAllValueStrings().size() <= 3) {
                // show all options in a 1/2/3 toggle switch
                auto *parameterSwitch = new SwitchParameterComponent(parameter->getAllValueStrings());
                parameterWrapper->attachSwitch(parameterSwitch);
                parameterComponent.reset(parameterSwitch);
            } else {
                // too many for a reasonable switch. show dropdown instead
                auto *comboBox = new ComboBox();
                comboBox->addItemList(parameter->getAllValueStrings(), 1);
                parameterWrapper->attachComboBox(comboBox);
                parameterComponent.reset(comboBox);
            }
        } else if (parameter->getNumSteps() == 2 || parameter->getNumSteps() == 3) {
            auto labels = StringArray();
            for (int i = 0; i < parameter->getNumSteps(); i++) {
                float value = float(i) / float((parameter->getNumSteps() - 1));
                labels.add(parameter->getText(value, 16));
            }
            auto *parameterSwitch = new SwitchParameterComponent(labels);
            parameterWrapper->attachSwitch(parameterSwitch);
            parameterComponent.reset(parameterSwitch);
        } else {
            // Everything else can be represented as a slider.
            auto *slider = new Slider(Slider::RotaryHorizontalVerticalDrag, Slider::TextEntryBoxPosition::NoTextBox);
            parameterWrapper->attachSlider(slider);
            parameterWrapper->attachLabel(&valueLabel);
            parameterComponent.reset(slider);

            valueLabel.setColour(Label::outlineColourId,
                                 parameterComponent->findColour(Slider::textBoxOutlineColourId));
            valueLabel.setBorderSize({1, 1, 1, 1});
            valueLabel.setJustificationType(Justification::centred);
            valueLabel.setEditable(true, true);
            valueLabel.setVisible(true);
        }

        addAndMakeVisible(parameterComponent.get());
        resized();
    }

    void resized() override {
        if (parameterComponent == nullptr)
            return;
        auto area = getLocalBounds().reduced(5);
        parameterName.setBounds(area.removeFromTop(area.getHeight() / 5));
        auto bottom = area.removeFromBottom(area.getHeight() / 5);

        bool isSlider = dynamic_cast<Slider *>(parameterComponent.get());
        bool isButton = dynamic_cast<Button *>(parameterComponent.get());
        bool isCombobox = dynamic_cast<ComboBox *>(parameterComponent.get());
        bool isSwitch = dynamic_cast<SwitchParameterComponent *>(parameterComponent.get());

        if (parameterLabel.isVisible())
            parameterLabel.setBounds(isSlider ? bottom.removeFromRight(bottom.getWidth() / 4) : bottom);
        if (isSlider) {
            valueLabel.setBounds(bottom);
        } else if (isCombobox) {
            area.setHeight(area.getHeight() / 3);
        } else if (isSwitch) {
            area = area.withSizeKeepingCentre(area.getWidth(),
                                              area.getHeight() / 2); // TODO compute height by num items
        } else if (isButton) {
            auto smallerSquare = area.withWidth(area.getWidth() / 3).withHeight(area.getWidth() / 3);
            smallerSquare.setCentre(area.getCentre());
            area = smallerSquare;
        }
        parameterComponent->setBounds(area);
    }

private:
    Label parameterName, parameterLabel, valueLabel;
    std::unique_ptr<Component> parameterComponent{};
    StatefulAudioProcessorWrapper::Parameter *parameterWrapper{};

    void detachParameterComponent() {
        if (parameterComponent == nullptr || parameterWrapper == nullptr)
            return;
        if (auto *slider = dynamic_cast<Slider *>(parameterComponent.get())) {
            parameterWrapper->detachSlider(slider);
            parameterWrapper->detachLabel(&valueLabel);
        } else if (auto *button = dynamic_cast<Button *>(parameterComponent.get()))
            parameterWrapper->detachButton(button);
        else if (auto *comboBox = dynamic_cast<ComboBox *>(parameterComponent.get()))
            parameterWrapper->detachComboBox(comboBox);
        else if (auto *parameterSwitch = dynamic_cast<SwitchParameterComponent *>(parameterComponent.get()))
            parameterWrapper->detachSwitch(parameterSwitch);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterDisplayComponent)
};