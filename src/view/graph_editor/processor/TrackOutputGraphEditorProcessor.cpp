#include <processors/TrackOutputProcessor.h>

#include "view/parameter_control/slider/MinimalSliderControl.h"
#include "view/parameter_control/level_meter/MinimalLevelMeter.h"

#include "TrackOutputGraphEditorProcessor.h"

TrackOutputGraphEditorProcessor::TrackOutputGraphEditorProcessor(const ValueTree &state, Track *track, View &view, StatefulAudioProcessorWrappers &processorWrappers, ConnectorDragListener &connectorDragListener) :
        BaseGraphEditorProcessor(state, track, view, processorWrappers, connectorDragListener) {
}

TrackOutputGraphEditorProcessor::~TrackOutputGraphEditorProcessor() {
    if (panSlider != nullptr && panParameter != nullptr) {
        panParameter->detachParameterControl(panSlider.get());
    }
    if (levelMeter != nullptr && gainParameter != nullptr) {
        gainParameter->detachParameterControl(levelMeter.get());
    }
    panParameter = nullptr;
    gainParameter = nullptr;
}

void TrackOutputGraphEditorProcessor::resized() {
    BaseGraphEditorProcessor::resized();

    const auto &boxBounds = getBoxBounds();
    auto remainingBounds = boxBounds.reduced(5);
    if (panSlider != nullptr) {
        panSlider->setBounds(remainingBounds.removeFromTop(remainingBounds.getHeight() / 2).reduced(0, boxBounds.getHeight() / 7));
    }
    if (levelMeter != nullptr) {
        levelMeter->setBounds(remainingBounds.reduced(0, boxBounds.getHeight() / 14));
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

    // XXX should be done in constructor, but track/processor views are currently instantiated before the processor graph.
    // XXX also, this level meter should be a ParameterDisplayComponent - then we wouldn't have to search for the parameter wrapper.
    if (levelMeter == nullptr) {
        if (auto *processorWrapper = getProcessorWrapper()) {
            if (auto *trackOutputProcessor = dynamic_cast<TrackOutputProcessor *>(processorWrapper->processor)) {
                if (auto *levelMeterSource = trackOutputProcessor->getMeterSource()) {
                    addAndMakeVisible((panSlider = std::make_unique<MinimalSliderControl>(SliderControl::Orientation::horizontal, true)).get());
                    panParameter = processorWrapper->getParameter(0);
                    panParameter->attachParameterControl(panSlider.get());
                    panSlider->getProperties().set("fromCentre", true);

                    addAndMakeVisible((levelMeter = std::make_unique<MinimalLevelMeter>(LevelMeter::horizontal)).get());
                    levelMeter->setMeterSource(levelMeterSource);
                    gainParameter = processorWrapper->getParameter(1);
                    gainParameter->attachParameterControl(levelMeter.get());
                }
            }
        }
    }

    BaseGraphEditorProcessor::valueTreePropertyChanged(v, i);
}
