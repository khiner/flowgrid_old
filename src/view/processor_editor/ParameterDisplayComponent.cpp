#include "ParameterDisplayComponent.h"

#include "view/parameter_control/level_meter/MinimalLevelMeter.h"

ParameterDisplayComponent::ParameterDisplayComponent() {
    addChildComponent(parameterName);
    addChildComponent(valueLabel);
}

ParameterDisplayComponent::~ParameterDisplayComponent() {
    detachParameterComponent();
}

void ParameterDisplayComponent::setParameter(StatefulAudioProcessorWrapper::Parameter *parameterWrapper) {
    if (this->parameterWrapper == parameterWrapper && parameterComponent) return;

    detachParameterComponent();

    this->parameterWrapper = parameterWrapper;

    if (this->parameterWrapper == nullptr)
        return;

    this->parameterWrapper->addListener(this);

    auto *parameter = parameterWrapper->sourceParameter;

    parameterName.setText(parameter->getName(128), dontSendNotification);
    parameterName.setJustificationType(Justification::centred);
    addAndMakeVisible(parameterName);

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
    } else if (auto *levelMeterSource = parameterWrapper->getLevelMeterSource()) {
        auto *levelMeter = new MinimalLevelMeter(LevelMeter::vertical);
        levelMeter->setMeterSource(levelMeterSource);
        parameterWrapper->attachParameterControl(levelMeter);
        parameterWrapper->attachLabel(&valueLabel);
        parameterComponent.reset(levelMeter);
    } else {
        // Everything else can be represented as a slider.
        auto *slider = new Slider(Slider::RotaryHorizontalVerticalDrag, Slider::TextEntryBoxPosition::NoTextBox);
        parameterWrapper->attachSlider(slider);
        parameterWrapper->attachLabel(&valueLabel);

        if (parameterWrapper->range.getRange().getStart() == -1 && parameterWrapper->range.getRange().getEnd() == 1)
            slider->getProperties().set("fromCentre", true);

        parameterComponent.reset(slider);
    }

    if (getSlider() || getLevelMeter()) {
        valueLabel.setJustificationType(Justification::centred);
        valueLabel.setEditable(true, true);
        addAndMakeVisible(valueLabel);
    }

    addAndMakeVisible(parameterComponent.get());
    resized();
}

void ParameterDisplayComponent::resized() {
    if (parameterComponent == nullptr) return;

    auto area = getLocalBounds().reduced(2);
    parameterName.setBounds(area.removeFromTop(16));

    auto r = getLocalBounds();
    if (r.getHeight() < 80 || (r.getWidth() < 64 && getSlider() && !getDraggableValueLabel())) {
        if (getDraggableValueLabel() == nullptr) {
            detachParameterComponent();
            removeChildComponent(parameterComponent.get());
            removeChildComponent(&valueLabel);
            auto *draggableValueLabel = new DraggableValueLabel();
            parameterWrapper->attachSlider(draggableValueLabel);
            parameterComponent.reset(draggableValueLabel);
            addAndMakeVisible(parameterComponent.get());
        }
        parameterComponent->setBounds(area);
        return;
    }
    if (getDraggableValueLabel()) {
        detachParameterComponent();
        setParameter(parameterWrapper);
        return;
    }

    auto bottom = area.removeFromBottom(20);

    if (getSlider() || getLevelMeter()) {
        valueLabel.setBounds(bottom);
    }
    if (getCombobox()) {
        area.setHeight(area.getHeight() / 3);
    } else if (getSwitch()) {
        // TODO compute height by num items
        area = area.withSizeKeepingCentre(area.getWidth() - 4, area.getHeight() / 2);
    } else if (getButton()) {
        auto smallerSquare = area.withWidth(area.getWidth() / 3).withHeight(area.getWidth() / 3);
        smallerSquare.setCentre(area.getCentre());
        area = smallerSquare;
    } else if (getLevelMeter()) {
        area = area.withSizeKeepingCentre(jmin(40, area.getWidth()), area.getHeight());
    }
    parameterComponent->setBounds(area);
}

void ParameterDisplayComponent::detachParameterComponent() {
    if (parameterComponent == nullptr || parameterWrapper == nullptr) return;

    parameterWrapper->removeListener(this);
    if (auto *slider = getSlider()) {
        parameterWrapper->detachSlider(slider);
        parameterWrapper->detachLabel(&valueLabel);
    } else if (auto *button = getButton())
        parameterWrapper->detachButton(button);
    else if (auto *comboBox = getCombobox())
        parameterWrapper->detachComboBox(comboBox);
    else if (auto *parameterSwitch = getSwitch())
        parameterWrapper->detachSwitch(parameterSwitch);
    else if (auto *levelMeter = getLevelMeter()) {
        parameterWrapper->detachParameterControl(levelMeter);
        parameterWrapper->detachLabel(&valueLabel);
    }
    parameterComponent = nullptr;
}
