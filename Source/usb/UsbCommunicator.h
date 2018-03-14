#pragma once

#include <atomic>
#include <thread>

#include "../libusb/libusb.h"

// Forward declarations. This avoid having to include libusb.h from here
// which leads to declaration conflicts with juce
class libusb_transfer;

class libusb_device_handle;

class UsbCommunicator {
public:
    UsbCommunicator(const uint16_t vendorId, const uint16_t productId):
            vendorId(vendorId), productId(productId), frameHeaderTransfer(nullptr), terminate(false) {
        handle = findDeviceHandle();
        pollThread = std::thread(&UsbCommunicator::pollUsbForEvents, this);
    }

    virtual ~UsbCommunicator() {
        // shutdown the polling thread
        terminate = true;
        if (pollThread.joinable()) {
            pollThread.join();
        }
    }

    /*!
     *  Callback for when a transfer is finished and the next one needs to be
     *  initiated
     */
    void onTransferFinished(libusb_transfer *transfer);

    /*!
     *  Continuously poll events from libusb, possibly treating any error reported
     */
    void pollUsbForEvents();

protected:
    // Allocate a libusb_transfer mapped to a transfer buffer. It also sets
    // up the callback needed to communicate the transfer is done
    static libusb_transfer* allocateAndPrepareTransferChunk(
            libusb_device_handle *handle,
            UsbCommunicator *instance,
            unsigned char *buffer,
            int bufferSize,
            unsigned char endpoint);
    /*!
     *  Initiate the send process
     */
    virtual void startSending() = 0;

    /*!
     *  Send the next slice of data using the provided transfer struct
     */
    virtual void sendNextSlice(libusb_transfer *transfer) = 0;

    /*!
     *  Callback for when a full frame has been sent
     *  Note that there's no real need of doing double buffering since the
     *  display deals nicely with it already
     */
    virtual void onFrameSendCompleted()= 0;

    uint16_t vendorId;
    uint16_t productId;
    libusb_transfer *frameHeaderTransfer;
    libusb_device_handle *handle;

private:
    libusb_device_handle* findDeviceHandle();

    // Callback received whenever a transfer has been completed.
    // We defer the processing to the communicator class
    LIBUSB_CALL static inline void onTransferFinishedStatic(libusb_transfer *transfer) {
        static_cast<UsbCommunicator *>(transfer->user_data)->onTransferFinished(transfer);
    }

    std::thread pollThread;
    std::atomic<bool> terminate;
};
