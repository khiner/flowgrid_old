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

#include "../libusb/libusb.h"

namespace {
    using namespace ableton;
    using namespace std;

    // Uses libusb to create a device handle for the push display
    void findPushDisplayDeviceHandle(libusb_device_handle **pHandle) {
        int errorCode = libusb_init(nullptr);

        // Initialises the library
        if (errorCode < 0) {
            throw runtime_error("Failed to initialize usblib");
        }

        libusb_set_debug(nullptr, LIBUSB_LOG_LEVEL_ERROR);

        // Get a list of connected devices
        libusb_device **devices;
        ssize_t count;
        count = libusb_get_device_list(nullptr, &devices);
        if (count < 0) {
            throw runtime_error("could not get usb device list");
        }

        // Look for the one matching push2's decriptors
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

            const uint16_t kAbletonVendorID = 0x2982;
            const uint16_t kPush2ProductID = 0x1967;

            if (descriptor.bDeviceClass == LIBUSB_CLASS_PER_INTERFACE
                && descriptor.idVendor == kAbletonVendorID
                && descriptor.idProduct == kPush2ProductID) {
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

        *pHandle = device_handle;
        libusb_free_device_list(devices, 1);

        if (!device_handle) {
            throw runtime_error(errorMsg);
        }
    }

    // Callback received whenever a transfer has been completed.
    // We defer the processing to the communicator class
    void LIBUSB_CALL onTransferFinished(libusb_transfer *transfer) {
        static_cast<ableton::Push2UsbCommunicator *>(transfer->user_data)->onTransferFinished(transfer);
    }

    // Allocate a libusb_transfer mapped to a transfer buffer. It also sets
    // up the callback needed to communicate the transfer is done
    libusb_transfer *allocateAndPrepareTransferChunk(
            libusb_device_handle *handle,
            Push2UsbCommunicator *instance,
            unsigned char *buffer,
            int bufferSize) {
        // Allocate a transfer structure
        libusb_transfer *transfer = libusb_alloc_transfer(0);
        if (!transfer) {
            return nullptr;
        }

        // Sets the transfer characteristic
        const unsigned char kPush2BulkEPOut = 0x01;

        libusb_fill_bulk_transfer(transfer, handle, kPush2BulkEPOut,
                                  buffer, bufferSize,
                                  onTransferFinished, instance, 1000);
        return transfer;
    }
}

Push2UsbCommunicator::Push2UsbCommunicator(const Push2Display::pixel_t *dataSource)
        : handle(nullptr) {
    // Capture the data source
    this->dataSource = dataSource;

    // Initialise the handle
    findPushDisplayDeviceHandle(&handle);

    // Initiate a thread so we can receive events from libusb
    terminate = false;
    pollThread = std::thread(&Push2UsbCommunicator::pollUsbForEvents, this);
}

Push2UsbCommunicator::~Push2UsbCommunicator() {
    // shutdown the polling thread
    terminate = true;
    if (pollThread.joinable()) {
        pollThread.join();
    }
}

void Push2UsbCommunicator::startSending() {
    currentLine = 0;

    // transfer struct for the frame header
    static unsigned char frameHeader[16] =
            {
                    0xFF, 0xCC, 0xAA, 0x88,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00
            };

    frameHeaderTransfer =
            allocateAndPrepareTransferChunk(handle, this, frameHeader, sizeof(frameHeader));

    for (int i = 0; i < SEND_BUFFER_COUNT; i++) {
        unsigned char *buffer = (sendBuffers + i * SEND_BUFFER_SIZE);

        // Allocates a transfer struct for the send buffers
        libusb_transfer *transfer =
                allocateAndPrepareTransferChunk(handle, this, buffer, SEND_BUFFER_SIZE);

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

    const unsigned char *src = (const unsigned char *) dataSource + LINE_SIZE_BYTES * currentLine;
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

void LIBUSB_CALL Push2UsbCommunicator::onTransferFinished(libusb_transfer *transfer) {
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
        onFrameCompleted();
    } else {
        sendNextSlice(transfer);
    }
}

void Push2UsbCommunicator::onFrameCompleted() {
    // Insert code here if you want anything to happen after each frame
}

void Push2UsbCommunicator::pollUsbForEvents() {
    static struct timeval timeout_500ms = {0, 500000};
    int terminate_main_loop = 0;

    while (!terminate_main_loop && !terminate.load()) {
        if (libusb_handle_events_timeout_completed(nullptr, &timeout_500ms, &terminate_main_loop) < 0) {
            assert(false);
        }
    }
}
