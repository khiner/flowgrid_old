#pragma once

#include "view/UiColours.h"
#include "ProcessorGraph.h"
#include "Project.h"
#include "processor_editor/ProcessorEditor.h"

class SelectionPanel : public Component,
                       private ProjectChangeListener {
public:
    SelectionPanel(Project &project, ProcessorGraph &audioGraphBuilder)
            : project(project), audioGraphBuilder(audioGraphBuilder) {
        addAndMakeVisible(titleLabel);
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

        titleLabel.setBounds(r.removeFromTop(22));
        if (processorEditor != nullptr) {
            processorEditor->setBounds(r);
        }
    }

private:
    Project &project;
    ProcessorGraph &audioGraphBuilder;

    Label titleLabel;
    std::unique_ptr<ProcessorEditor> processorEditor {};

    void itemSelected(const ValueTree &item) override {
        for (auto *c : getChildren())
            c->setVisible(false);

        if (!item.isValid()) {
            titleLabel.setText("No item selected", dontSendNotification);
            titleLabel.setVisible(true);
            removeChildComponent(processorEditor.get());
            processorEditor = nullptr;
        } else if (item.hasType(IDs::PROCESSOR)) {
            const String &name = item[IDs::name];
            titleLabel.setText("Processor Selected: " + name, dontSendNotification);

            if (auto *processorWrapper = audioGraphBuilder.getProcessorWrapperForState(item)) {
                if (processorEditor != nullptr) {
                    removeChildComponent(processorEditor.get());
                    processorEditor = nullptr;
                }
                processorEditor = std::make_unique<ProcessorEditor>(processorWrapper->processor);
                addAndMakeVisible(processorEditor.get());
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
