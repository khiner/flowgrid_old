#include "TrackInputGraphEditorProcessor.h"

#include "BinaryData.h"

TrackInputGraphEditorProcessor::TrackInputGraphEditorProcessor(const ValueTree &state, Track *track, View &view, Project &project, ProcessorGraph &processorGraph, ConnectorDragListener &connectorDragListener)
        :
        BaseGraphEditorProcessor(state, track, view, processorGraph, connectorDragListener), project(project) {
    nameLabel.setJustificationType(Justification::centred);
    addAndMakeVisible(nameLabel);
    nameLabel.addMouseListener(this, false);

    nameLabel.setText(track->getName(), dontSendNotification);
    nameLabel.setEditable(false, true);
    nameLabel.onTextChange = [this] { this->track->setName(nameLabel.getText(false)); };
}

TrackInputGraphEditorProcessor::~TrackInputGraphEditorProcessor() {
    nameLabel.removeMouseListener(this);
    if (audioMonitorToggle != nullptr && audioMonitorParameter != nullptr) {
        audioMonitorParameter->detachButton(audioMonitorToggle.get());
    }
    if (midiMonitorToggle != nullptr && midiMonitorParameter != nullptr) {
        midiMonitorParameter->detachButton(midiMonitorToggle.get());
    }
    audioMonitorParameter = nullptr;
    midiMonitorParameter = nullptr;
}

void TrackInputGraphEditorProcessor::mouseDown(const MouseEvent &e) {
    if (e.eventComponent == &nameLabel) {
        if (e.mods.isPopupMenu()) {
            auto colourSelector = std::make_unique<ColourSelector>();
            colourSelector->setName("background");
            colourSelector->setCurrentColour(findColour(TextButton::buttonColourId));
            colourSelector->addChangeListener(this);
            colourSelector->setColour(ColourSelector::backgroundColourId, Colours::transparentBlack);
            colourSelector->setSize(300, 400);

            CallOutBox::launchAsynchronously(std::move(colourSelector), getScreenBounds(), nullptr);
        } else {
            bool isTrackSelected = isSelected();
            project.setTrackSelected(track, !(isTrackSelected && e.mods.isCommandDown()), !(isTrackSelected || e.mods.isCommandDown()));
            project.beginDragging({getTrackIndex(), -1});
        }
    }
}

void TrackInputGraphEditorProcessor::mouseUp(const MouseEvent &e) {
    if (e.eventComponent == &nameLabel) {
        if (e.mouseWasClicked() && !e.mods.isCommandDown()) {
            // single click deselects other tracks/processors
            project.setTrackSelected(track, true);
        }
    }
}

void TrackInputGraphEditorProcessor::resized() {
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

void TrackInputGraphEditorProcessor::paint(Graphics &g) {
    const auto &backgroundColour = findColour(ResizableWindow::backgroundColourId);
    g.setColour(backgroundColour);

    const auto &r = getBoxBounds();
    Path p;
    p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(),
                          4.0f, 4.0f, true, true, false, false);
    g.fillPath(p);
}

void TrackInputGraphEditorProcessor::valueTreePropertyChanged(ValueTree &v, const Identifier &i) {
    if (v != state) return;

    // XXX should be done in constructor, but track/processor views are currently instantiated before the processor graph.
    // XXX also, this level meter should be a ParameterDisplayComponent - then we wouldn't have to search for the parameter wrapper.
    if (audioMonitorToggle == nullptr && midiMonitorToggle == nullptr) {
        if (auto *processorWrapper = getProcessorWrapper()) {
            addAndMakeVisible((audioMonitorToggle = std::make_unique<ImageButton>()).get());
            audioMonitorToggle->setClickingTogglesState(true);
            const auto &audioImage = ImageCache::getFromMemory(BinaryData::Audio_png, BinaryData::Audio_pngSize);
            audioMonitorToggle->setImages(false, true, true,
                                          audioImage, 1.0, Colours::black,
                                          {}, 1.0, Colours::transparentBlack,
                                          audioImage, 1.0, findColour(CustomColourIds::defaultAudioConnectionColourId));

            audioMonitorParameter = processorWrapper->getParameter(0);
            audioMonitorParameter->attachButton(audioMonitorToggle.get());

            addAndMakeVisible((midiMonitorToggle = std::make_unique<ImageButton>()).get());
            midiMonitorToggle->setClickingTogglesState(true);
            const auto &midiImage = ImageCache::getFromMemory(BinaryData::Midi_png, BinaryData::Midi_pngSize);
            midiMonitorToggle->setImages(false, true, true,
                                         midiImage, 1.0, Colours::black,
                                         {}, 1.0, Colours::transparentBlack,
                                         midiImage, 1.0, findColour(CustomColourIds::defaultMidiConnectionColourId));

            midiMonitorParameter = processorWrapper->getParameter(1);
            midiMonitorParameter->attachButton(midiMonitorToggle.get());
        }
    }

    BaseGraphEditorProcessor::valueTreePropertyChanged(v, i);
}

void TrackInputGraphEditorProcessor::changeListenerCallback(ChangeBroadcaster *source) {
    if (auto *cs = dynamic_cast<ColourSelector *> (source)) {
        track->setColour(cs->getCurrentColour(), &project.getUndoManager());
    }
}
