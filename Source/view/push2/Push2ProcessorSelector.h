#pragma once

#include <utility>
#include "JuceHeader.h"
#include "Push2ComponentBase.h"

class Push2ProcessorSelector : public Push2ComponentBase {
    class ProcessorSelectorRow : public Component {
    public:
        explicit ProcessorSelectorRow(Project &project) : project(project) {
            for (int i = 0; i < NUM_ITEMS_PER_ROW; i++) {
                auto *label = new Label();
                addChildComponent(label);
                label->setJustificationType(Justification::centred);
                labels.add(label);
            }
        }

        const PluginDescription* selectProcessor(int index) {
            if (currentTree == nullptr)
                return nullptr;

            if (index + currentViewOffsetIndex < currentTree->subFolders.size()) {
                setCurrentTree(currentTree->subFolders.getUnchecked(index + currentViewOffsetIndex));
                return nullptr;
            } else {
                index -= currentTree->subFolders.size();
                if (index + currentViewOffsetIndex < currentTree->plugins.size() && labels[index]->isEnabled()) {
                    return currentTree->plugins.getUnchecked(index + currentViewOffsetIndex);
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
            currentViewOffsetIndex = 0;
            updateLabels(project.selectedTrackHasMixerChannel());
        }

        void setSelected(bool selected) {
            this->selected = selected;
            repaint();
        }

        Label* findSelectedLabel() const {
            if (currentTree == nullptr)
                return nullptr;

            for (int i = 0; i < jmin(getTotalNumberOfTreeItems() - currentViewOffsetIndex, NUM_ITEMS_PER_ROW); i++) {
                if (i + currentViewOffsetIndex < currentTree->subFolders.size()) {
                    if (currentTree->subFolders.getUnchecked(i + currentViewOffsetIndex)->selected)
                        return labels[i];
                }
            }
            if (currentViewOffsetIndex < currentTree->subFolders.size()) {
                return labels.getUnchecked(0); // default to first subfolder in view
            }
            return nullptr;
        }

        void arrowPressed(Direction direction) {
            if (direction == Direction::left) {
                currentViewOffsetIndex = jmax(0, currentViewOffsetIndex - NUM_ITEMS_PER_ROW);
            } else if (direction == Direction::right && currentTree != nullptr) {
                currentViewOffsetIndex = jmin(currentViewOffsetIndex + NUM_ITEMS_PER_ROW, getTotalNumberOfTreeItems() - getTotalNumberOfTreeItems() % NUM_ITEMS_PER_ROW);
            }
            updateLabels(project.selectedTrackHasMixerChannel());
        }

        int getTotalNumberOfTreeItems() const {
            return currentTree != nullptr ? currentTree->subFolders.size() + currentTree->plugins.size() : 0;
        }

        void paint(Graphics& g) override {
            g.fillAll(selected ? Colours::white.withAlpha(0.07f) : Colours::transparentBlack);
        }

        void resized() override {
            auto r = getLocalBounds();
            for (auto *label : labels) {
                label->setBounds(r.removeFromLeft(getWidth() / NUM_ITEMS_PER_ROW));
            }
        }

        int currentViewOffsetIndex { 0 };
        KnownPluginList::PluginTree* currentTree{};

        OwnedArray<Label> labels;
    private:
        bool selected { false };
        Project &project;

        void updateLabels(bool trackHasMixerAlready) {
            for (int i = 0; i < labels.size(); i++) {
                Label *label = labels.getUnchecked(i);
                label->setColour(Label::ColourIds::backgroundColourId, Colours::transparentBlack);
                label->setVisible(false);
            }

            if (currentTree == nullptr)
                return;

            for (int i = 0; i < jmin(getTotalNumberOfTreeItems() - currentViewOffsetIndex, NUM_ITEMS_PER_ROW); i++) {
                Label *label = labels.getUnchecked(i);
                labels[i]->setVisible(true);
                if (i + currentViewOffsetIndex < currentTree->subFolders.size()) {
                    KnownPluginList::PluginTree *subFolder = currentTree->subFolders.getUnchecked(i + currentViewOffsetIndex);
                    label->setText(subFolder->folder, dontSendNotification);
                } else if (i + currentViewOffsetIndex < getTotalNumberOfTreeItems()) {
                    const PluginDescription *plugin = currentTree->plugins.getUnchecked(i + currentViewOffsetIndex - currentTree->subFolders.size());
                    labels[i]->setText(plugin->name, dontSendNotification);
                    labels[i]->setEnabled(!trackHasMixerAlready || plugin->name != MixerChannelProcessor::name());
                }
            }
            if (auto *selectedLabel = findSelectedLabel()) {
                selectedLabel->setColour(Label::ColourIds::backgroundColourId, Colours::white.withAlpha(0.08f));
            }
        }
    };

public:
    Push2ProcessorSelector(Project &project, Push2MidiCommunicator &push2MidiCommunicator)
            : Push2ComponentBase(project, push2MidiCommunicator) {
        topProcessorSelector = std::make_unique<ProcessorSelectorRow>(project);
        bottomProcessorSelector = std::make_unique<ProcessorSelectorRow>(project);
        addChildComponent(topProcessorSelector.get());
        addChildComponent(bottomProcessorSelector.get());
    }

    void setVisible(bool visible) override {
        Component::setVisible(visible);
        topProcessorSelector->setVisible(visible);
        bottomProcessorSelector->setVisible(visible);
        
        if (visible) {
            rootTree = KnownPluginList::PluginTree();
            
            KnownPluginList::PluginTree *internalPluginTree = project.getUserCreatablePluginListInternal().createTree(project.getPluginSortMethod());
            internalPluginTree->folder = "Internal";
            KnownPluginList::PluginTree *externalPluginTree = project.getPluginListExternal().createTree(project.getPluginSortMethod());
            externalPluginTree->folder = "External";
            internalPluginTree->parent = &rootTree;
            externalPluginTree->parent = &rootTree;

            rootTree.subFolders.add(internalPluginTree);
            rootTree.subFolders.add(externalPluginTree);

            setCurrentTree(topProcessorSelector.get(), &rootTree);
            selectProcessorRow(topProcessorSelector.get());
            updateEnabledPush2Arrows();
        } else {
            setCurrentTree(topProcessorSelector.get(), nullptr);
            setCurrentTree(bottomProcessorSelector.get(), nullptr);
            updateEnabledPush2Arrows();
        }
    }

    const PluginDescription* selectTopProcessor(int index) {
        KnownPluginList::PluginTree *previousTree = topProcessorSelector->currentTree;
        const PluginDescription *description = topProcessorSelector->selectProcessor(index);
        if (description != nullptr)
            return description;

        if (topProcessorSelector->currentTree != previousTree) {
            setCurrentTree(bottomProcessorSelector.get(), topProcessorSelector->currentTree);
            setCurrentTree(topProcessorSelector.get(), previousTree);
            selectProcessorRow(bottomProcessorSelector.get());
            updateEnabledPush2Arrows();
        }
        return nullptr;
    }

    const PluginDescription* selectBottomProcessor(int index) {
        KnownPluginList::PluginTree *previousTree = bottomProcessorSelector->currentTree;
        const PluginDescription *description = bottomProcessorSelector->selectProcessor(index);
        if (description != nullptr)
            return description;

        if (bottomProcessorSelector->currentTree != previousTree) {
            setCurrentTree(topProcessorSelector.get(), previousTree);
            updateEnabledPush2Arrows();
        }
        return nullptr;
    }

    void aboveScreenButtonPressed(int buttonIndex) override {
        if (const auto* selectedProcessor = selectTopProcessor(buttonIndex)) {
            project.createAndAddProcessor(*selectedProcessor);
        }
    }

    void belowScreenButtonPressed(int buttonIndex) override {
        if (const auto* selectedProcessor = selectBottomProcessor(buttonIndex)) {
            project.createAndAddProcessor(*selectedProcessor);
        }
    }

    void arrowPressed(Direction direction) override {
        if (!canNavigateInDirection(direction))
            return;
        if (currentProcessorSelector != nullptr && (direction == Direction::left || direction == Direction::right)) {
            currentProcessorSelector->arrowPressed(direction);
            updateEnabledPush2Arrows();
        } else if (direction == Direction::down) {
            if (auto* selectedLabel = currentProcessorSelector->findSelectedLabel()) {
                int selectedIndex = currentProcessorSelector->labels.indexOf(selectedLabel);
                if (currentProcessorSelector == topProcessorSelector.get()) {
                    selectTopProcessor(selectedIndex);
                } else if (currentProcessorSelector == bottomProcessorSelector.get()) {
                    selectBottomProcessor(selectedIndex);
                }
            }
        } else if (direction == Direction::up) {
            if (currentProcessorSelector == bottomProcessorSelector.get()) {
                selectProcessorRow(topProcessorSelector.get());
                updateEnabledPush2Arrows();
            } else if (currentProcessorSelector->currentTree->parent != nullptr) {
                setCurrentTree(bottomProcessorSelector.get(), topProcessorSelector->currentTree);
                setCurrentTree(topProcessorSelector.get(), currentProcessorSelector->currentTree->parent);
                updateEnabledPush2Arrows();
            }
        }
    }

    void resized() override {
        auto r = getLocalBounds();
        topProcessorSelector->setBounds(r.removeFromTop(30));
        bottomProcessorSelector->setBounds(r.removeFromBottom(30));
    }

private:
    static const int NUM_ITEMS_PER_ROW = 8;

    void setCurrentTree(ProcessorSelectorRow *processorSelectorRow, KnownPluginList::PluginTree* tree) {
        for (int i = 0; i < NUM_ITEMS_PER_ROW; i++) {
            if (topProcessorSelector.get() == processorSelectorRow)
                push2MidiCommunicator.setAboveScreenButtonEnabled(i, false);
            else if (bottomProcessorSelector.get() == processorSelectorRow)
                push2MidiCommunicator.setBelowScreenButtonEnabled(i, false);
        }

        processorSelectorRow->setCurrentTree(tree);
        for (int i = 0; i < processorSelectorRow->labels.size(); i++){
            if (processorSelectorRow->labels.getUnchecked(i)->isVisible()) {
                if (topProcessorSelector.get() == processorSelectorRow)
                    push2MidiCommunicator.setAboveScreenButtonEnabled(i, true);
                else if (bottomProcessorSelector.get() == processorSelectorRow)
                    push2MidiCommunicator.setBelowScreenButtonEnabled(i, true);
            }
        }
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
        if (currentProcessorSelector == nullptr) {
            push2MidiCommunicator.setAllArrowButtonsEnabled(false);
        } else {
            for (Direction direction : Push2MidiCommunicator::directions) {
                push2MidiCommunicator.setArrowButtonEnabled(direction, canNavigateInDirection(direction));
            }
        }
    }

    bool canNavigateInDirection(Direction direction) {
        switch (direction) {
            case Direction::right: return canNavigateRight();
            case Direction::down: return canNavigateDown();
            case Direction::left: return canNavigateLeft();
            case Direction::up: return canNavigateUp();
        }
    }

    bool canNavigateRight() const { return currentProcessorSelector->currentViewOffsetIndex + NUM_ITEMS_PER_ROW < currentProcessorSelector->getTotalNumberOfTreeItems(); }
    bool canNavigateDown() const { return currentProcessorSelector->findSelectedLabel() != nullptr; }
    bool canNavigateLeft() const { return currentProcessorSelector->currentViewOffsetIndex > 0; }
    bool canNavigateUp() const { return bottomProcessorSelector.get() == currentProcessorSelector || currentProcessorSelector->currentTree->parent != nullptr; }

    KnownPluginList::PluginTree rootTree;
    std::unique_ptr<ProcessorSelectorRow> topProcessorSelector;
    std::unique_ptr<ProcessorSelectorRow> bottomProcessorSelector;
    
    ProcessorSelectorRow* currentProcessorSelector{};
};
