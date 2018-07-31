#include <memory>

#pragma once

#include "JuceHeader.h"
#include "processors/StatefulAudioProcessorWrapper.h"
#include "SwitchParameterComponent.h"

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
            parameterLabel.setVisible(!parameter->getLabel().isEmpty());
            parameterLabel.setJustificationType(Justification::centred);

            if (parameter->isBoolean()) {
                // The AU, AUv3 and VST (only via a .vstxml file) SDKs support
                // marking a parameter as boolean. If you want consistency across
                // all  formats then it might be best to use a
                // SwitchParameterComponent instead.
                auto* button = new TextButton();
                button->setClickingTogglesState(true);
                parameterWrapper->attachButton(button);
                parameterComponent.reset(button);
            } else if (parameter->getNumSteps() == 2) {
                // Most hosts display any parameter with just two steps as a switch.
                auto* parameterSwitch = new SwitchParameterComponent(parameter->getText(0.0f, 16), parameter->getText(1.0f, 16));
                parameterWrapper->attachSwitch(parameterSwitch);
                parameterComponent.reset(parameterSwitch);
            } else if (!parameter->getAllValueStrings().isEmpty()) {
                // If we have a list of strings to represent the different states a
                // parameter can be in then we should present a dropdown allowing a
                // user to pick one of them.
                auto* comboBox = new ComboBox();
                comboBox->addItemList(parameter->getAllValueStrings(), 1);
                parameterWrapper->attachComboBox(comboBox);
                parameterComponent.reset(comboBox);
            } else {
                // Everything else can be represented as a slider.
                auto* slider = new Slider(Slider::RotaryHorizontalVerticalDrag, Slider::TextEntryBoxPosition::NoTextBox);
                parameterWrapper->attachSlider(slider);
                parameterWrapper->attachLabel(&valueLabel);
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
                area = area.withSizeKeepingCentre(area.getWidth(), area.getHeight() / 2); // TODO compute height by num items
            } else if (isButton) {
                auto smallerSquare = area.withWidth(area.getWidth() / 3).withHeight(area.getWidth() / 3);
                smallerSquare.setCentre(area.getCentre());
                area = smallerSquare;
            }
            parameterComponent->setBounds(area);
        }

    private:
        Label parameterName, parameterLabel, valueLabel;
        std::unique_ptr<Component> parameterComponent {};
        StatefulAudioProcessorWrapper::Parameter *parameterWrapper {};

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
