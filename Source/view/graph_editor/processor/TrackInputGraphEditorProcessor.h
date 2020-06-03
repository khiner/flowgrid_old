#pragma once

#include <view/parameter_control/slider/MinimalSliderControl.h>
#include "view/parameter_control/level_meter/MinimalLevelMeter.h"
#include "BaseGraphEditorProcessor.h"

class TrackInputGraphEditorProcessor : public BaseGraphEditorProcessor {
public:
    TrackInputGraphEditorProcessor(Project &project, TracksState &tracks, ViewState &view,
                                    const ValueTree &state, ConnectorDragListener &connectorDragListener) :
            BaseGraphEditorProcessor(project, tracks, view, state, connectorDragListener, false) {
    }

    ~TrackInputGraphEditorProcessor() {
        if (auto *processorWrapper = getProcessorWrapper()) {
            if (monitoringToggle != nullptr) {
                const auto &parameterWrapper = processorWrapper->getParameter(0);
                parameterWrapper->detachButton(monitoringToggle.get());
            }
        }
    }

    void resized() override {
        BaseGraphEditorProcessor::resized();
        const auto &localBounds = getLocalBounds();;
        auto remainingBounds = localBounds.reduced(5);

        if (monitoringToggle != nullptr) {
            monitoringToggle->setBounds(remainingBounds);
        }
    }

    void paint(Graphics &g) override {
        auto r = getLocalBounds();
        const auto &backgroundColour = findColour(ResizableWindow::backgroundColourId);
        g.setColour(backgroundColour);
        g.fillRect(r);
    }

    bool isInView() override {
        return true;
    }

private:
    std::unique_ptr<TextButton> monitoringToggle;

    Rectangle<int> getBoxBounds() override {
        return getLocalBounds();
    }

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v != state)
            return;

        if (monitoringToggle == nullptr) {
            if (auto *processorWrapper = getProcessorWrapper()) {
                const auto &monitoringParameter = processorWrapper->getParameter(0);
                addAndMakeVisible((monitoringToggle = std::make_unique<TextButton>()).get());
                monitoringToggle->setClickingTogglesState(true);
                monitoringParameter->attachButton(monitoringToggle.get());
            }
        }

        BaseGraphEditorProcessor::valueTreePropertyChanged(v, i);
    }
};
