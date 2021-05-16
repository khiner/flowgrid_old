#include "UsbCommunicator.h"

UsbCommunicator::UsbCommunicator(const uint16_t vendorId, const uint16_t productId) :
        vendorId(vendorId), productId(productId), frameHeaderTransfer(nullptr), terminate(false) {
    auto errorCode = libusb_init(nullptr);
    if (errorCode < 0) throw std::runtime_error("Failed to initialize libusb");

    startTimer(1000); // check back every second
}

UsbCommunicator::~UsbCommunicator() {
    terminate = true;
    if (pollThread.joinable()) {
        pollThread.join();
    }
}

void UsbCommunicator::onTransferFinished(libusb_transfer *transfer) {
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
            case LIBUSB_TRANSFER_COMPLETED:
                printf("Transfer completed\n");
                break;
            default:
                printf("snd transfer failed with status: %d\n", transfer->status);
                break;
        }
        terminate.store(true);
        libusb_close(handle);
        handle = nullptr;
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

void UsbCommunicator::pollUsbForEvents() {
    static struct timeval timeout_500ms = {0, 500000};
    int terminate_main_loop = 0;
    while (!terminate_main_loop && !terminate.load()) {
        if (libusb_handle_events_timeout_completed(nullptr, &timeout_500ms, &terminate_main_loop) < 0) {
            assert(false);
        }
    }
}

libusb_transfer *UsbCommunicator::allocateAndPrepareTransferChunk(libusb_device_handle *handle, UsbCommunicator *instance, unsigned char *buffer, int bufferSize, const unsigned char endpoint) {
    // Allocate a transfer structure
    auto transfer = libusb_alloc_transfer(0);
    if (!transfer) return nullptr;

    libusb_fill_bulk_transfer(transfer, handle, endpoint, buffer, bufferSize,
                              onTransferFinishedStatic, instance, 1000);
    return transfer;
}

bool UsbCommunicator::findDeviceHandleAndStartPolling() {
    libusb_device **devices;
    auto count = libusb_get_device_list(nullptr, &devices);
    if (count < 0) throw std::runtime_error("could not get usb device list");

    // Look for the one matching Push 2's descriptors
    libusb_device *device;
    int errorCode;
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
        if (pollThread.joinable()) {
            pollThread.join();
            headerNeedsSending.store(true);
            terminate.store(false);
        }
        pollThread = std::thread(&UsbCommunicator::pollUsbForEvents, this);
        return true;
    }

    return false;
}

void UsbCommunicator::timerCallback() {
    if (handle == nullptr) findDeviceHandleAndStartPolling();
}
