#include "../libusb/libusb.h"
#include "UsbCommunicator.h"

#include <cassert>
#include <iostream>

using namespace std;

// Allocate a libusb_transfer mapped to a transfer buffer. It also sets
// up the callback needed to communicate the transfer is done
libusb_transfer* UsbCommunicator::allocateAndPrepareTransferChunk(
        libusb_device_handle *handle,
        UsbCommunicator *instance,
        unsigned char *buffer,
        int bufferSize,
        const unsigned char endpoint) {
    // Allocate a transfer structure
    auto transfer = libusb_alloc_transfer(0);
    if (!transfer) {
        return nullptr;
    }

    libusb_fill_bulk_transfer(transfer, handle, endpoint,
                              buffer, bufferSize,
                              onTransferFinishedStatic, instance, 1000);
    return transfer;
}

// Uses libusb to create a device handle for the Push 2 display
libusb_device_handle* UsbCommunicator::findDeviceHandle() {
    auto errorCode = libusb_init(nullptr);
    if (errorCode < 0) {
        throw runtime_error("Failed to initialize usblib");
    }

    libusb_set_debug(nullptr, LIBUSB_LOG_LEVEL_ERROR);

    // Get a list of connected devices
    libusb_device **devices;
    auto count = libusb_get_device_list(nullptr, &devices);
    if (count < 0) {
        throw runtime_error("could not get usb device list");
    }

    // Look for the one matching Push 2's descriptors
    libusb_device *device;
    libusb_device_handle *device_handle = nullptr;

    // set message in case we get to the end of the list w/o finding a device
    char errorMsg[256];
    sprintf(errorMsg, "display device not found\n");

    for (int i = 0; (device = devices[i]) != nullptr; i++) {
        struct libusb_device_descriptor descriptor{};
        if ((errorCode = libusb_get_device_descriptor(device, &descriptor)) < 0) {
            sprintf(errorMsg, "could not get usb device descriptor, error: %d", errorCode);
            continue;
        }

        if (descriptor.bDeviceClass == LIBUSB_CLASS_PER_INTERFACE
            && descriptor.idVendor == vendorId
            && descriptor.idProduct == productId) {
            if ((errorCode = libusb_open(device, &device_handle)) < 0) {
                sprintf(errorMsg, "could not open device, error: %d", errorCode);
            } else if ((errorCode = libusb_claim_interface(device_handle, 0)) < 0) {
                sprintf(errorMsg, "could not claim device with interface 0, error: %d", errorCode);
                libusb_close(device_handle);
                device_handle = nullptr;
            } else {
                break; // successfully opened
            }
        }
    }

    libusb_free_device_list(devices, 1);

    if (!device_handle) {
        std::cerr << errorMsg << '\n';
    }
    return device_handle;
}

void LIBUSB_CALL UsbCommunicator::onTransferFinished(libusb_transfer *transfer) {
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        assert(false);
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
        throw runtime_error(errorMsg);
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

void UsbCommunicator::pollUsbForEvents() {
    static struct timeval timeout_500ms = {0, 500000};
    int terminate_main_loop = 0;

    while (!terminate_main_loop && !terminate.load()) {
        if (libusb_handle_events_timeout_completed(nullptr, &timeout_500ms, &terminate_main_loop) < 0) {
            assert(false);
        }
    }
}
