#pragma once

#include "JuceHeader.h"
#include "processors/StatefulAudioProcessorWrapper.h"
#include "ParameterDisplayComponent.h"

class ParametersPanel : public Component {
public:
    explicit ParametersPanel(int maxRows) : maxRows(maxRows) {
        for (int paramIndex = 0; paramIndex < numColumns * maxRows; paramIndex++) {
            addChildComponent(paramComponents.add(new ParameterDisplayComponent()));
        }
    }

    ~ParametersPanel() override {
        clearParameters();
    }

    void setProcessor(StatefulAudioProcessorWrapper *processorWrapper) {
        if (this->processorWrapper == processorWrapper)
            return;

        parameters.clear(false);
        currentPage = 0;
        this->processorWrapper = processorWrapper;
        if (processorWrapper != nullptr) {
            for (int i = 0; i < processorWrapper->getNumParameters(); i++) {
                parameters.add(processorWrapper->getParameter(i));
            }
        }
        updateParameterComponents();
    }

    void addParameter(StatefulAudioProcessorWrapper::Parameter* parameter) {
        processorWrapper = nullptr; // manual changes mean this is no longer 1:1 with a single processor
        parameters.add(parameter);
        updateParameterComponents();
    }

    void clearParameters() {
        parameters.clear(false);
        updateParameterComponents();
    }

    StatefulAudioProcessorWrapper::Parameter* getParameterOnCurrentPageAt(int parameterIndex) const {
        int pageCorrectedParameterIndex = currentPage * maxRows * numColumns + parameterIndex;
        return parameters[pageCorrectedParameterIndex];
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
        return (currentPage + 1) * maxRows * numColumns < parameters.size();
    }

    void resized() override {
        auto r = getLocalBounds();
        auto componentWidth = r.getWidth() / numColumns;
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
    static const int numColumns {8};
    int maxRows;
    int currentPage{0};
    OwnedArray<ParameterDisplayComponent> paramComponents;
    OwnedArray<StatefulAudioProcessorWrapper::Parameter> parameters;
    StatefulAudioProcessorWrapper *processorWrapper{};

    void updateParameterComponents() {
        for (int paramIndex = 0; paramIndex < paramComponents.size(); paramIndex++) {
            auto *component = paramComponents.getUnchecked(paramIndex);
            if (auto *parameter = getParameterOnCurrentPageAt(paramIndex)) {
                component->setParameter(parameter);
                component->setVisible(true);
            } else {
                component->setVisible(false);
                component->setParameter(nullptr);
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametersPanel)
};