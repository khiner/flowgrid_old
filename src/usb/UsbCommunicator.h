#pragma once

#include <atomic>
#include <thread>

#include <juce_events/juce_events.h>

#include "libusb.h"

class UsbCommunicator : private juce::Timer {
public:
    UsbCommunicator(uint16_t vendorId, uint16_t productId);

    ~UsbCommunicator() override;

    bool isValid() { return handle != nullptr; }

    /*!
     *  Callback for when a transfer is finished and the next one needs to be
     *  initiated
     */
    void LIBUSB_CALL onTransferFinished(libusb_transfer *transfer);

    /*!
     *  Continuously poll events from libusb, possibly treating any error reported
     */
    void pollUsbForEvents();

protected:
    // Allocate a libusb_transfer mapped to a transfer buffer. It also sets
    // up the callback needed to communicate the transfer is done
    static libusb_transfer *allocateAndPrepareTransferChunk(libusb_device_handle *handle, UsbCommunicator *instance,
                                                            unsigned char *buffer, int bufferSize, unsigned char endpoint);

    virtual void startSending() = 0;
    virtual void sendNextSlice(libusb_transfer *transfer) = 0;

    /*!
     *  Note that there's no real need of doing double buffering since the
     *  display deals nicely with it already
     */
    virtual void onFrameSendCompleted() = 0;

    uint16_t vendorId;
    uint16_t productId;
    libusb_transfer *frameHeaderTransfer;
    libusb_device_handle *handle{};
    std::atomic<bool> headerNeedsSending{true};

private:
    void timerCallback() override;

    bool findDeviceHandleAndStartPolling();

    // Callback received whenever a transfer has been completed.
    // We defer the processing to the communicator class
    LIBUSB_CALL static inline void onTransferFinishedStatic(libusb_transfer *transfer) {
        static_cast<UsbCommunicator *>(transfer->user_data)->onTransferFinished(transfer);
    }

    std::thread pollThread;
    std::atomic<bool> terminate;
};
