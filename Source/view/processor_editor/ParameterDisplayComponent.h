#pragma once

#include "JuceHeader.h"
#include "processors/StatefulAudioProcessorWrapper.h"
#include "processors/MixerChannelProcessor.h"

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
        } else if (auto* levelMeterSource = parameterWrapper->getLevelMeterSource()) {
            auto* levelMeter = new LevelMeter();
            levelMeter->setMeterSource(levelMeterSource);
            parameterWrapper->attachLevelMeter(levelMeter);
            parameterWrapper->attachLabel(&valueLabel);
            parameterComponent.reset(levelMeter);
        } else {
            // Everything else can be represented as a slider.
            auto *slider = new Slider(Slider::RotaryHorizontalVerticalDrag, Slider::TextEntryBoxPosition::NoTextBox);
            parameterWrapper->attachSlider(slider);
            parameterWrapper->attachLabel(&valueLabel);
            parameterComponent.reset(slider);
        }

        if (getSlider() || getLevelMeter()) {
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
        auto area = getLocalBounds().reduced(2);
        parameterName.setBounds(area.removeFromTop(area.getHeight() / 5));
        auto bottom = area.removeFromBottom(area.getHeight() / 5);

        if (parameterLabel.isVisible())
            parameterLabel.setBounds(getSlider() || getLevelMeter() ? bottom.removeFromRight(bottom.getWidth() / 3) : bottom);
        if (getSlider() || getLevelMeter()) {
            valueLabel.setBounds(bottom);
        }
        if (getCombobox()) {
            area.setHeight(area.getHeight() / 3);
        } else if (getSwitch()) {
            area = area.withSizeKeepingCentre(area.getWidth(),
                                              area.getHeight() / 2); // TODO compute height by num items
        } else if (getButton()) {
            auto smallerSquare = area.withWidth(area.getWidth() / 3).withHeight(area.getWidth() / 3);
            smallerSquare.setCentre(area.getCentre());
            area = smallerSquare;
        } else if (getLevelMeter()) {
            area = area.withSizeKeepingCentre(jmin(40, area.getWidth()), area.getHeight());
        }
        parameterComponent->setBounds(area);
    }

    Slider* getSlider() { return dynamic_cast<Slider *>(parameterComponent.get()); }
    Button* getButton() { return dynamic_cast<Button *>(parameterComponent.get()); }
    ComboBox* getCombobox() { return dynamic_cast<ComboBox *>(parameterComponent.get()); }
    SwitchParameterComponent* getSwitch() { return dynamic_cast<SwitchParameterComponent *>(parameterComponent.get()); }
    LevelMeter* getLevelMeter() { return dynamic_cast<LevelMeter *>(parameterComponent.get()); }

private:
    Label parameterName, parameterLabel, valueLabel;
    std::unique_ptr<Component> parameterComponent{};
    StatefulAudioProcessorWrapper::Parameter *parameterWrapper{};

    void detachParameterComponent() {
        if (parameterComponent == nullptr || parameterWrapper == nullptr)
            return;
        if (auto* slider = getSlider()) {
            parameterWrapper->detachSlider(slider);
            parameterWrapper->detachLabel(&valueLabel);
        } else if (auto *button = getButton())
            parameterWrapper->detachButton(button);
        else if (auto *comboBox = getCombobox())
            parameterWrapper->detachComboBox(comboBox);
        else if (auto *parameterSwitch = getSwitch())
            parameterWrapper->detachSwitch(parameterSwitch);
        else if (auto *levelMeter = getLevelMeter())
            parameterWrapper->detachLevelMeter(levelMeter);
        parameterComponent = nullptr;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterDisplayComponent)
};