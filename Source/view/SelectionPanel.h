#pragma once

#include "view/ColourChangeButton.h"

class SelectionPanel : public Component,
                       private ProjectChangeListener {
public:
    SelectionPanel(Project &project, ProcessorGraph &audioGraphBuilder)
            : project(project), audioGraphBuilder(audioGraphBuilder) {
        Utilities::addAndMakeVisible(*this, {&titleLabel, &nameEditor, &colourButton, &startSlider, &lengthSlider});

        colourButton.setButtonText("Set colour");

        startSlider.setRange(0.0, 10.0);
        lengthSlider.setRange(0.0, 10.0);

        Utilities::visitComponents({&nameEditor, &colourButton, &startSlider, &lengthSlider},
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

        itemSelected(ValueTree());
    }

    ~SelectionPanel() override {
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

        Utilities::visitComponents({&nameEditor, &colourButton, &startSlider, &lengthSlider},
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
    ProcessorGraph &audioGraphBuilder;

    Label titleLabel;
    TextEditor nameEditor{"Name: "};
    ColourChangeButton colourButton{"Colour: "};
    Slider startSlider{"Start: "}, lengthSlider{"Length: "};
    OwnedArray<Label> labels;

    OwnedArray<Label> processorLabels;
    OwnedArray<Slider> processorSliders;

    void itemSelected(ValueTree item) override {
        for (auto *c : getChildren())
            c->setVisible(false);

        if (!item.isValid()) {
            titleLabel.setText("No item selected", dontSendNotification);
            titleLabel.setVisible(true);
        } else if (item.hasType(IDs::PROCESSOR)) {
            const String &name = item.getProperty(IDs::name);
            titleLabel.setText("Processor Selected: " + name, dontSendNotification);

            String uuid = item[IDs::uuid];
            StatefulAudioProcessor *processor = audioGraphBuilder.getProcessorForUuid(uuid);
            if (processor != nullptr) {
                for (int i = 0; i < processor->getNumParameters(); i++) {
                    auto *slider = processorSliders.getUnchecked(i);
                    auto *label = processorLabels.getUnchecked(i);
                    slider->setVisible(true);
                    processor->getParameterInfo(i)->attachSlider(slider, label);
                }
            }
        } else if (item.hasType(IDs::CLIP)) {
            const String &name = item.getProperty(IDs::name);
            titleLabel.setText("Clip Selected: " + name, dontSendNotification);
            nameEditor.setVisible(true);
            startSlider.setVisible(true);
            lengthSlider.setVisible(true);

            nameEditor.getTextValue().referTo(item.getPropertyAsValue(IDs::name, project.getUndoManager()));
            startSlider.getValueObject().referTo(
                    item.getPropertyAsValue(IDs::start, project.getUndoManager()));
            lengthSlider.getValueObject().referTo(
                    item.getPropertyAsValue(IDs::length, project.getUndoManager()));
        } else if (item.hasType(IDs::TRACK)) {
            const String &name = item.getProperty(IDs::name);
            titleLabel.setText("Track Selected: " + name, dontSendNotification);
            nameEditor.setVisible(true);
            colourButton.setVisible(true);

            nameEditor.getTextValue().referTo(item.getPropertyAsValue(IDs::name, project.getUndoManager()));
            colourButton.getColourValueObject().referTo(item.getPropertyAsValue(IDs::colour, project.getUndoManager()));
        }

        repaint();
        resized();
    }

    void itemRemoved(ValueTree item) override {
        if (item == project.getSelectedProcessor()) {
            itemSelected(ValueTree());
        }
    }
};
