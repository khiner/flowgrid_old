#pragma once

#include <view/parameter_control/slider/MinimalSliderControl.h>
#include "view/parameter_control/level_meter/MinimalLevelMeter.h"
#include "BaseGraphEditorProcessor.h"

class TrackOutputGraphEditorProcessor : public BaseGraphEditorProcessor {
public:
    TrackOutputGraphEditorProcessor(Project &project, TracksState &tracks, ViewState &view,
                                    const ValueTree &state, ConnectorDragListener &connectorDragListener) :
            BaseGraphEditorProcessor(project, tracks, view, state, connectorDragListener) {
    }

    ~TrackOutputGraphEditorProcessor() {
        if (auto *processorWrapper = getProcessorWrapper()) {
            if (panSlider != nullptr) {
                const auto &parameterWrapper = processorWrapper->getParameter(0);
                parameterWrapper->detachParameterControl(panSlider.get());
            }
            if (levelMeter != nullptr) {
                const auto &parameterWrapper = processorWrapper->getParameter(1);
                parameterWrapper->detachParameterControl(levelMeter.get());
            }
        }
    }

    void resized() override {
        BaseGraphEditorProcessor::resized();
        const auto &boxBounds = getBoxBounds();
        auto remainingBounds = boxBounds.reduced(5);

        if (panSlider != nullptr) {
            const auto panBounds = remainingBounds.removeFromTop(remainingBounds.getHeight() / 2).reduced(0, boxBounds.getHeight() / 7);
            panSlider->setBounds(panBounds);
        }

        if (levelMeter != nullptr) {
            const auto levelMeterBounds = remainingBounds.reduced(0, boxBounds.getHeight() / 14);
            levelMeter->setBounds(levelMeterBounds);
        }
    }

    void paint(Graphics &g) override {
        const auto &r = getBoxBounds();
        const auto &backgroundColour = findColour(ResizableWindow::backgroundColourId);
        g.setColour(backgroundColour);
        // hack to get rounded corners only on bottom:
        // draw two overlapping rects, one with rounded corners
        if (isMasterTrack()) {
            g.fillRect(r.withWidth(getWidth() / 4));
            g.fillRoundedRectangle(r.toFloat(), 4.0f);
        } else {
            g.fillRect(r.withHeight(getHeight() / 4));
            g.fillRoundedRectangle(r.toFloat(), 4.0f);
        }
    }

    bool isInView() override {
        return true;
    }

private:
    std::unique_ptr<LevelMeter> levelMeter;
    std::unique_ptr<SliderControl> panSlider;

    Rectangle<int> getBoxBounds() override {
        return isMasterTrack() ?
               getLocalBounds() :
               getLocalBounds().withTrimmedTop(channelSize / 2).withTrimmedBottom(ViewState::TRACKS_MARGIN);
    }

    void colourChanged() override {
        repaint();
    }

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v != state)
            return;

        if (levelMeter == nullptr) {
            if (auto *processorWrapper = getProcessorWrapper()) {
                if (auto *trackOutputProcessor = dynamic_cast<TrackOutputProcessor *>(processorWrapper->processor)) {
                    if (auto *levelMeterSource = trackOutputProcessor->getMeterSource()) {
                        addAndMakeVisible((panSlider = std::make_unique<MinimalSliderControl>(SliderControl::Orientation::horizontal, true)).get());
                        const auto &panParameter = processorWrapper->getParameter(0);
                        panParameter->attachParameterControl(panSlider.get());
                        panSlider->getProperties().set("fromCentre", true);

                        addAndMakeVisible((levelMeter = std::make_unique<MinimalLevelMeter>(LevelMeter::horizontal)).get());
                        levelMeter->setMeterSource(levelMeterSource);
                        const auto &gainParameter = processorWrapper->getParameter(1);
                        gainParameter->attachParameterControl(levelMeter.get());
                    }
                }
            }
        }

        BaseGraphEditorProcessor::valueTreePropertyChanged(v, i);
    }
};
