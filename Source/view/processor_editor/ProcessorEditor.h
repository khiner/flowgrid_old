#include <memory>

#pragma once

#include "JuceHeader.h"

class ProcessorEditor : public Component {
public:
    explicit ProcessorEditor(int maxRows=2) :
            pageLeftButton("Page left", 0.5, findColour(ResizableWindow::backgroundColourId).brighter(0.75)),
            pageRightButton("Page right", 0.0, findColour(ResizableWindow::backgroundColourId).brighter(0.75)) {
        setOpaque(true);
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(pageLeftButton);
        addAndMakeVisible(pageRightButton);

        addAndMakeVisible((parametersPanel = std::make_unique<ParametersPanel>(maxRows)).get());

        pageLeftButton.onClick = [this]() {
            parametersPanel->pageLeft();
            updatePageButtonVisibility();
        };

        pageRightButton.onClick = [this]() {
            parametersPanel->pageRight();
            updatePageButtonVisibility();
        };
    }

    ~ProcessorEditor() override = default;

    void paint(Graphics &g) override {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    }

    void resized() override {
        auto r = getLocalBounds();
        auto top = r.removeFromTop(30);
        pageRightButton.setBounds(top.removeFromRight(30).reduced(5));
        pageLeftButton.setBounds(top.removeFromRight(30).reduced(5));
        titleLabel.setBounds(top);

        parametersPanel->setBounds(r);
    }

    void setProcessor(AudioProcessor *const processor) {
        if (processor != nullptr) {
            titleLabel.setText(processor->getName(), dontSendNotification);
        }
        parametersPanel->setProcessor(processor);
        updatePageButtonVisibility();
    }

    void updatePageButtonVisibility() {
        pageLeftButton.setVisible(parametersPanel->canPageLeft());
        pageRightButton.setVisible(parametersPanel->canPageRight());
    }

private:
    class ParameterListener : private AudioProcessorParameter::Listener,
                              private Timer {
    public:
        explicit ParameterListener(AudioProcessorParameter &param)
                : parameter(param) {
            parameter.addListener(this);
            startTimer(100);
        }

        ~ParameterListener() override {
            parameter.removeListener(this);
        }

        AudioProcessorParameter &getParameter() noexcept {
            return parameter;
        }

        virtual void handleNewParameterValue() = 0;

    private:
        void parameterValueChanged(int, float) override {
            parameterValueHasChanged = 1;
        }

        void parameterGestureChanged(int, bool) override {}

        void timerCallback() override {
            if (parameterValueHasChanged.compareAndSetBool(0, 1)) {
                handleNewParameterValue();
                startTimerHz(50);
            } else {
                startTimer(jmin(250, getTimerInterval() + 10));
            }
        }

        AudioProcessorParameter &parameter;
        Atomic<int> parameterValueHasChanged{0};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterListener)
    };

    class BooleanParameterComponent final : public Component,
                                            private ParameterListener {
    public:
        BooleanParameterComponent(AudioProcessorParameter &param) : ParameterListener(param) {
            // Set the initial value.
            handleNewParameterValue();

            button.onClick = [this]() { buttonClicked(); };

            addAndMakeVisible(button);
        }

        void paint(Graphics &) override {}

        void resized() override {
            auto area = getLocalBounds();
            area.removeFromLeft(8);
            button.setBounds(area.reduced(0, 10));
        }

    private:
        void handleNewParameterValue() override {
            auto parameterState = getParameterState(getParameter().getValue());

            if (button.getToggleState() != parameterState)
                button.setToggleState(parameterState, dontSendNotification);
        }

        void buttonClicked() {
            if (getParameterState(getParameter().getValue()) != button.getToggleState()) {
                getParameter().beginChangeGesture();
                getParameter().setValueNotifyingHost(button.getToggleState() ? 1.0f : 0.0f);
                getParameter().endChangeGesture();
            }
        }

        bool getParameterState(float value) const noexcept {
            return value >= 0.5f;
        }

        ToggleButton button;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BooleanParameterComponent)
    };

    class SwitchParameterComponent final : public Component,
                                           private ParameterListener {
    public:
        SwitchParameterComponent(AudioProcessorParameter &param) : ParameterListener(param) {
            auto *leftButton = buttons.add(new TextButton());
            auto *rightButton = buttons.add(new TextButton());

            for (auto *button : buttons) {
                button->setRadioGroupId(293847);
                button->setClickingTogglesState(true);
            }

            leftButton->setButtonText(getParameter().getText(0.0f, 16));
            rightButton->setButtonText(getParameter().getText(1.0f, 16));

            leftButton->setConnectedEdges(Button::ConnectedOnRight);
            rightButton->setConnectedEdges(Button::ConnectedOnLeft);

            // Set the initial value.
            leftButton->setToggleState(true, dontSendNotification);
            handleNewParameterValue();

            rightButton->onStateChange = [this]() { rightButtonChanged(); };

            for (auto *button : buttons)
                addAndMakeVisible(button);
        }

        void paint(Graphics &) override {}

        void resized() override {
            auto area = getLocalBounds().reduced(0, 8);
            area.removeFromLeft(8);

            for (auto *button : buttons)
                button->setBounds(area.removeFromLeft(80));
        }

    private:
        void handleNewParameterValue() override {
            bool newState = getParameterState();

            if (buttons[1]->getToggleState() != newState) {
                buttons[1]->setToggleState(newState, dontSendNotification);
                buttons[0]->setToggleState(!newState, dontSendNotification);
            }
        }

        void rightButtonChanged() {
            auto buttonState = buttons[1]->getToggleState();

            if (getParameterState() != buttonState) {
                getParameter().beginChangeGesture();

                if (getParameter().getAllValueStrings().isEmpty()) {
                    getParameter().setValueNotifyingHost(buttonState ? 1.0f : 0.0f);
                } else {
                    // When a parameter provides a list of strings we must set its
                    // value using those strings, rather than a float, because VSTs can
                    // have uneven spacing between the different allowed values and we
                    // want the snapping behaviour to be consistent with what we do with
                    // a combo box.
                    String selectedText = buttonState ? buttons[1]->getButtonText() : buttons[0]->getButtonText();
                    getParameter().setValueNotifyingHost(getParameter().getValueForText(selectedText));
                }

                getParameter().endChangeGesture();
            }
        }

        bool getParameterState() {
            if (getParameter().getAllValueStrings().isEmpty())
                return getParameter().getValue() > 0.5f;

            auto index = getParameter().getAllValueStrings()
                    .indexOf(getParameter().getCurrentValueAsText());

            if (index < 0) {
                // The parameter is producing some unexpected text, so we'll do
                // some linear interpolation.
                index = roundToInt(getParameter().getValue());
            }

            return index == 1;
        }

        OwnedArray<TextButton> buttons;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwitchParameterComponent)
    };

    class ChoiceParameterComponent final : public Component,
                                           private ParameterListener {
    public:
        explicit ChoiceParameterComponent(AudioProcessorParameter &param)
                : ParameterListener(param),
                  parameterValues(getParameter().getAllValueStrings()) {
            box.addItemList(parameterValues, 1);

            // Set the initial value.
            handleNewParameterValue();

            box.onChange = [this]() { boxChanged(); };
            addAndMakeVisible(box);
        }

        void paint(Graphics &) override {}

        void resized() override {
            auto area = getLocalBounds();
            area.removeFromLeft(8);
            box.setBounds(area.reduced(0, 10));
        }

    private:
        void handleNewParameterValue() override {
            auto index = parameterValues.indexOf(getParameter().getCurrentValueAsText());

            if (index < 0) {
                // The parameter is producing some unexpected text, so we'll do
                // some linear interpolation.
                index = roundToInt(getParameter().getValue() * (parameterValues.size() - 1));
            }

            box.setSelectedItemIndex(index);
        }

        void boxChanged() {
            if (getParameter().getCurrentValueAsText() != box.getText()) {
                getParameter().beginChangeGesture();

                // When a parameter provides a list of strings we must set its
                // value using those strings, rather than a float, because VSTs can
                // have uneven spacing between the different allowed values.
                getParameter().setValueNotifyingHost(getParameter().getValueForText(box.getText()));

                getParameter().endChangeGesture();
            }
        }

        ComboBox box;
        const StringArray parameterValues;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceParameterComponent)
    };

    class SliderParameterComponent final : public Component,
                                           private ParameterListener {
    public:
        SliderParameterComponent(AudioProcessorParameter &param, Label& valueLabel)
                : ParameterListener(param), valueLabel(valueLabel) {
            if (getParameter().getNumSteps() != AudioProcessor::getDefaultNumParameterSteps())
                slider.setRange(0.0, 1.0, 1.0 / (getParameter().getNumSteps() - 1.0));
            else
                slider.setRange(0.0, 1.0);

            slider.setScrollWheelEnabled(false);
            addAndMakeVisible(slider);

            // Set the initial value.
            handleNewParameterValue();

            slider.onValueChange = [this]() { sliderValueChanged(); };
            slider.onDragStart = [this]() { sliderStartedDragging(); };
            slider.onDragEnd = [this]() { sliderStoppedDragging(); };
        }

        void paint(Graphics &) override {}

        void resized() override {
            auto area = getLocalBounds();
            slider.setBounds(area.reduced(5));
        }

    private:
        Label &valueLabel;

        void updateTextDisplay() {
            valueLabel.setText(getParameter().getCurrentValueAsText(), dontSendNotification);
        }

        void handleNewParameterValue() override {
            if (!isDragging) {
                slider.setValue(getParameter().getValue(), dontSendNotification);
                updateTextDisplay();
            }
        }

        void sliderValueChanged() {
            auto newVal = (float) slider.getValue();

            if (getParameter().getValue() != newVal) {
                if (!isDragging)
                    getParameter().beginChangeGesture();

                getParameter().setValueNotifyingHost((float) slider.getValue());
                updateTextDisplay();

                if (!isDragging)
                    getParameter().endChangeGesture();
            }
        }

        void sliderStartedDragging() {
            isDragging = true;
            getParameter().beginChangeGesture();
        }

        void sliderStoppedDragging() {
            isDragging = false;
            getParameter().endChangeGesture();
        }

        Slider slider{Slider::RotaryHorizontalVerticalDrag, Slider::TextEntryBoxPosition::NoTextBox};
        bool isDragging = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliderParameterComponent)
    };

    class ParameterDisplayComponent : public Component {
    public:
        ParameterDisplayComponent() {
            addChildComponent(parameterName);
            addChildComponent(parameterLabel);
            addChildComponent(valueLabel);
        }

        void setParameter(AudioProcessorParameter *param) {
            for (auto* child : getChildren()) {
                child->setVisible(false);
            }
            
            if (param == nullptr) {
                parameterComponent = nullptr;
                return;
            }

            parameterName.setText(param->getName(128), dontSendNotification);
            parameterName.setJustificationType(Justification::centred);
            parameterName.setVisible(true);

            parameterLabel.setText(param->getLabel(), dontSendNotification);
            parameterLabel.setVisible(true);

            if (param->isBoolean()) {
                // The AU, AUv3 and VST (only via a .vstxml file) SDKs support
                // marking a parameter as boolean. If you want consistency across
                // all  formats then it might be best to use a
                // SwitchParameterComponent instead.
                parameterComponent = std::make_unique<BooleanParameterComponent>(*param);
            } else if (param->getNumSteps() == 2) {
                // Most hosts display any parameter with just two steps as a switch.
                parameterComponent = std::make_unique<SwitchParameterComponent>(*param);
            } else if (!param->getAllValueStrings().isEmpty()) {
                // If we have a list of strings to represent the different states a
                // parameter can be in then we should present a dropdown allowing a
                // user to pick one of them.
                parameterComponent = std::make_unique<ChoiceParameterComponent>(*param);
            } else {
                // Everything else can be represented as a slider.
                parameterComponent = std::make_unique<SliderParameterComponent>(*param, valueLabel);

                valueLabel.setColour(Label::outlineColourId, parameterComponent->findColour(Slider::textBoxOutlineColourId));
                valueLabel.setBorderSize({1, 1, 1, 1});
                valueLabel.setJustificationType(Justification::centred);
                valueLabel.setEditable(true, true);
                valueLabel.setVisible(true);
            }

            addAndMakeVisible(parameterComponent.get());
            resized();
        }

        void resized() override {
            auto area = getLocalBounds().reduced(5);
            parameterName.setBounds(area.removeFromTop(area.getHeight() / 5));
            auto bottom = area.removeFromBottom(area.getHeight() / 5);
            parameterLabel.setBounds(bottom.removeFromRight(area.getWidth() / 4));
            valueLabel.setBounds(bottom);
            if (parameterComponent != nullptr)
                parameterComponent->setBounds(area);
        }

    private:
        Label parameterName, parameterLabel, valueLabel;
        std::unique_ptr<Component> parameterComponent;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterDisplayComponent)
    };

    class ParametersPanel : public Component {
    public:
        explicit ParametersPanel(int maxRows) : maxRows(maxRows) {
            for (int paramIndex = 0; paramIndex < 8 * maxRows; paramIndex++) {
                addChildComponent(paramComponents.add(new ParameterDisplayComponent()));
            }
        }

        void setProcessor(AudioProcessor *processor) {
            if (this->processor != processor) {
                currentPage = 0;
            }
            this->processor = processor;
            updateParameterComponents();
        }

        void updateParameterComponents() const {
            int componentIndex = 0;
            if (processor != nullptr) {
                for (int paramIndex = currentPage * maxRows * 8; paramIndex < processor->getParameters().size(); paramIndex++) {
                    auto *parameter = processor->getParameters().getUnchecked(paramIndex);
                    if (parameter->isAutomatable()) {
                        auto *component = paramComponents.getUnchecked(componentIndex++);
                        component->setParameter(parameter);
                        component->setVisible(true);
                        if (componentIndex >= paramComponents.size())
                            break;
                    }
                }
            }
            for (; componentIndex < paramComponents.size(); componentIndex++) {
                auto *component = paramComponents.getUnchecked(componentIndex);
                component->setVisible(false);
                component->setParameter(nullptr);
            }
        }

        void pageLeft() {
            if (canPageLeft()) {
                currentPage--;
                updateParameterComponents();
            }
        }

        void pageRight() {
            if (canPageRight()) {
                currentPage++;
                updateParameterComponents();
            }
        }

        bool canPageLeft() {
            return currentPage > 0;
        }

        bool canPageRight() {
            return processor != nullptr && (currentPage + 1) * maxRows * 8 < processor->getParameters().size();
        }

        void paint(Graphics &g) override {
            g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
        }

        void resized() override {
            auto r = getLocalBounds();
            auto componentWidth = r.getWidth() / 8;
            auto componentHeight = componentWidth * 7 / 5;
            Rectangle<int> currentRowArea = r.removeFromTop(componentHeight);

            int column = 1, row = 1;
            for (auto *comp : paramComponents) {
                if (column++ % 9 == 0) {
                    column = 1;
                    if (row++ >= 2)
                        break;
                    currentRowArea = r.removeFromTop(componentHeight);
                }
                comp->setBounds(currentRowArea.removeFromLeft(componentWidth));
            }
        }

    private:
        int maxRows;
        int currentPage {0};
        OwnedArray<ParameterDisplayComponent> paramComponents;
        AudioProcessor *processor;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametersPanel)
    };

    std::unique_ptr<ParametersPanel> parametersPanel;
    Label titleLabel;
    ArrowButton pageLeftButton, pageRightButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorEditor)
};
