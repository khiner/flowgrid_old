#include <memory>

#pragma once

#include "JuceHeader.h"
#include "processors/StatefulAudioProcessorWrapper.h"

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

    void setProcessor(StatefulAudioProcessorWrapper *const processorWrapper) {
        if (processorWrapper != nullptr) {
            titleLabel.setText(processorWrapper->processor->getName(), dontSendNotification);
        }
        parametersPanel->setProcessor(processorWrapper);
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
            for (auto* child : getChildren()) {
                child->setVisible(false);
            }

            detachParameterComponent();

            parameterComponent = nullptr;

            this->parameterWrapper = parameterWrapper;

            if (this->parameterWrapper == nullptr)
                return;

            auto* parameter = parameterWrapper->sourceParameter;

            parameterName.setText(parameter->getName(128), dontSendNotification);
            parameterName.setJustificationType(Justification::centred);
            parameterName.setVisible(true);

            parameterLabel.setText(parameter->getLabel(), dontSendNotification);
            parameterLabel.setVisible(true);

            if (parameter->isBoolean()) {
                // The AU, AUv3 and VST (only via a .vstxml file) SDKs support
                // marking a parameter as boolean. If you want consistency across
                // all  formats then it might be best to use a
                // SwitchParameterComponent instead.
                parameterComponent = std::make_unique<BooleanParameterComponent>(*parameter);
            } else if (parameter->getNumSteps() == 2) {
                // Most hosts display any parameter with just two steps as a switch.
                parameterComponent = std::make_unique<SwitchParameterComponent>(*parameter);
            } else if (!parameter->getAllValueStrings().isEmpty()) {
                // If we have a list of strings to represent the different states a
                // parameter can be in then we should present a dropdown allowing a
                // user to pick one of them.
                auto* comboBox = new ComboBox();
                comboBox->addItemList(parameter->getAllValueStrings(), 1);
                parameterWrapper->attachComboBox(comboBox, &valueLabel);
                parameterComponent.reset(comboBox);
            } else {
                // Everything else can be represented as a slider.
                auto* slider = new Slider(Slider::RotaryHorizontalVerticalDrag, Slider::TextEntryBoxPosition::NoTextBox);
                parameterWrapper->attachSlider(slider, &valueLabel);
                parameterComponent.reset(slider);

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
        StatefulAudioProcessorWrapper::Parameter *parameterWrapper {};

        void detachParameterComponent() {
            if (auto *slider = dynamic_cast<Slider *>(parameterComponent.get()))
                parameterWrapper->detachSlider(slider, &valueLabel);
            else if (auto *comboBox = dynamic_cast<ComboBox *>(parameterComponent.get()))
                parameterWrapper->detachComboBox(comboBox, &valueLabel);
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterDisplayComponent)
    };

    class ParametersPanel : public Component {
    public:
        explicit ParametersPanel(int maxRows) : maxRows(maxRows) {
            for (int paramIndex = 0; paramIndex < 8 * maxRows; paramIndex++) {
                addChildComponent(paramComponents.add(new ParameterDisplayComponent()));
            }
        }

        void setProcessor(StatefulAudioProcessorWrapper *processor) {
            if (this->processorWrapper != processor) {
                currentPage = 0;
            }
            this->processorWrapper = processor;
            updateParameterComponents();
        }

        void updateParameterComponents() const {
            int componentIndex = 0;
            if (processorWrapper != nullptr) {
                for (int paramIndex = currentPage * maxRows * 8; paramIndex < processorWrapper->processor->getParameters().size(); paramIndex++) {
                    auto *parameter = processorWrapper->getParameter(paramIndex);
                    if (parameter->sourceParameter->isAutomatable()) {
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
            return processorWrapper != nullptr && (currentPage + 1) * maxRows * 8 < processorWrapper->processor->getParameters().size();
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
        StatefulAudioProcessorWrapper *processorWrapper {};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametersPanel)
    };

    std::unique_ptr<ParametersPanel> parametersPanel;
    Label titleLabel;
    ArrowButton pageLeftButton, pageRightButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorEditor)
};
