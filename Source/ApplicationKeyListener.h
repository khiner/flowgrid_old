#pragma once

#include "JuceHeader.h"
#include "Project.h"

class ApplicationKeyListener : public KeyListener {
public:
    ApplicationKeyListener(Project &project, UndoManager &undoManager)
            : project(project), undoManager(undoManager) {}

    bool keyPressed(const KeyPress &key, Component* originatingComponent) override {
        if (key == KeyPress::deleteKey || key == KeyPress::backspaceKey) {
            project.deleteSelectedItems();
            return true;
        }

        return false;
    }
private:
    Project &project;
    UndoManager &undoManager;
};