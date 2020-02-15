#pragma once

#include <thread>

#include <usb/UsbCommunicator.h>
#include "Push2Display.h"

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
        if (frameHeaderTransfer == nullptr || headerNeedsSending.load()) {
            startSending();
        }
    }

protected:
    /*!
     *  Initiate the send process
     */
    void startSending() override {
        currentLine = 0;

        // transfer struct for the frame header
        static unsigned char frameHeader[16] = {
                0xFF, 0xCC, 0xAA, 0x88,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00
        };

        static const unsigned char push2BulkEPOut = 0x01;
        frameHeaderTransfer = allocateAndPrepareTransferChunk(handle, this, frameHeader, sizeof(frameHeader), push2BulkEPOut);

        for (int i = 0; i < SEND_BUFFER_COUNT; i++) {
            unsigned char *buffer = (sendBuffers + i * SEND_BUFFER_SIZE);

            // Allocates a transfer struct for the send buffers
            auto *transfer = allocateAndPrepareTransferChunk(handle, this, buffer, SEND_BUFFER_SIZE, push2BulkEPOut);

            // Start a request for this buffer
            sendNextSlice(transfer);
        }
    }

    /*!
     *  Send the next slice of data using the provided transfer struct
     */
    void sendNextSlice(libusb_transfer *transfer) override {
        // Start of a new frame, so send header first
        if (currentLine == 0) {
            if (libusb_submit_transfer(frameHeaderTransfer) < 0) {
                std::cerr << "could not submit frame header transfer" << '\n';
                headerNeedsSending.store(true);
                return;
            } else {
                headerNeedsSending.store(false);
            }
        }

        // Copy the next slice of the source data (represented by currentLine)
        // to the transfer buffer
        unsigned char *dst = transfer->buffer;

        const unsigned char *src = (const unsigned char *) pixels + LINE_SIZE_BYTES * currentLine;
        unsigned char *end = dst + SEND_BUFFER_SIZE;

        while (dst < end) {
            *dst++ = *src++;
        }

        if (libusb_submit_transfer(transfer) < 0) {
            std::cerr << "could not submit display data transfer" << '\n';
            return;
        }

        // Update slice position
        currentLine += LINE_COUNT_PER_SEND_BUFFER;

        if (currentLine >= NUM_LINES) {
            currentLine = 0;
        }
    }


    /*!
     *  Callback for when a full frame has been sent
     *  Note that there's no real need of doing double buffering since the
     *  display deals nicely with it already
     */
    void onFrameSendCompleted() override {}

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

