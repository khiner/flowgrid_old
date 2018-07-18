#pragma once

#include "JuceHeader.h"

class Push2ProcessorSelector : public Push2ComponentBase {
public:
    explicit Push2ProcessorSelector(Project &project, Push2MidiCommunicator &push2MidiCommunicator)
            : Push2ComponentBase(project, push2MidiCommunicator) {
        for (int i = 0; i < NUM_ITEMS_PER_ROW; i++) {
            auto *label = new Label();
            addAndMakeVisible(label);
            label->setJustificationType(Justification::centred);
            labels.add(label);
        }
    }

    void setVisible(bool visible) override {
        Component::setVisible(visible);
        if (visible) {
            rootTree = KnownPluginList::PluginTree();
            KnownPluginList::PluginTree *internalPluginTree = project.getKnownPluginListInternal().createTree(project.getPluginSortMethod());
            internalPluginTree->folder = "Internal";
            KnownPluginList::PluginTree *externalPluginTree = project.getKnownPluginListExternal().createTree(project.getPluginSortMethod());
            externalPluginTree->folder = "External";

            rootTree.subFolders.add(internalPluginTree);
            rootTree.subFolders.add(externalPluginTree);
            setCurrentTree(&rootTree);
        } else {
            setCurrentTree(nullptr);
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

    void arrowPressed(Direction direction) {
        if (direction == Direction::left) {
            currentViewOffsetIndex = jmax(0, currentViewOffsetIndex - NUM_ITEMS_PER_ROW);
        } else if (direction == Direction::right && currentTree != nullptr) {
            currentViewOffsetIndex = jmin(currentViewOffsetIndex + NUM_ITEMS_PER_ROW, getTotalNumberOfTreeItems() - getTotalNumberOfTreeItems() % NUM_ITEMS_PER_ROW);
        }
        updateEnabledPush2Arrows();
        updateLabels();
    }

    void resized() override {
        auto r = getBounds().withHeight(getHeight() / NUM_ITEMS_PER_ROW);
        for (auto *label : labels) {
            label->setBounds(r.removeFromLeft(getWidth() / NUM_ITEMS_PER_ROW));
        }
    }

private:
    const int NUM_ITEMS_PER_ROW = 8;
    
    KnownPluginList::PluginTree rootTree;
    KnownPluginList::PluginTree* currentTree{};
    OwnedArray<Label> labels;
    
    int currentViewOffsetIndex { 0 };
    
    void setCurrentTree(KnownPluginList::PluginTree* tree) {
        if (currentTree == tree)
            return;

        currentTree = tree;
        currentViewOffsetIndex = 0;
        updateEnabledPush2Arrows();
        updateLabels();
    }

    void updateLabels() {
        bool disableMixerChannel = project.getMixerChannelProcessorForTrack(project.getSelectedTrack()).isValid();
        for (int i = 0; i < labels.size(); i++) {
            labels.getUnchecked(i)->setVisible(false);
            push2MidiCommunicator.setAboveScreenButtonEnabled(i, false);
        }

        if (currentTree == nullptr)
            return;

        for (int i = 0; i < jmin(getTotalNumberOfTreeItems() - currentViewOffsetIndex, NUM_ITEMS_PER_ROW); i++) {
            Label *label = labels.getUnchecked(i);
            labels[i]->setVisible(true);
            push2MidiCommunicator.setAboveScreenButtonEnabled(i, true);
            if (i + currentViewOffsetIndex < currentTree->subFolders.size()) {
                KnownPluginList::PluginTree *subFolder = currentTree->subFolders.getUnchecked(i + currentViewOffsetIndex);
                label->setText(subFolder->folder, dontSendNotification);
            } else if (i + currentViewOffsetIndex < getTotalNumberOfTreeItems()) {
                const PluginDescription *plugin = currentTree->plugins.getUnchecked(i + currentViewOffsetIndex - currentTree->subFolders.size());
                labels[i]->setText(plugin->name, dontSendNotification);
                labels[i]->setEnabled(!disableMixerChannel || plugin->name != MixerChannelProcessor::getIdentifier());
            }
        }
    }

    void updateEnabledPush2Arrows() {
        push2MidiCommunicator.setAllArrowButtonsEnabled(false);
        
        if (currentTree == nullptr)
            return;

        if (currentViewOffsetIndex > 0)
            push2MidiCommunicator.setArrowButtonEnabled(Direction::left, true);
        if (currentViewOffsetIndex + NUM_ITEMS_PER_ROW < getTotalNumberOfTreeItems())
            push2MidiCommunicator.setArrowButtonEnabled(Direction::right, true);
    }

    int getTotalNumberOfTreeItems() {
        if (currentTree == nullptr)
            return 0;

        return currentTree->subFolders.size() + currentTree->plugins.size();
    }
};
