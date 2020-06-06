#pragma once

#include <view/parameter_control/slider/MinimalSliderControl.h>
#include "view/parameter_control/level_meter/MinimalLevelMeter.h"
#include "BaseGraphEditorProcessor.h"

class TrackInputGraphEditorProcessor : public BaseGraphEditorProcessor, private ChangeListener {
public:
    TrackInputGraphEditorProcessor(Project &project, TracksState &tracks, ViewState &view,
                                    const ValueTree &state, ConnectorDragListener &connectorDragListener) :
            BaseGraphEditorProcessor(project, tracks, view, state, connectorDragListener),
            project(project), tracks(tracks) {
        nameLabel.setJustificationType(Justification::centred);
        addAndMakeVisible(nameLabel);
        nameLabel.addMouseListener(this, false);

        if (!isMasterTrack()) {
            nameLabel.setText(getTrackName(), dontSendNotification);
//            nameLabel.setColour(Label::textColourId, Colours::black);
            nameLabel.setEditable(false, true);
            nameLabel.onTextChange = [this] { this->tracks.setTrackName(this->state, nameLabel.getText(false)); };
        } else {
            masterTrackName.setText(getTrackName());
            masterTrackName.setColour(findColour(TextEditor::textColourId));
            masterTrackName.setJustification(Justification::centred);
            addAndMakeVisible(masterTrackName);
        }
    }

    ~TrackInputGraphEditorProcessor() {
        nameLabel.removeMouseListener(this);
        if (auto *processorWrapper = getProcessorWrapper()) {
            if (monitoringToggle != nullptr) {
                const auto &parameterWrapper = processorWrapper->getParameter(0);
                parameterWrapper->detachButton(monitoringToggle.get());
            }
        }
    }

    String getTrackName() const { return getTrack()[IDs::name].toString(); }

    void setTrackName(const String &trackName) {
        nameLabel.setText(trackName, dontSendNotification);
    }

    void colourChanged() override {
        const auto &backgroundColour = findColour(ResizableWindow::backgroundColourId);
        nameLabel.setColour(Label::backgroundColourId, backgroundColour);
    }

    void mouseDown(const MouseEvent &e) override {
        if (e.eventComponent == &nameLabel) {
            if (e.mods.isPopupMenu()) {
                auto *colourSelector = new ColourSelector();
                colourSelector->setName("background");
                colourSelector->setCurrentColour(findColour(TextButton::buttonColourId));
                colourSelector->addChangeListener(this);
                colourSelector->setColour(ColourSelector::backgroundColourId, Colours::transparentBlack);
                colourSelector->setSize(300, 400);

                CallOutBox::launchAsynchronously(colourSelector, getScreenBounds(), nullptr);
            } else {
                bool isTrackSelected = isSelected();
                project.setTrackSelected(state, !(isTrackSelected && e.mods.isCommandDown()), !(isTrackSelected || e.mods.isCommandDown()));
                project.beginDragging({getTrackIndex(), -1});
            }
        }
    }

    void mouseUp(const MouseEvent &e) override {
        if (e.eventComponent == &nameLabel) {
            if (e.mouseWasClicked() && !e.mods.isCommandDown()) {
                // single click deselects other tracks/processors
                project.setTrackSelected(state, true);
            }
        }
    }

    void resized() override {
        BaseGraphEditorProcessor::resized();
        auto r = getBoxBounds();

        const auto &monitoringToggleBounds = isMasterTrack()
                                             ? r.removeFromTop(r.getWidth())
                                             : r.removeFromLeft(r.getHeight());
        if (monitoringToggle != nullptr)
            monitoringToggle->setBounds(monitoringToggleBounds.reduced(monitoringToggleBounds.getHeight() / 4));
        nameLabel.setBounds(r);
        if (isMasterTrack()) {
            const auto &rFloat = r.toFloat();
            masterTrackName.setBoundingBox(Parallelogram<float>(rFloat.getBottomLeft(), rFloat.getTopLeft(), rFloat.getBottomRight()));
            masterTrackName.setFontHeight(0.75f * r.getWidth());
        }
    }

    void layoutChannel(GraphEditorChannel *pin, float indexPosition, float totalSpaces, const Rectangle<float> &boxBounds) const override {
        if (isMasterTrack() && pin->isInput()) {
            pin->setSize(channelSize, channelSize);
            int x = boxBounds.getX() + indexPosition * channelSize;
            return pin->setTopLeftPosition(x, boxBounds.getY());
        }
        return BaseGraphEditorProcessor::layoutChannel(pin, indexPosition, totalSpaces, boxBounds);
    }

    juce::Point<float> getConnectorDirectionVector(bool isInput) const override {
        if (isMasterTrack() && isInput)
            return {0, -1};

        return BaseGraphEditorProcessor::getConnectorDirectionVector(isInput);
    }

    // TODO do we need this? Might just be able to set colour on all child components
    void paint(Graphics &g) override {
        auto r = getBoxBounds();
        const auto &backgroundColour = findColour(ResizableWindow::backgroundColourId);
        g.setColour(backgroundColour);
        g.fillRect(r);
    }

    bool isInView() override {
        return true;
    }

private:
    Project &project;
    TracksState &tracks;
    Label nameLabel;
    DrawableText masterTrackName;
    std::unique_ptr<ImageButton> monitoringToggle;

    Rectangle<int> getBoxBounds() override {
        return isMasterTrack() ?
               getLocalBounds().withTrimmedLeft(ViewState::TRACKS_VERTICAL_MARGIN).withTrimmedRight(channelSize / 2) :
               getLocalBounds().withTrimmedTop(ViewState::TRACKS_VERTICAL_MARGIN).withTrimmedBottom(channelSize / 2);
    }

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v != state)
            return;

        if (monitoringToggle == nullptr) {
            if (auto *processorWrapper = getProcessorWrapper()) {
                const auto &monitoringParameter = processorWrapper->getParameter(0);
                addAndMakeVisible((monitoringToggle = std::make_unique<ImageButton>()).get(), 0);
                monitoringToggle->setClickingTogglesState(true);
                Image audioImage = ImageCache::getFromMemory(BinaryData::Audio_png, BinaryData::Audio_pngSize);
//                Image monitoringOffImage = ImageCache::getFromMemory(BinaryData::CircleWithLineHorizontal_png, BinaryData::CircleWithLineHorizontal_pngSize);
                monitoringToggle->setImages(false, true, true,
                                            audioImage, 1.0, Colours::black,
                                            {}, 1.0, Colours::transparentBlack,
                                            audioImage, 1.0, Colours::yellow);
                monitoringParameter->attachButton(monitoringToggle.get());
            }
        }

        BaseGraphEditorProcessor::valueTreePropertyChanged(v, i);
    }

    void changeListenerCallback(ChangeBroadcaster *source) override {
        if (auto *cs = dynamic_cast<ColourSelector *> (source)) {
            tracks.setTrackColour(state, cs->getCurrentColour());
        }
    }
};
