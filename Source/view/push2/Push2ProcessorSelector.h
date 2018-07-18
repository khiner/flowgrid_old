#pragma once

#include "JuceHeader.h"

class Push2ProcessorSelector : public Push2ComponentBase {
public:
    explicit Push2ProcessorSelector(Project &project, Push2MidiCommunicator &push2MidiCommunicator)
            : Push2ComponentBase(project, push2MidiCommunicator) {
        for (int i = 0; i < 8; i++) {
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

        push2MidiCommunicator.setAboveScreenButtonColor(index, 127);

        if (index < currentTree->subFolders.size()) {
            setCurrentTree(currentTree->subFolders[index]);
            return nullptr;
        } else {
            index -= currentTree->subFolders.size();
            if (index < currentTree->plugins.size() && labels[index]->isEnabled()) {
                return currentTree->plugins[index];
            }
        }

        return nullptr;
    }

    void resized() override {
        auto r = getBounds().withHeight(getHeight() / 8);
        for (auto *label : labels) {
            label->setBounds(r.removeFromLeft(getWidth() / 8));
        }
    }

private:
    KnownPluginList::PluginTree rootTree;
    KnownPluginList::PluginTree* currentTree{};
    OwnedArray<Label> labels;

    void setCurrentTree(KnownPluginList::PluginTree* tree) {
        currentTree = tree;

        bool disableMixerChannel = project.getMixerChannelProcessorForTrack(project.getSelectedTrack()).isValid();
        for (auto* label : labels)
            label->setVisible(false);

        if (currentTree == nullptr)
            return;

        int i = 0;
        for (auto *subFolder : currentTree->subFolders) {
            if (i < labels.size()) {
                labels[i]->setText(subFolder->folder, dontSendNotification);
                labels[i]->setVisible(true);
                i++;
            }
        }
        for (auto *plugin : currentTree->plugins) {
            if (i < labels.size()) {
                labels[i]->setText(plugin->name, dontSendNotification);
                labels[i]->setVisible(true);
                labels[i]->setEnabled(!disableMixerChannel || plugin->name != MixerChannelProcessor::getIdentifier());
                i++;
            }
        }
    }
};
