#include <processors/TrackOutputProcessor.h>

#include "view/parameter_control/slider/MinimalSliderControl.h"
#include "view/parameter_control/level_meter/MinimalLevelMeter.h"

#include "TrackOutputGraphEditorProcessor.h"

TrackOutputGraphEditorProcessor::TrackOutputGraphEditorProcessor(const ValueTree &state, Track *track, View &view, ProcessorGraph &processorGraph, ConnectorDragListener &connectorDragListener) :
        BaseGraphEditorProcessor(state, track, view, processorGraph, connectorDragListener) {
}

TrackOutputGraphEditorProcessor::~TrackOutputGraphEditorProcessor() {
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

void TrackOutputGraphEditorProcessor::resized() {
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

void TrackOutputGraphEditorProcessor::paint(Graphics &g) {
    const auto &backgroundColour = findColour(ResizableWindow::backgroundColourId);
    g.setColour(backgroundColour);

    const auto &r = getBoxBounds();
    bool isMaster = track->isMaster();
    bool curveTopLeft = false, curveTopRight = isMaster, curveBottomLeft = !isMaster, curveBottomRight = true;
    Path p;
    p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(),
                          4.0f, 4.0f, curveTopLeft, curveTopRight, curveBottomLeft, curveBottomRight);
    g.fillPath(p);
}

Rectangle<int> TrackOutputGraphEditorProcessor::getBoxBounds() const {
    return track->isMaster() ? getLocalBounds() : getLocalBounds().withTrimmedTop(channelSize / 2).withTrimmedBottom(View::TRACKS_MARGIN);
}

void TrackOutputGraphEditorProcessor::valueTreePropertyChanged(ValueTree &v, const Identifier &i) {
    if (v != state) return;

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
