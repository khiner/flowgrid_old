#pragma once


#include "MainParameters.h"

class AppState {
public:
    MainParameters& getMainParameters() {
        return mainParameters;
    };

private:
    MainParameters mainParameters;
};


