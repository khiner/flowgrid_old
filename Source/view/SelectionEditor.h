#include <JuceHeader.h>
#include <Utilities.h>
#include <Identifiers.h>
#include <ValueTreeItems.h>
#include <processors/ProcessorManager.h>
#include "UiColours.h"
#include "processor_editor/ProcessorEditor.h"

class SelectionEditor : public Component,
                        public DragAndDropContainer,
                        private ProjectChangeListener,
                        private Button::Listener,
                        private Timer {
public:
    SelectionEditor(const ValueTree &state, UndoManager &undoManager, Project& project, ProcessorGraph &audioGraphBuilder)
            : undoManager(undoManager), project(project), audioGraphBuilder(audioGraphBuilder) {
        project.addChangeListener(this);
        addAndMakeVisible(titleLabel);
        addChildComponent((processorEditor = std::make_unique<ProcessorEditor>()).get());
        Utilities::visitComponents({&undoButton, &redoButton, &createTrackButton, &addProcessorButton},
                                   [this](Component *c) { addAndMakeVisible(c); });

        undoButton.addListener(this);
        redoButton.addListener(this);
        createTrackButton.addListener(this);
        addProcessorButton.addListener(this);

        startTimer(500);
        setSize(800, 600);
    }

    void resized() override {
        Rectangle<int> r(getLocalBounds().reduced(4));

        Rectangle<int> buttons(r.removeFromBottom(22));
        undoButton.setBounds(buttons.removeFromLeft(100));
        buttons.removeFromLeft(6);
        redoButton.setBounds(buttons.removeFromLeft(100));

        buttons.removeFromLeft(6);
        createTrackButton.setBounds(buttons.removeFromLeft(100));

        buttons.removeFromLeft(6);
        addProcessorButton.setBounds(buttons.removeFromLeft(120));

        r.removeFromBottom(4);

        titleLabel.setBounds(r.removeFromTop(22));
        if (processorEditor != nullptr) {
            processorEditor->setBounds(r);
        }
    }

    void buttonClicked(Button *b) override {
        if (b == &undoButton) {
            undoManager.undo();
        } else if (b == &redoButton) {
            undoManager.redo();
        } else if (b == &createTrackButton) {
            project.createAndAddTrack();
        } else if (b == &addProcessorButton) {
            if (project.getSelectedTrack().isValid()) {
                addProcessorMenu = std::make_unique<PopupMenu>();
                project.addPluginsToMenu(*addProcessorMenu, project.getSelectedTrack());
                addProcessorMenu->showMenuAsync({}, ModalCallbackFunction::create([this](int r) {
                    if (auto *description = project.getChosenType(r)) {
                        project.createAndAddProcessor(*description);
                    }
                }));
            }
        }
    }

    void itemSelected(const ValueTree& item) override {
        const ValueTree &selectedTrack = project.getSelectedTrack();
        addProcessorButton.setVisible(selectedTrack.isValid());

        if (!item.isValid()) {
            titleLabel.setText("No item selected", dontSendNotification);
            titleLabel.setVisible(true);
            processorEditor->setVisible(false);
            processorEditor->setProcessor(nullptr);
        } else if (item.hasType(IDs::PROCESSOR)) {
            const String &name = item[IDs::name];
            titleLabel.setText("Processor Selected: " + name, dontSendNotification);

            if (auto *processorWrapper = audioGraphBuilder.getProcessorWrapperForState(item)) {
                processorEditor->setProcessor(processorWrapper->processor);
                processorEditor->setVisible(true);
            }
        }

        resized();
    }

    void itemRemoved(const ValueTree& item) override {
        if (item == project.getSelectedTrack()) {
            addProcessorButton.setVisible(false);
        } else if (item == project.getSelectedProcessor()) {
            itemSelected(ValueTree());
        }
    }

private:
    TextButton undoButton{"Undo"}, redoButton{"Redo"}, createTrackButton{"Add Track"};

    TextButton addProcessorButton{"Add Processor"};

    Label titleLabel;
    std::unique_ptr<ProcessorEditor> processorEditor {};

    UndoManager &undoManager;
    Project &project;
    ProcessorGraph &audioGraphBuilder;

    std::unique_ptr<PopupMenu> addProcessorMenu;

    void timerCallback() override {
        undoManager.beginNewTransaction();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectionEditor)
};
