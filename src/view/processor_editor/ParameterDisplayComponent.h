#pragma once

#include "view/parameter_control/level_meter/LevelMeter.h"
#include "processors/StatefulAudioProcessorWrapper.h"
#include "DraggableValueLabel.h"

class ParameterDisplayComponent : public Component, public StatefulAudioProcessorWrapper::Parameter::Listener {
public:
    ParameterDisplayComponent();

    ~ParameterDisplayComponent() override;

    // This allows the parent to capture click events when clicking on non-interactive elements like param labels.
    // For example, this allows dragging a processor with inline param UI in the graph
    bool hitTest(int x, int y) override {
        return parameterComponent != nullptr && parameterComponent->getBounds().contains(x, y);
    }

    void setParameter(StatefulAudioProcessorWrapper::Parameter *parameterWrapper);

    void resized() override;

    void parameterWillBeDestroyed(StatefulAudioProcessorWrapper::Parameter *parameter) override {
        if (parameter == this->parameterWrapper)
            detachParameterComponent();
    }

    Slider *getSlider() const { return dynamic_cast<Slider *>(parameterComponent.get()); }
    DraggableValueLabel *getDraggableValueLabel() const { return dynamic_cast<DraggableValueLabel *>(parameterComponent.get()); }
    Button *getButton() const { return dynamic_cast<Button *>(parameterComponent.get()); }
    ComboBox *getCombobox() const { return dynamic_cast<ComboBox *>(parameterComponent.get()); }
    SwitchParameterComponent *getSwitch() const { return dynamic_cast<SwitchParameterComponent *>(parameterComponent.get()); }
    LevelMeter *getLevelMeter() const { return dynamic_cast<LevelMeter *>(parameterComponent.get()); }

private:
    Label parameterName, valueLabel;
    std::unique_ptr<Component> parameterComponent{};
    StatefulAudioProcessorWrapper::Parameter *parameterWrapper{};

    void detachParameterComponent();
};
