#pragma once

#include <utility>
#include "JuceHeader.h"
#include "Push2ComponentBase.h"

class Push2ProcessorSelector : public Push2ComponentBase {
    class ProcessorSelectorRow : public Component {
    public:
        explicit ProcessorSelectorRow(Project &project, Push2 &push2, bool top)
                : project(project), tracksManager(project.getTracksManager()) {
            for (int i = 0; i < NUM_COLUMNS; i++) {
                auto *label = new Push2Label(i, top, push2);
                addChildComponent(label);
                label->setJustificationType(Justification::centred);
                labels.add(label);
            }
            addAndMakeVisible(selectionRectangleOverlay);
        }

        void setVisible(bool visible) override {
            Component::setVisible(visible);
            updateLabels();
        }

        KnownPluginList::PluginTree* selectFolder(int index) {
            if (currentTree == nullptr)
                return nullptr;
            if (index + currentViewOffset < currentTree->subFolders.size()) {
                auto *subfolder = currentTree->subFolders.getUnchecked(index + currentViewOffset);
                selectFolder(subfolder);
                return subfolder;
            }

            return nullptr;
        }

        void selectFolder(KnownPluginList::PluginTree* subfolder) {
            jassert(currentTree->subFolders.contains(subfolder));

            for (auto* folder : currentTree->subFolders) {
                folder->selected = false;
            }
            subfolder->selected = true;
            updateLabels();
        }

        const PluginDescription* selectProcessor(int index) {
            if (currentTree == nullptr)
                return nullptr;

            if (index + currentViewOffset >= currentTree->subFolders.size()) {
                index -= currentTree->subFolders.size();
                if (index + currentViewOffset < currentTree->plugins.size() && labels[index]->isEnabled()) {
                    return &currentTree->plugins.getReference(index + currentViewOffset);
                }
            }

            return nullptr;
        }

        void setCurrentTree(KnownPluginList::PluginTree* tree) {
            if (currentTree == tree)
                return;

            if (currentTree != nullptr && currentTree->subFolders.contains(tree)) {
                for (auto* folder : currentTree->subFolders) {
                    folder->selected = false;
                }
            }
            currentTree = tree;
            if (currentTree != nullptr)
                currentTree->selected = true;
            setCurrentViewOffset(0);
        }

        void setSelected(bool selected) {
            selectionRectangleOverlay.setFill(selected ? Colours::white.withAlpha(0.1f) : Colours::transparentBlack);
        }

        int getCurrentViewOffset() {
            return currentViewOffset;
        }

        void setCurrentViewOffset(int currentViewOffset) {
            this->currentViewOffset = currentViewOffset;
            updateLabels();
        }

        KnownPluginList::PluginTree* findSelectedSubfolder() const {
            if (currentTree == nullptr)
                return nullptr;

            for (int i = 0; i < jmin(getTotalNumberOfTreeItems() - currentViewOffset, NUM_COLUMNS); i++) {
                if (i + currentViewOffset < currentTree->subFolders.size()) {
                    auto *subfolder = currentTree->subFolders.getUnchecked(i + currentViewOffset);
                    if (subfolder->selected)
                        return subfolder;
                }
            }
            if (currentViewOffset < currentTree->subFolders.size()) {
                // default to first subfolder in view
                return currentTree->subFolders.getUnchecked(currentViewOffset);
            }

            return nullptr;
        }
        
        Push2Label* findSelectedLabel() const {
            auto* selectedSubfolder = findSelectedSubfolder();
            if (selectedSubfolder != nullptr)
                return labels.getUnchecked(currentTree->subFolders.indexOf(selectedSubfolder) - currentViewOffset);

            return nullptr;
        }

        void arrowPressed(int direction) {
            if (direction == Push2::leftArrowDirection)
                setCurrentViewOffset(jmax(0, currentViewOffset - NUM_COLUMNS));
            else if (direction == Push2::rightArrowDirection && currentTree != nullptr)
                setCurrentViewOffset(jmin(currentViewOffset + NUM_COLUMNS,
                        getTotalNumberOfTreeItems() - getTotalNumberOfTreeItems() % NUM_COLUMNS));
        }

        int getTotalNumberOfTreeItems() const {
            return currentTree != nullptr ? currentTree->subFolders.size() + currentTree->plugins.size() : 0;
        }

        void resized() override {
            auto r = getLocalBounds();
            for (auto *label : labels) {
                label->setBounds(r.removeFromLeft(getWidth() / NUM_COLUMNS));
            }
            selectionRectangleOverlay.setRectangle(getLocalBounds().toFloat());
        }

        KnownPluginList::PluginTree* currentTree{};
        OwnedArray<Push2Label> labels;
    private:
        int currentViewOffset { 0 };
        Project &project;
        TracksStateManager &tracksManager;
        DrawableRectangle selectionRectangleOverlay;

        void updateLabels() {
            const bool trackHasMixerAlready = tracksManager.selectedTrackHasMixerChannel();

            for (int i = 0; i < labels.size(); i++) {
                Push2Label *label = labels.getUnchecked(i);
                label->setSelected(false);
                label->setVisible(false);
            }

            if (currentTree == nullptr)
                return;

            for (int i = 0; i < jmin(getTotalNumberOfTreeItems() - currentViewOffset, NUM_COLUMNS); i++) {
                Push2Label *label = labels.getUnchecked(i);
                labels[i]->setVisible(true);
                if (i + currentViewOffset < currentTree->subFolders.size()) {
                    KnownPluginList::PluginTree *subFolder = currentTree->subFolders.getUnchecked(i + currentViewOffset);
                    label->setText(subFolder->folder, dontSendNotification);
                    label->setUnderlined(true);
                } else if (i + currentViewOffset < getTotalNumberOfTreeItems()) {
                    const PluginDescription *plugin = &currentTree->plugins.getReference(i + currentViewOffset - currentTree->subFolders.size());
                    label->setText(plugin->name, dontSendNotification);
                    label->setEnabled(!trackHasMixerAlready || plugin->name != MixerChannelProcessor::name());
                    label->setUnderlined(false);
                }
            }
            if (auto *selectedLabel = findSelectedLabel()) {
                selectedLabel->setSelected(true);
            }
        }
    };

public:
    Push2ProcessorSelector(Project &project, Push2MidiCommunicator &push2MidiCommunicator)
            : Push2ComponentBase(project, push2MidiCommunicator) {
        topProcessorSelector = std::make_unique<ProcessorSelectorRow>(project, push2, true);
        bottomProcessorSelector = std::make_unique<ProcessorSelectorRow>(project, push2, false);
        addChildComponent(topProcessorSelector.get());
        addChildComponent(bottomProcessorSelector.get());

        rootTree = KnownPluginList::PluginTree();

        auto internalPluginTree = project.getUserCreatablePluginListInternal().createTree(project.getPluginSortMethod());
        internalPluginTree->folder = "Internal";
        auto externalPluginTree = project.getPluginListExternal().createTree(project.getPluginSortMethod());
        externalPluginTree->folder = "External";
        internalPluginTree->parent = &rootTree;
        externalPluginTree->parent = &rootTree;

        rootTree.subFolders.add(std::move(internalPluginTree));
        rootTree.subFolders.add(std::move(externalPluginTree));

        setCurrentTree(topProcessorSelector.get(), &rootTree);
        selectProcessorRow(topProcessorSelector.get());
    }

    void setVisible(bool visible) override {
        Push2ComponentBase::setVisible(visible);
        topProcessorSelector->setVisible(visible);
        bottomProcessorSelector->setVisible(visible);
    }

    const PluginDescription* selectTopProcessor(int index) {
        if (const auto *description = topProcessorSelector->selectProcessor(index))
            return description;

        if (auto* subfolder = topProcessorSelector->selectFolder(index)) {
            setCurrentTree(bottomProcessorSelector.get(), subfolder);
            selectProcessorRow(bottomProcessorSelector.get());
            updateEnabledPush2Arrows();
        }
        return nullptr;
    }

    const PluginDescription* selectBottomProcessor(int index) {
        if (const auto *description = bottomProcessorSelector->selectProcessor(index))
            return description;

        if (auto* subfolder = bottomProcessorSelector->selectFolder(index)) {
            setCurrentTree(topProcessorSelector.get(), bottomProcessorSelector->currentTree);
            topProcessorSelector->setCurrentViewOffset(bottomProcessorSelector->getCurrentViewOffset());
            setCurrentTree(bottomProcessorSelector.get(), subfolder);
            updateEnabledPush2Arrows();
        }
        return nullptr;
    }

    void aboveScreenButtonPressed(int buttonIndex) override {
        if (const auto* selectedProcessor = selectTopProcessor(buttonIndex)) {
            tracksManager.createAndAddProcessor(*selectedProcessor, &project.getUndoManager());
        }
    }

    void belowScreenButtonPressed(int buttonIndex) override {
        if (const auto* selectedProcessor = selectBottomProcessor(buttonIndex)) {
            tracksManager.createAndAddProcessor(*selectedProcessor, &project.getUndoManager());
        }
    }

    void arrowPressed(int direction) override {
        if (!canNavigateInDirection(direction))
            return;
        if (currentProcessorSelector != nullptr && (direction == Push2::leftArrowDirection || direction == Push2::rightArrowDirection)) {
            currentProcessorSelector->arrowPressed(direction);
        } else if (direction == Push2::downArrowDirection) {
            if (auto* selectedLabel = currentProcessorSelector->findSelectedLabel()) {
                int selectedIndex = currentProcessorSelector->labels.indexOf(selectedLabel);
                if (currentProcessorSelector == topProcessorSelector.get()) {
                    selectTopProcessor(selectedIndex);
                } else if (currentProcessorSelector == bottomProcessorSelector.get()) {
                    selectBottomProcessor(selectedIndex);
                }
            }
        } else if (direction == Push2::upArrowDirection) {
            if (currentProcessorSelector == bottomProcessorSelector.get()) {
                selectProcessorRow(topProcessorSelector.get());
            } else if (auto* parent = currentProcessorSelector->currentTree->parent) {
                setCurrentTree(bottomProcessorSelector.get(), topProcessorSelector->currentTree);
                bottomProcessorSelector->setCurrentViewOffset(topProcessorSelector->getCurrentViewOffset());
                bottomProcessorSelector->selectFolder(topProcessorSelector->findSelectedSubfolder());
                setCurrentTree(topProcessorSelector.get(), parent);
            }
        }
        updateEnabledPush2Arrows();
    }

    void resized() override {
        auto r = getLocalBounds();
        topProcessorSelector->setBounds(r.removeFromTop(30));
        bottomProcessorSelector->setBounds(r.removeFromBottom(30));
    }

    void updateEnabledPush2Buttons() override {
        if (isVisible())
            push2.activateWhiteLedButton(Push2::addDevice);
        else
            push2.enableWhiteLedButton(Push2::addDevice);
        updateEnabledPush2Arrows();
    }

private:
    void setCurrentTree(ProcessorSelectorRow *processorSelectorRow, KnownPluginList::PluginTree* tree) {
        processorSelectorRow->setCurrentTree(tree);
    }

    void selectProcessorRow(ProcessorSelectorRow* processorSelectorRow) {
        if (currentProcessorSelector != nullptr) {
            currentProcessorSelector->setSelected(false);
        }
        if (processorSelectorRow) {
            currentProcessorSelector = processorSelectorRow;
            currentProcessorSelector->setSelected(true);
        }
    }

    void updateEnabledPush2Arrows() {
        for (int direction : { Push2::upArrowDirection, Push2::downArrowDirection, Push2::leftArrowDirection, Push2::rightArrowDirection }) {
            if (isVisible() && currentProcessorSelector != nullptr && canNavigateInDirection(direction))
                push2.activateWhiteLedButton(Push2::ccNumberForArrowButton(direction));
            else
                push2.disableWhiteLedButton(Push2::ccNumberForArrowButton(direction));
        }
    }

    bool canNavigateInDirection(int direction) {
        switch (direction) {
            case Push2::rightArrowDirection: return canNavigateRight();
            case Push2::downArrowDirection: return canNavigateDown();
            case Push2::leftArrowDirection: return canNavigateLeft();
            case Push2::upArrowDirection: return canNavigateUp();
            default: return false;
        }
    }

    bool canNavigateRight() const { return currentProcessorSelector->getCurrentViewOffset() + NUM_COLUMNS < currentProcessorSelector->getTotalNumberOfTreeItems(); }
    bool canNavigateDown() const { return currentProcessorSelector->findSelectedLabel() != nullptr; }
    bool canNavigateLeft() const { return currentProcessorSelector->getCurrentViewOffset() > 0; }
    bool canNavigateUp() const { return bottomProcessorSelector.get() == currentProcessorSelector || currentProcessorSelector->currentTree->parent != nullptr; }

    KnownPluginList::PluginTree rootTree;
    std::unique_ptr<ProcessorSelectorRow> topProcessorSelector;
    std::unique_ptr<ProcessorSelectorRow> bottomProcessorSelector;
    
    ProcessorSelectorRow* currentProcessorSelector{};
};
