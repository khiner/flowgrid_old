#pragma once

#include "view/UiColours.h"
#include "ProcessorGraph.h"
#include "Project.h"
#include "Utilities.h"

class SelectionPanel : public Component,
                       private ProjectChangeListener {
public:
    SelectionPanel(Project &project, ProcessorGraph &audioGraphBuilder)
            : project(project), audioGraphBuilder(audioGraphBuilder) {
        Utilities::addAndMakeVisible(*this, {&titleLabel});

        for (int paramIndex = 0; paramIndex < MAX_PROCESSOR_PARAMS_TO_DISPLAY; paramIndex++) {
            Slider *slider = new Slider("Param " + String(paramIndex) + ": ");
            addAndMakeVisible(slider);
            
            processorLabels.add(new Label(String(), slider->getName()))->attachToComponent(slider, true);
            processorSliders.add(slider);
        }

        project.addChangeListener(this);

        itemSelected({});
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
    OwnedArray<Label> labels;

    OwnedArray<Label> processorLabels;
    OwnedArray<Slider> processorSliders;

    void itemSelected(const ValueTree &item) override {
        for (auto *c : getChildren())
            c->setVisible(false);

        if (!item.isValid()) {
            titleLabel.setText("No item selected", dontSendNotification);
            titleLabel.setVisible(true);
        } else if (item.hasType(IDs::PROCESSOR)) {
            const String &name = item[IDs::name];
            titleLabel.setText("Processor Selected: " + name, dontSendNotification);

            auto *processorWrapper = audioGraphBuilder.getProcessorWrapperForState(item);
            if (processorWrapper != nullptr) {
                for (int i = 0; i < jmin(processorSliders.size(), processorWrapper->processor->getParameters().size()); i++) {
                    auto *slider = processorSliders.getUnchecked(i);
                    auto *label = processorLabels.getUnchecked(i);
                    slider->setVisible(true);
                    if (auto *parameter = processorWrapper->getParameterObject(i)) {
                        parameter->attachSlider(slider, label);
                    }
                }
            }
        }

        repaint();
        resized();
    }

    void itemRemoved(const ValueTree& item) override {
        if (item == project.getSelectedProcessor()) {
            itemSelected(ValueTree());
        }
    }
};
