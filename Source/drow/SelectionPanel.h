#pragma once

#include "Components.h"
#include "ValueTreeItems.h"
#include "drow_Utilities.h"

class SelectionPanel : public Component,
                       private ProjectChangeListener {
public:
    SelectionPanel(Project &e, AudioGraphBuilder &audioGraphBuilder)
            : project(e), audioGraphBuilder(audioGraphBuilder) {
        drow::addAndMakeVisible(*this, {&titleLabel, &nameEditor, &colourButton, &startSlider, &lengthSlider});

        colourButton.setButtonText("Set colour");

        startSlider.setRange(0.0, 10.0);
        lengthSlider.setRange(0.0, 10.0);

        drow::visitComponents({&nameEditor, &colourButton, &startSlider, &lengthSlider},
                              [this](Component *c) {
                                  labels.add(new Label(String(), c->getName()))->attachToComponent(c, true);
                              });

        for (int paramIndex = 0; paramIndex < MAX_PROCESSOR_PARAMS_TO_DISPLAY; paramIndex++) {
            Slider *slider = new Slider("Param " + String(paramIndex) + ": ");
            addAndMakeVisible(slider);
            
            processorLabels.add(new Label(String(), slider->getName()))->attachToComponent(slider, true);
            processorSliders.add(slider);
        }

        project.addChangeListener(this);

        itemSelected(nullptr);
    }

    ~SelectionPanel() {
        project.removeChangeListener(this);
    }

    void paint(Graphics &g) override {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override {
        auto r = getLocalBounds();

        static const int ITEM_WIDTH = 250, ITEM_HEIGHT = 22;
        titleLabel.setBounds(r.removeFromTop(ITEM_HEIGHT));
        r = r.withTrimmedLeft(70).withWidth(ITEM_WIDTH);

        drow::visitComponents({&nameEditor, &colourButton, &startSlider, &lengthSlider},
                              [&r](Component *c) { if (c->isVisible()) c->setBounds(r.removeFromTop(ITEM_HEIGHT)); });

        for (int sliderIndex = 0; sliderIndex < processorSliders.size(); sliderIndex++) {
            auto* slider = processorSliders[sliderIndex];
            if (slider->isVisible()) {
                slider->setBounds(r.removeFromTop(ITEM_HEIGHT));
                if (sliderIndex == processorSliders.size() / 2 - 1) {
                    // layout the other column to the right of the first
                    r = getLocalBounds()
                            .withTrimmedLeft(ITEM_WIDTH + slider->getTextBoxWidth() + processorLabels[sliderIndex]->getWidth())
                            .withTrimmedTop(ITEM_HEIGHT)
                            .withWidth(ITEM_WIDTH);
                }
            }
        }
    }

private:
    const static int MAX_PROCESSOR_PARAMS_TO_DISPLAY = 8;

    Project &project;
    AudioGraphBuilder &audioGraphBuilder;

    Label titleLabel;
    TextEditor nameEditor{"Name: "};
    ColourChangeButton colourButton{"Colour: "};
    Slider startSlider{"Start: "}, lengthSlider{"Length: "};
    OwnedArray<Label> labels;

    OwnedArray<Label> processorLabels;
    OwnedArray<Slider> processorSliders;
    OwnedArray<AudioProcessorValueTreeState::SliderAttachment> processorSliderAttachements;

    void itemSelected(ValueTreeItem *item) override {
        for (auto *c : getChildren())
            c->setVisible(false);

        if (item == nullptr) {
            titleLabel.setText("No item selected", dontSendNotification);
            titleLabel.setVisible(true);
        } else if (auto *processor = dynamic_cast<Processor *> (item)) {
            titleLabel.setText("Processor Selected: " + processor->getDisplayText(), dontSendNotification);

            processorSliderAttachements.clear(true);

            StatefulAudioProcessor *selectedAudioProcessor = audioGraphBuilder.getAudioProcessorWithUuid(processor->getState().getProperty(IDs::uuid, processor->getUndoManager()));
            if (selectedAudioProcessor != nullptr) {
                for (int i = 0; i < 8; i++) {
                    auto *slider = processorSliders[i];
                    slider->setVisible(true);
                    slider->getValueObject().refersToSameSourceAs(processor->getState().getChild(i).getPropertyAsValue(IDs::value, processor->getUndoManager()));
                    auto sliderAttachment = new AudioProcessorValueTreeState::SliderAttachment(
                            *selectedAudioProcessor->getState(), selectedAudioProcessor->getParameterIdentifier(i),
                            *slider);
                    processorSliderAttachements.add(sliderAttachment);
                }
            }
        } else if (auto *clip = dynamic_cast<Clip *> (item)) {
            titleLabel.setText("Clip Selected: " + clip->getDisplayText(), dontSendNotification);
            nameEditor.setVisible(true);
            startSlider.setVisible(true);
            lengthSlider.setVisible(true);

            nameEditor.getTextValue().referTo(clip->getState().getPropertyAsValue(IDs::name, clip->getUndoManager()));
            startSlider.getValueObject().referTo(
                    clip->getState().getPropertyAsValue(IDs::start, clip->getUndoManager()));
            lengthSlider.getValueObject().referTo(
                    clip->getState().getPropertyAsValue(IDs::length, clip->getUndoManager()));
        } else if (auto *track = dynamic_cast<Track *> (item)) {
            titleLabel.setText("Track Selected: " + track->getDisplayText(), dontSendNotification);
            nameEditor.setVisible(true);
            colourButton.setVisible(true);

            nameEditor.getTextValue().referTo(track->getState().getPropertyAsValue(IDs::name, track->getUndoManager()));
            colourButton.getColourValueObject().referTo(
                    track->getState().getPropertyAsValue(IDs::colour, track->getUndoManager()));
        }

        repaint();
        resized();
    }
};
