#pragma once

#include "Push2Display.h"
#include "../JuceLibraryCode/JuceHeader.h"

namespace ableton {
    /*!
     *  Implements a bridge between juce::Graphics and push2 display format.
     */

    class Push2DisplayBridge {
    public:

        Push2DisplayBridge();

        /*!
         * Initialises the bridge
         *
         *  \return the result of the initialisation process
         */

        NBase::Result Init(ableton::Push2Display &display);

        /*!
         * Access a reference to the juce::Graphics of the bridge
         * the returned object can be used with juce's drawing primitives
         */

        juce::Graphics &GetGraphic();

        /*!
         * Tells the bridge the drawing is done and the bitmap can be sent to
         * the push display
         */

        void Flip();

    private:
        ableton::Push2Display *push2Display_;    /*< The push display the bridge works on */

        juce::Image image_;        /*< The image used to render the pixel data */
        juce::Graphics graphics_;  /*< The graphics associated to the image */
    };
}
