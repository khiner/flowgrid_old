#pragma once

#include <thread>

#include <usb/UsbCommunicator.h>
#include "Push2Display.h"

// Forward declarations. This avoid having to include libusb.h from here
// which leads to declaration conflicts with juce
class libusb_transfer;

class libusb_device_handle;

/*!
 *  This class manages the communication with the Push 2 display over usb.
 *
 */
class Push2UsbCommunicator : public UsbCommunicator {
public:
    Push2UsbCommunicator(const uint16_t vendorId, const uint16_t productId) :
            UsbCommunicator(vendorId, productId), currentLine(0) {}

    inline void setPixelValue(int x, int y, Push2Display::pixel_t value) {
        pixels[y * LINE_WIDTH + x] = value;
    }

    inline void onFrameFillCompleted() {
        if (frameHeaderTransfer == nullptr) {
            startSending();
        }
    }
protected:
    /*!
     *  Initiate the send process
     */
    void startSending() override;

    /*!
     *  Send the next slice of data using the provided transfer struct
     */
    void sendNextSlice(libusb_transfer *transfer) override;

    /*!
     *  Callback for when a full frame has been sent
     *  Note that there's no real need of doing double buffering since the
     *  display deals nicely with it already
     */
    void onFrameSendCompleted() override;

private:
    static const int LINE_WIDTH = 1024;
    static const int NUM_LINES = Push2Display::HEIGHT;

    // The display frame size is 960*160*2=300k, but we use 64 extra filler
    // pixels per line so that we get exactly 2048 bytes per line. The purpose
    // is that the device receives exactly 4 buffers of 512 bytes each per line,
    // so that the line boundary (which is where we save to SDRAM) does not fall
    // into the middle of a received buffer. Therefore we actually send
    // 1024*160*2=320k bytes per frame.
    static const int LINE_SIZE_BYTES = LINE_WIDTH * 2;
    static const int LINE_COUNT_PER_SEND_BUFFER = 8;

    // The data sent to the display is sliced into chunks of LINE_COUNT_PER_SEND_BUFFER
    // and we use SEND_BUFFER_COUNT buffers to communicate so we can prepare the next
    // request without having to wait for the current one to be finished
    // The sent buffer size (SEND_BUFFER_SIZE) must a multiple of these 2k per line,
    // and is selected for optimal performance.
    static const int SEND_BUFFER_COUNT = 3;
    static const int SEND_BUFFER_SIZE = LINE_COUNT_PER_SEND_BUFFER * LINE_SIZE_BYTES; // buffer length in bytes

    Push2Display::pixel_t pixels[Push2UsbCommunicator::LINE_WIDTH * Push2UsbCommunicator::NUM_LINES]{};
    unsigned char sendBuffers[SEND_BUFFER_COUNT * SEND_BUFFER_SIZE]{};
    uint8_t currentLine;
};

