#pragma once

#include "JuceHeader.h"
#include "processors/StatefulAudioProcessorWrapper.h"
#include "ParameterDisplayComponent.h"

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
            for (int paramIndex = currentPage * maxRows * 8;
                 paramIndex < processorWrapper->processor->getParameters().size(); paramIndex++) {
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
        return processorWrapper != nullptr &&
               (currentPage + 1) * maxRows * 8 < processorWrapper->processor->getParameters().size();
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
    int currentPage{0};
    OwnedArray<ParameterDisplayComponent> paramComponents;
    StatefulAudioProcessorWrapper *processorWrapper{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametersPanel)
};