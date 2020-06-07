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

        nameLabel.setText(getTrackName(), dontSendNotification);
        nameLabel.setEditable(false, true);
        nameLabel.onTextChange = [this] { this->tracks.setTrackName(this->state, nameLabel.getText(false)); };
    }

    ~TrackInputGraphEditorProcessor() {
        nameLabel.removeMouseListener(this);
        if (auto *processorWrapper = getProcessorWrapper()) {
            if (audioMonitorToggle != nullptr) {
                const auto &parameterWrapper = processorWrapper->getParameter(0);
                parameterWrapper->detachButton(audioMonitorToggle.get());
            }
            if (midiMonitorToggle != nullptr) {
                const auto &parameterWrapper = processorWrapper->getParameter(1);
                parameterWrapper->detachButton(midiMonitorToggle.get());
            }
        }
    }

    String getTrackName() const { return getTrack()[IDs::name].toString(); }

    void setTrackName(const String &trackName) {
        nameLabel.setText(trackName, dontSendNotification);
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
                project.setTrackSelected(getTrack(), !(isTrackSelected && e.mods.isCommandDown()), !(isTrackSelected || e.mods.isCommandDown()));
                project.beginDragging({getTrackIndex(), -1});
            }
        }
    }

    void mouseUp(const MouseEvent &e) override {
        if (e.eventComponent == &nameLabel) {
            if (e.mouseWasClicked() && !e.mods.isCommandDown()) {
                // single click deselects other tracks/processors
                project.setTrackSelected(getTrack(), true);
            }
        }
    }

    void resized() override {
        BaseGraphEditorProcessor::resized();

        const Rectangle<int> &boxBounds = getBoxBounds();
        int toggleSideLength = boxBounds.getHeight() / 3;
        juce::Rectangle<int> toggleBounds = boxBounds.withSizeKeepingCentre(toggleSideLength, toggleSideLength);

        // safe assumption with these channel checks but could be better by looking for channels with properties
        if (audioMonitorToggle != nullptr) {
            if (channels.size() >= 2) {
                const auto *leftAudioChannel = channels.getUnchecked(0);
                audioMonitorToggle->setBounds(toggleBounds.withCentre({leftAudioChannel->getBounds().getRight(), toggleBounds.getCentreY()}));
            }
        }
        if (midiMonitorToggle != nullptr) {
            if (channels.size() >= 3) {
                const auto *midiChannel = channels.getUnchecked(2);
                midiMonitorToggle->setBounds(toggleBounds.withCentre({midiChannel->getBounds().getCentreX(), toggleBounds.getCentreY()}));
            }
        }

        nameLabel.setBounds(boxBounds.withLeft(midiMonitorToggle != nullptr ? midiMonitorToggle->getRight() : boxBounds.getX()));
    }

    void paint(Graphics &g) override {
        const auto &r = getBoxBounds();
        const auto &backgroundColour = findColour(ResizableWindow::backgroundColourId);
        g.setColour(backgroundColour);
        // hack to get rounded corners only on top:
        // draw two overlapping rects, one with rounded corners
        g.fillRoundedRectangle(r.toFloat(), 4.0f);
        g.fillRect(r.withTop(getHeight() / 2));
    }

    bool isInView() override {
        return true;
    }

    static constexpr int VERTICAL_MARGIN = channelSize / 2;

private:
    Project &project;
    TracksState &tracks;
    Label nameLabel;
    std::unique_ptr<ImageButton> audioMonitorToggle, midiMonitorToggle;

    Rectangle<int> getBoxBounds() override {
        return getLocalBounds().withTrimmedTop(VERTICAL_MARGIN).withTrimmedBottom(VERTICAL_MARGIN);
    }

    void colourChanged() override {
        repaint();
    }

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v != state)
            return;

        if (audioMonitorToggle == nullptr && midiMonitorToggle == nullptr) {
            if (auto *processorWrapper = getProcessorWrapper()) {
                addAndMakeVisible((audioMonitorToggle = std::make_unique<ImageButton>()).get());
                audioMonitorToggle->setClickingTogglesState(true);
                const auto &audioImage = ImageCache::getFromMemory(BinaryData::Audio_png, BinaryData::Audio_pngSize);
                audioMonitorToggle->setImages(false, true, true,
                                              audioImage, 1.0, Colours::black,
                                              {}, 1.0, Colours::transparentBlack,
                                              audioImage, 1.0, findColour(CustomColourIds::defaultAudioConnectionColourId));

                const auto &monitorAudioParameter = processorWrapper->getParameter(0);
                monitorAudioParameter->attachButton(audioMonitorToggle.get());

                addAndMakeVisible((midiMonitorToggle = std::make_unique<ImageButton>()).get());
                midiMonitorToggle->setClickingTogglesState(true);
                const auto &midiImage = ImageCache::getFromMemory(BinaryData::Midi_png, BinaryData::Midi_pngSize);
                midiMonitorToggle->setImages(false, true, true,
                                             midiImage, 1.0, Colours::black,
                                             {}, 1.0, Colours::transparentBlack,
                                             midiImage, 1.0, findColour(CustomColourIds::defaultMidiConnectionColourId));

                const auto &monitorMidiParameter = processorWrapper->getParameter(1);
                monitorMidiParameter->attachButton(midiMonitorToggle.get());
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
