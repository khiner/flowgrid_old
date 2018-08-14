#pragma once

#include <atomic>
#include <thread>
#include <cassert>
#include <iostream>

#include "libusb.h"

#include "JuceHeader.h"

class UsbCommunicator : private Timer {
public:
    UsbCommunicator(const uint16_t vendorId, const uint16_t productId):
            vendorId(vendorId), productId(productId), frameHeaderTransfer(nullptr), terminate(false) {
        if (!findDeviceHandleAndStartPolling())
            startTimer(1000); // check back every second
    }

    virtual ~UsbCommunicator() {
        // shutdown the polling thread
        terminate = true;
        if (pollThread.joinable()) {
            pollThread.join();
        }
    }

    inline bool isValid() { return handle != nullptr; }

    /*!
     *  Callback for when a transfer is finished and the next one needs to be
     *  initiated
     */
    void LIBUSB_CALL onTransferFinished(libusb_transfer *transfer) {
        if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
            switch (transfer->status) {
                case LIBUSB_TRANSFER_ERROR:
                    printf("transfer failed\n");
                    break;
                case LIBUSB_TRANSFER_TIMED_OUT:
                    printf("transfer timed out\n");
                    break;
                case LIBUSB_TRANSFER_CANCELLED:
                    printf("transfer was cancelled\n");
                    break;
                case LIBUSB_TRANSFER_STALL:
                    printf("endpoint stalled/control request not supported\n");
                    break;
                case LIBUSB_TRANSFER_NO_DEVICE:
                    printf("device was disconnected\n");
                    break;
                case LIBUSB_TRANSFER_OVERFLOW:
                    printf("device sent more data than requested\n");
                    break;
                default:
                    printf("snd transfer failed with status: %d\n", transfer->status);
                    break;
            }
        } else if (transfer->length != transfer->actual_length) {
            char errorMsg[256];
            sprintf(errorMsg, "only transferred %d of %d bytes\n", transfer->actual_length, transfer->length);
            throw std::runtime_error(errorMsg);
        } else if (transfer == frameHeaderTransfer) {
            if (!terminate.load()) {
                onFrameSendCompleted();
            }
        } else {
            if (!terminate.load()) {
                sendNextSlice(transfer);
            }
        }
    }

    /*!
     *  Continuously poll events from libusb, possibly treating any error reported
     */
    void pollUsbForEvents() {
        static struct timeval timeout_500ms = {0, 500000};
        int terminate_main_loop = 0;

        while (!terminate_main_loop && !terminate.load()) {
            if (libusb_handle_events_timeout_completed(nullptr, &timeout_500ms, &terminate_main_loop) < 0) {
                assert(false);
            }
        }
    }

protected:
    // Allocate a libusb_transfer mapped to a transfer buffer. It also sets
    // up the callback needed to communicate the transfer is done
    libusb_transfer* allocateAndPrepareTransferChunk(libusb_device_handle *handle, UsbCommunicator *instance,
                                                     unsigned char *buffer, int bufferSize,
                                                     const unsigned char endpoint) {
        // Allocate a transfer structure
        auto transfer = libusb_alloc_transfer(0);
        if (!transfer)
            return nullptr;

        libusb_fill_bulk_transfer(transfer, handle, endpoint, buffer, bufferSize,
                                  onTransferFinishedStatic, instance, 1000);
        return transfer;
    }

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
    libusb_device_handle *handle {};

private:
    void timerCallback() override {
        if (findDeviceHandleAndStartPolling())
            stopTimer();
    }

    bool findDeviceHandleAndStartPolling() {
        auto errorCode = libusb_init(nullptr);
        if (errorCode < 0)
            throw std::runtime_error("Failed to initialize usblib");

        libusb_device **devices;
        auto count = libusb_get_device_list(nullptr, &devices);
        if (count < 0)
            throw std::runtime_error("could not get usb device list");

        // Look for the one matching Push 2's descriptors
        libusb_device *device;
        handle = nullptr;

        for (int i = 0; (device = devices[i]) != nullptr; i++) {
            struct libusb_device_descriptor descriptor{};
            if ((errorCode = libusb_get_device_descriptor(device, &descriptor)) < 0) {
                std::cerr << "could not get usb device descriptor, error: " << errorCode << '\n';
                continue;
            }

            if (descriptor.bDeviceClass == LIBUSB_CLASS_PER_INTERFACE
                && descriptor.idVendor == vendorId
                && descriptor.idProduct == productId) {
                if ((errorCode = libusb_open(device, &handle)) < 0) {
                    std::cerr << "could not open device, error: " << errorCode << '\n';
                } else if ((errorCode = libusb_claim_interface(handle, 0)) < 0) {
                    std::cerr << "could not claim device with interface 0, error: " << errorCode << '\n';
                    libusb_close(handle);
                    handle = nullptr;
                } else {
                    break; // successfully opened
                }
            }
        }

        libusb_free_device_list(devices, 1);

        if (handle != nullptr) {
            pollThread = std::thread(&UsbCommunicator::pollUsbForEvents, this);
            return true;
        }

        return false;
    }

    // Callback received whenever a transfer has been completed.
    // We defer the processing to the communicator class
    LIBUSB_CALL static inline void onTransferFinishedStatic(libusb_transfer *transfer) {
        static_cast<UsbCommunicator *>(transfer->user_data)->onTransferFinished(transfer);
    }

    std::thread pollThread;
    std::atomic<bool> terminate;
};
