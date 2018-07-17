#pragma once

#include "JuceHeader.h"

class Push2ProcessorSelector : public Component {
public:
    explicit Push2ProcessorSelector(Project &project) : project(project) {
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
            rootTree = std::unique_ptr<KnownPluginList::PluginTree>(
                    project.getKnownPluginList().createTree(project.getPluginSortMethod()));
            setCurrentTree(rootTree.get());
        } else {
            setCurrentTree(nullptr);
        }
    }


    const PluginDescription* selectProcessor(const ValueTree &track, int index) {
        if (currentTree == nullptr)
            return nullptr;

        if (index < currentTree->subFolders.size()) {
            setCurrentTree(currentTree->subFolders[index]);
            return nullptr;
        } else {
            index -= currentTree->subFolders.size();
            if (index < currentTree->plugins.size()) {
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
    Project& project;
    std::unique_ptr<KnownPluginList::PluginTree> rootTree;
    KnownPluginList::PluginTree* currentTree{};
    OwnedArray<Label> labels;

    void setCurrentTree(KnownPluginList::PluginTree* tree) {
        currentTree = tree;

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
                i++;
            }
        }
    }
};
