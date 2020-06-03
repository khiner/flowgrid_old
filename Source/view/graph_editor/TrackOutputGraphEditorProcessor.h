#pragma once

#include "view/parameter_control/level_meter/MinimalLevelMeter.h"
#include "BaseGraphEditorProcessor.h"

class TrackOutputGraphEditorProcessor : public BaseGraphEditorProcessor {
public:
    TrackOutputGraphEditorProcessor(Project &project, TracksState &tracks, ViewState &view,
                                    const ValueTree &state, ConnectorDragListener &connectorDragListener) :
            BaseGraphEditorProcessor(project, tracks, view, state, connectorDragListener, false) {
    }

    ~TrackOutputGraphEditorProcessor() {
        if (auto *processorWrapper = getProcessorWrapper()) {
            if (panSlider != nullptr) {
                const auto &parameterWrapper = processorWrapper->getParameter(0);
                parameterWrapper->detachSlider(panSlider.get());
            }
            if (levelMeter != nullptr) {
                const auto &parameterWrapper = processorWrapper->getParameter(1);
                parameterWrapper->detachLevelMeter(levelMeter.get());
            }
        }
    }

    void resized() override {
        BaseGraphEditorProcessor::resized();
        auto remainingBounds = getBoxBounds().reduced(5).toFloat();

        if (panSlider != nullptr) {
            const auto panBounds = remainingBounds.removeFromTop(remainingBounds.getHeight() / 2).toNearestInt();
            panSlider->setBounds(panBounds);
        }

        if (levelMeter != nullptr) {
            const auto levelMeterBounds = remainingBounds.removeFromTop(remainingBounds.getHeight() / 2).toNearestInt();
            levelMeter->setBounds(levelMeterBounds);
        }
    }

    void paint(Graphics &g) override {
        auto r = getBoxBounds();
        const auto &boxColour = findColour(ResizableWindow::backgroundColourId);
//        auto boxColour = findColour(TextEditor::backgroundColourId);
        g.setColour(boxColour);
        // hack to get rounded corners only on bottom:
        // draw two overlapping rects, one with rounded corners
        g.fillRect(r.withHeight(getHeight() / 4));
        g.fillRoundedRectangle(r.toFloat(), 6.0f);
    }

    bool isInView() override {
        return true;
    }
private:
    std::unique_ptr<MinimalLevelMeter> levelMeter;
    std::unique_ptr<Slider> panSlider;

    Rectangle<int> getBoxBounds() override {
        return getLocalBounds().withTop(pinSize).withTrimmedBottom(ViewState::TRACK_BOTTOM_MARGIN);
    }

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v != state)
            return;

        if (levelMeter == nullptr) {
            if (auto *processorWrapper = getProcessorWrapper()) {
                if (auto *trackOutputProcessor = dynamic_cast<TrackOutputProcessor *>(processorWrapper->processor)) {
                    if (auto *levelMeterSource = trackOutputProcessor->getMeterSource()) {
                        addAndMakeVisible((panSlider = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextEntryBoxPosition::NoTextBox)).get());
                        const auto &panParameter = processorWrapper->getParameter(0);
                        panParameter->attachSlider(panSlider.get());
                        panSlider->getProperties().set("fromCentre", true);

                        addAndMakeVisible((levelMeter = std::make_unique<MinimalLevelMeter>(LevelMeter::horizontal)).get());
                        levelMeter->setMeterSource(levelMeterSource);
                        const auto &gainParameter = processorWrapper->getParameter(1);
                        gainParameter->attachLevelMeter(levelMeter.get());
                    }
                }
            }
        }

        BaseGraphEditorProcessor::valueTreePropertyChanged(v, i);
    }
};
