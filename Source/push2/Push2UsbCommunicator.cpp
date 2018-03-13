#include "Push2UsbCommunicator.h"

#ifdef _WIN32
// see following link for a discussion of the
// warning suppression:
// http://sourceforge.net/mailarchive/forum.php?
// thread_name=50F6011C.2020000%40akeo.ie&forum_name=libusbx-devel

// Disable: warning C4200: nonstandard extension used:
// zero-sized array in struct/union
#pragma warning(disable:4200)
#endif

using namespace std;

void Push2UsbCommunicator::startSending() {
    currentLine = 0;

    // transfer struct for the frame header
    static unsigned char frameHeader[16] = {
            0xFF, 0xCC, 0xAA, 0x88,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
    };

    static const unsigned char push2BulkEPOut = 0x01;
    frameHeaderTransfer =
            allocateAndPrepareTransferChunk(handle, this, frameHeader, sizeof(frameHeader), push2BulkEPOut);

    for (int i = 0; i < SEND_BUFFER_COUNT; i++) {
        unsigned char *buffer = (sendBuffers + i * SEND_BUFFER_SIZE);

        // Allocates a transfer struct for the send buffers
        libusb_transfer *transfer =
                allocateAndPrepareTransferChunk(handle, this, buffer, SEND_BUFFER_SIZE, push2BulkEPOut);

        // Start a request for this buffer
        sendNextSlice(transfer);
    }
}

void Push2UsbCommunicator::sendNextSlice(libusb_transfer *transfer) {
    // Start of a new frame, so send header first
    if (currentLine == 0) {
        if (libusb_submit_transfer(frameHeaderTransfer) < 0) {
            throw runtime_error("could not submit frame header transfer");
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

    // Send it
    if (libusb_submit_transfer(transfer) < 0) {
        throw runtime_error("could not submit display data transfer");
    }

    // Update slice position
    currentLine += LINE_COUNT_PER_SEND_BUFFER;

    if (currentLine >= NUM_LINES) {
        currentLine = 0;
    }
}

void Push2UsbCommunicator::onFrameSendCompleted() {
    // Insert code here if you want anything to happen after each frame
}
