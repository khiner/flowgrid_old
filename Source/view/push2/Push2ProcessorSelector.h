#pragma once

#include <utility>
#include "JuceHeader.h"

class Push2ProcessorSelector : public Push2ComponentBase {
    class ProcessorSelectorRow : public Component {
    public:
        explicit ProcessorSelectorRow() {
            for (int i = 0; i < NUM_ITEMS_PER_ROW; i++) {
                auto *label = new Label();
                addChildComponent(label);
                label->setJustificationType(Justification::centred);
                labels.add(label);
            }
        }

        const PluginDescription* selectProcessor(const ValueTree &track, int index) {
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

        void setCurrentTree(KnownPluginList::PluginTree* tree, bool trackHasMixerAlready=false) {
            if (currentTree == tree)
                return;

            currentTree = tree;
            currentViewOffsetIndex = 0;
            updateLabels(trackHasMixerAlready);
        }

        void setSelected(bool selected) {
            this->selected = selected;
            repaint();
        }

        void arrowPressed(Direction direction) {
            if (direction == Direction::left) {
                currentViewOffsetIndex = jmax(0, currentViewOffsetIndex - NUM_ITEMS_PER_ROW);
            } else if (direction == Direction::right && currentTree != nullptr) {
                currentViewOffsetIndex = jmin(currentViewOffsetIndex + NUM_ITEMS_PER_ROW, getTotalNumberOfTreeItems() - getTotalNumberOfTreeItems() % NUM_ITEMS_PER_ROW);
            }
            updateLabels();
        }

        int getTotalNumberOfTreeItems() {
            if (currentTree == nullptr)
                return 0;

            return currentTree->subFolders.size() + currentTree->plugins.size();
        }

//        void paint(Graphics& g) override {
//            if (selected) {
//                g.fillAll(Colours::red);
//            }
//        }

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

        void updateLabels(bool trackHasMixerAlready=false) {
            for (int i = 0; i < labels.size(); i++) {
                labels.getUnchecked(i)->setVisible(false);
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
                    labels[i]->setEnabled(!trackHasMixerAlready || plugin->name != MixerChannelProcessor::getIdentifier());
                }
            }
        }
    };

public:
    Push2ProcessorSelector(Project &project, Push2MidiCommunicator &push2MidiCommunicator)
            : Push2ComponentBase(project, push2MidiCommunicator) {
        topProcessorSelector = std::make_unique<ProcessorSelectorRow>();
        bottomProcessorSelector = std::make_unique<ProcessorSelectorRow>();
        addChildComponent(topProcessorSelector.get());
        addChildComponent(bottomProcessorSelector.get());
    }

    void setVisible(bool visible) override {
        Component::setVisible(visible);
        topProcessorSelector->setVisible(visible);
        bottomProcessorSelector->setVisible(visible);
        
        if (visible) {
            rootTree = KnownPluginList::PluginTree();
            currentProcessorSelector = topProcessorSelector.get();
            
            KnownPluginList::PluginTree *internalPluginTree = project.getKnownPluginListInternal().createTree(project.getPluginSortMethod());
            internalPluginTree->folder = "Internal";
            KnownPluginList::PluginTree *externalPluginTree = project.getKnownPluginListExternal().createTree(project.getPluginSortMethod());
            externalPluginTree->folder = "External";

            rootTree.subFolders.add(internalPluginTree);
            rootTree.subFolders.add(externalPluginTree);

            setCurrentTree(currentProcessorSelector, &rootTree);
            updateEnabledPush2Arrows();
        } else {
            setCurrentTree(topProcessorSelector.get(), nullptr);
            setCurrentTree(bottomProcessorSelector.get(), nullptr);
            updateEnabledPush2Arrows();
        }
    }

    const PluginDescription* selectTopProcessor(const ValueTree &track, int index) {
        KnownPluginList::PluginTree *previousTree = topProcessorSelector->currentTree;
        const PluginDescription *description = topProcessorSelector->selectProcessor(track, index);
        if (description != nullptr)
            return description;

        if (topProcessorSelector->currentTree != previousTree) {
            setCurrentTree(bottomProcessorSelector.get(), topProcessorSelector->currentTree);
            setCurrentTree(topProcessorSelector.get(), previousTree);
        }
        return nullptr;
    }

    const PluginDescription* selectBottomProcessor(const ValueTree &track, int index) {
        KnownPluginList::PluginTree *previousTree = bottomProcessorSelector->currentTree;
        const PluginDescription *description = bottomProcessorSelector->selectProcessor(track, index);
        if (description != nullptr)
            return description;

        if (bottomProcessorSelector->currentTree != previousTree) {
            setCurrentTree(topProcessorSelector.get(), previousTree);
        }
        return nullptr;
    }

    void setCurrentTree(ProcessorSelectorRow *processorSelectorRow, KnownPluginList::PluginTree* tree) {
        for (int i = 0; i < NUM_ITEMS_PER_ROW; i++) {
            if (topProcessorSelector.get() == processorSelectorRow)
                push2MidiCommunicator.setAboveScreenButtonEnabled(i, false);
            else if (bottomProcessorSelector.get() == processorSelectorRow)
                push2MidiCommunicator.setBelowScreenButtonEnabled(i, false);
        }

        bool trackHasMixerAlready = project.getMixerChannelProcessorForTrack(project.getSelectedTrack()).isValid();
        processorSelectorRow->setCurrentTree(tree, trackHasMixerAlready);
        for (int i = 0; i < processorSelectorRow->labels.size(); i++){
            if (processorSelectorRow->labels.getUnchecked(i)->isVisible()) {
                if (topProcessorSelector.get() == processorSelectorRow)
                    push2MidiCommunicator.setAboveScreenButtonEnabled(i, true);
                else if (bottomProcessorSelector.get() == processorSelectorRow)
                    push2MidiCommunicator.setBelowScreenButtonEnabled(i, true);
            }
        }
    }

    void arrowPressed(Direction direction) {
        if ((direction == Direction::left || direction == Direction::right) && currentProcessorSelector != nullptr) {
            currentProcessorSelector->arrowPressed(direction);
        } else { // up/down
            if (direction == Direction::down && topProcessorSelector.get() == currentProcessorSelector) {
                selectProcessorRow(bottomProcessorSelector.get());
            } else if (direction == Direction::up && bottomProcessorSelector.get() == currentProcessorSelector) {
                selectProcessorRow(topProcessorSelector.get());
            }
        } 
        updateEnabledPush2Arrows();
    }

    void resized() override {
        auto r = getLocalBounds();
        topProcessorSelector->setBounds(r.removeFromTop(30));
        bottomProcessorSelector->setBounds(r.removeFromBottom(30));
    }

private:
    static const int NUM_ITEMS_PER_ROW = 8;

    void selectProcessorRow(ProcessorSelectorRow* processorSelectorRow) {
        currentProcessorSelector->setSelected(false);
        currentProcessorSelector = processorSelectorRow;
        currentProcessorSelector->setSelected(true);
    }

    void updateEnabledPush2Arrows() {
        push2MidiCommunicator.setAllArrowButtonsEnabled(false);

        if (currentProcessorSelector == nullptr)
            return;

        if (currentProcessorSelector->currentViewOffsetIndex > 0)
            push2MidiCommunicator.setArrowButtonEnabled(Direction::left, true);
        if (currentProcessorSelector->currentViewOffsetIndex + NUM_ITEMS_PER_ROW < currentProcessorSelector->getTotalNumberOfTreeItems())
            push2MidiCommunicator.setArrowButtonEnabled(Direction::right, true);
        if (topProcessorSelector.get() == currentProcessorSelector)
            push2MidiCommunicator.setArrowButtonEnabled(Direction::down, true);
        if (bottomProcessorSelector.get() == currentProcessorSelector)
            push2MidiCommunicator.setArrowButtonEnabled(Direction::up, true);
    }

    KnownPluginList::PluginTree rootTree;
    std::unique_ptr<ProcessorSelectorRow> topProcessorSelector;
    std::unique_ptr<ProcessorSelectorRow> bottomProcessorSelector;
    
    ProcessorSelectorRow* currentProcessorSelector{};
};
