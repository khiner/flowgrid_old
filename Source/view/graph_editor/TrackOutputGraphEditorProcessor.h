#pragma once

#include <view/level_meter/MinimalLevelMeter.h>
#include "BaseGraphEditorProcessor.h"

class TrackOutputGraphEditorProcessor : public BaseGraphEditorProcessor {
public:
    TrackOutputGraphEditorProcessor(Project &project, TracksState &tracks, ViewState &view,
                                    const ValueTree &state, ConnectorDragListener &connectorDragListener,
                                    bool showChannelLabels = false) :
    BaseGraphEditorProcessor(project, tracks, view, state, connectorDragListener, showChannelLabels) {
    }

    ~TrackOutputGraphEditorProcessor() {
        if (levelMeter != nullptr) {
            if (auto *processorWrapper = getProcessorWrapper()) {
                addAndMakeVisible((levelMeter = std::make_unique<MinimalLevelMeter>(LevelMeter::horizontal)).get());
                const auto &parameterWrapper = processorWrapper->getParameter(1);
                parameterWrapper->detachLevelMeter(levelMeter.get());
            }
        }
    }

    void resized() override {
        BaseGraphEditorProcessor::resized();
        auto remainingBounds = getBoxBounds().reduced(5).toFloat();

        const auto panBounds = remainingBounds.removeFromTop(remainingBounds.getHeight() / 2).toNearestInt();
//        panControl->setBounds(panBounds);

        if (levelMeter != nullptr) {
            const auto levelMeterBounds = remainingBounds.toNearestInt();
            levelMeter->setBounds(levelMeterBounds);
        }
    }

    bool isInView() override {
        return true;
    }
private:
    std::unique_ptr<MinimalLevelMeter> levelMeter;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v != state)
            return;

        if (levelMeter == nullptr) {
            if (auto *processorWrapper = getProcessorWrapper()) {
                if (auto *trackOutputProcessor = dynamic_cast<TrackOutputProcessor *>(processorWrapper->processor)) {
                    if (auto *levelMeterSource = trackOutputProcessor->getMeterSource()) {
                        addAndMakeVisible((levelMeter = std::make_unique<MinimalLevelMeter>(LevelMeter::horizontal)).get());
                        levelMeter->setMeterSource(levelMeterSource);
                        const auto &parameterWrapper = processorWrapper->getParameter(1);
                        parameterWrapper->attachLevelMeter(levelMeter.get());
//                        parameterWrapper->attachLabel(&valueLabel);
                    }
                }
            }
        }

        BaseGraphEditorProcessor::valueTreePropertyChanged(v, i);
    }
};
