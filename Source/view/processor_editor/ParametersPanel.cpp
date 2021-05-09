#include "ParametersPanel.h"

ParametersPanel::ParametersPanel(int maxRows, int numColumns) : maxRows(maxRows), numColumns(numColumns) {
    for (int paramIndex = 0; paramIndex < numColumns * maxRows; paramIndex++) {
        addChildComponent(paramComponents.add(new ParameterDisplayComponent()));
    }
    // This allows the parent to capture click events when clicking on non-interactive elements like param labels.
    // For example, this allows dragging a processor with inline param UI in the graph
    setInterceptsMouseClicks(false, true);
}

ParametersPanel::~ParametersPanel() {
    clearParameters();
}

void ParametersPanel::setProcessorWrapper(StatefulAudioProcessorWrapper *processorWrapper) {
    if (this->processorWrapper == processorWrapper) return;

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

void ParametersPanel::addParameter(StatefulAudioProcessorWrapper::Parameter *parameter) {
    processorWrapper = nullptr; // manual changes mean this is no longer 1:1 with a single processor
    parameters.add(parameter);
    updateParameterComponents();
}

void ParametersPanel::clearParameters() {
    parameters.clear(false);
    updateParameterComponents();
}

void ParametersPanel::pageLeft() {
    if (canPageLeft()) {
        currentPage--;
        updateParameterComponents();
    }
}

void ParametersPanel::pageRight() {
    if (canPageRight()) {
        currentPage++;
        updateParameterComponents();
    }
}

void ParametersPanel::paint(Graphics &g) {
    auto r = getLocalBounds();
    g.setColour(backgroundColour);
    g.fillRect(r);
    g.setColour(outlineColour);
    g.drawRect(r, 2);
}

void ParametersPanel::resized() {
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

void ParametersPanel::updateParameterComponents() {
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
