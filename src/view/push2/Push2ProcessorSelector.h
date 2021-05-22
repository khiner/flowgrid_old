#pragma once

#include "Push2ComponentBase.h"
#include "Push2Label.h"
#include "model/Project.h"

class Push2ProcessorSelector : public Push2ComponentBase {
    class ProcessorSelectorRow : public Component {
    public:
        explicit ProcessorSelectorRow(Push2 &push2, bool top);

        Push2Label *findSelectedLabel() const;
        KnownPluginList::PluginTree *findSelectedSubfolder() const;
        int getCurrentViewOffset() const { return currentViewOffset; }

        void setVisible(bool visible) override;
        KnownPluginList::PluginTree *selectFolder(int index);
        void selectFolder(KnownPluginList::PluginTree *subfolder);
        const PluginDescription *selectProcessor(int index) const;
        void setCurrentTree(KnownPluginList::PluginTree *tree);
        void setSelected(bool selected) {
            selectionRectangleOverlay.setFill(selected ? Colours::white.withAlpha(0.1f) : Colours::transparentBlack);
        }

        void setCurrentViewOffset(int currentViewOffset) {
            this->currentViewOffset = currentViewOffset;
            updateLabels();
        }

        void arrowPressed(int direction);

        int getTotalNumberOfTreeItems() const {
            return currentTree != nullptr ? currentTree->subFolders.size() + currentTree->plugins.size() : 0;
        }

        void resized() override;

        KnownPluginList::PluginTree *currentTree{};
        OwnedArray<Push2Label> labels;
    private:
        int currentViewOffset{0};
        DrawableRectangle selectionRectangleOverlay;

        void updateLabels() const;
    };

public:
    Push2ProcessorSelector(View &view, Tracks &tracks, Project &project, Push2MidiCommunicator &push2MidiCommunicator);

    void setVisible(bool visible) override;
    const PluginDescription *selectTopProcessor(int index);
    const PluginDescription *selectBottomProcessor(int index);
    void aboveScreenButtonPressed(int buttonIndex) override;
    void belowScreenButtonPressed(int buttonIndex) override;
    void arrowPressed(int direction) override;
    void resized() override;
    void updateEnabledPush2Buttons() override;

private:
    Project &project;
    KnownPluginList::PluginTree rootTree;
    std::unique_ptr<ProcessorSelectorRow> topProcessorSelector;
    std::unique_ptr<ProcessorSelectorRow> bottomProcessorSelector;
    ProcessorSelectorRow *currentProcessorSelector{};

    void selectProcessorRow(ProcessorSelectorRow *processorSelectorRow);
    void updateEnabledPush2Arrows();

    bool canNavigateRight() const { return currentProcessorSelector->getCurrentViewOffset() + NUM_COLUMNS < currentProcessorSelector->getTotalNumberOfTreeItems(); }
    bool canNavigateDown() const { return currentProcessorSelector->findSelectedLabel() != nullptr; }
    bool canNavigateLeft() const { return currentProcessorSelector->getCurrentViewOffset() > 0; }
    bool canNavigateUp() const { return bottomProcessorSelector.get() == currentProcessorSelector || currentProcessorSelector->currentTree->parent != nullptr; }
    bool canNavigateInDirection(int direction);
};
