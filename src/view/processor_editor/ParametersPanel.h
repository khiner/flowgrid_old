#pragma once

#include "processors/StatefulAudioProcessorWrapper.h"
#include "ParameterDisplayComponent.h"

class ParametersPanel : public Component {
public:
    explicit ParametersPanel(int maxRows, int numColumns = 8);

    ~ParametersPanel() override;

    void setProcessorWrapper(StatefulAudioProcessorWrapper *processorWrapper);

    ValueTree getProcessorState() const { return processorWrapper != nullptr ? processorWrapper->state : ValueTree(); }

    void addParameter(StatefulAudioProcessorWrapper::Parameter *parameter);

    void clearParameters();

    StatefulAudioProcessorWrapper::Parameter *getParameterOnCurrentPageAt(int parameterIndex) const {
        int pageCorrectedParameterIndex = currentPage * maxRows * numColumns + parameterIndex;
        return parameters[pageCorrectedParameterIndex];
    }

    bool canPageLeft() const { return currentPage > 0; }
    bool canPageRight() const { return (currentPage + 1) * maxRows * numColumns < parameters.size(); }
    void pageLeft();
    void pageRight();

    // todo switch to adding new LookAndFeel colour IDs
    void setBackgroundColour(const Colour &backgroundColour) {
        this->backgroundColour = backgroundColour;
        repaint();
    }
    void setOutlineColour(const Colour &outlineColour) {
        this->outlineColour = outlineColour;
        repaint();
    }

    void paint(Graphics &g) override;
    void resized() override;

    int getParameterWidth() { return getLocalBounds().getWidth() / numColumns; }
    int getParameterHeight() { return maxRows == 1 ? getLocalBounds().getHeight() : getParameterWidth() * 7 / 5; }

private:
    int maxRows;
    int numColumns;
    int currentPage{0};
    OwnedArray<ParameterDisplayComponent> paramComponents;
    OwnedArray<StatefulAudioProcessorWrapper::Parameter> parameters;
    StatefulAudioProcessorWrapper *processorWrapper{};

    Colour backgroundColour = Colours::transparentBlack;
    Colour outlineColour = Colours::transparentBlack;

    void updateParameterComponents();
};
