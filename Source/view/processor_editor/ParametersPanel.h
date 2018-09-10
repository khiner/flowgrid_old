#pragma once

#include "JuceHeader.h"
#include "processors/StatefulAudioProcessorWrapper.h"
#include "ParameterDisplayComponent.h"

class ParametersPanel : public Component {
public:
    explicit ParametersPanel(int maxRows, int numColumns=8) : maxRows(maxRows), numColumns(numColumns) {
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
            for (int i = 0; i < processorWrapper->getNumAutomatableParameters(); i++) {
                parameters.add(processorWrapper->getAutomatableParameter(i));
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

    // todo switch to adding new LookAndFeel colour IDs
    void setBackgroundColour(const Colour &backgroundColour) {
        this->backgroundColour = backgroundColour;
    }

    void setOutlineColour(const Colour &outlineColour) {
        this->outlineColour = outlineColour;
    }

    void paint(Graphics& g) override {
        auto r = getLocalBounds();
        g.setColour(backgroundColour);
        g.fillRect(r);
        g.setColour(outlineColour);
        g.drawRect(r, 2);
    }

    void resized() override {
        auto r = getLocalBounds();
        auto parameterWidth = getParameterWidth();
        auto parameterHeight = getParameterHeight();
        auto currentRowArea = r.removeFromTop(parameterHeight);

        int column = 1, row = 1;
        for (auto *comp : paramComponents) {
            if (column++ % (numColumns + 1) == 0) {
                column = 1;
                if (row++ >= maxRows)
                    break;
                currentRowArea = r.removeFromTop(parameterHeight);
            }
            comp->setBounds(currentRowArea.removeFromLeft(parameterWidth));
        }
        repaint();
    }

    int getParameterWidth() {
        return getLocalBounds().getWidth() / numColumns;
    }

    int getParameterHeight() {
        return maxRows == 1 ? getLocalBounds().getHeight() : getParameterWidth() * 7 / 5;
    }

private:
    int maxRows;
    int numColumns;
    int currentPage{0};
    OwnedArray<ParameterDisplayComponent> paramComponents;
    OwnedArray<StatefulAudioProcessorWrapper::Parameter> parameters;
    StatefulAudioProcessorWrapper *processorWrapper{};

    Colour backgroundColour = Colours::transparentBlack;
    Colour outlineColour = Colours::transparentBlack;

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