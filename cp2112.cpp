/* CP2112 class for Qt - Version 1.0.0
   Copyright (c) 2026 Samuel Lourenço

   This library is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or (at your
   option) any later version.

   This library is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
   License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this library.  If not, see <https://www.gnu.org/licenses/>.


   Please feel free to contact me via e-mail: samuel.fmlourenco@gmail.com */


// Includes
#include <QObject>
#include "cp2112.h"
extern "C" {
#include "libusb-extra.h"
}

// Definitions
const int IFACE = 0;                  // Interface number
const unsigned int TR_TIMEOUT = 500;  // Transfer timeout in milliseconds

// Private function that is used to perform control transfers
void CP2112::controlTransfer(quint8 bmRequestType, quint8 bRequest, quint16 wValue, quint16 wIndex, unsigned char *data, quint16 wLength, int &errcnt, QString &errstr)
{
    if (!isOpen()) {
        ++errcnt;
        errstr += QObject::tr("In controlTransfer(): device is not open.\n");  // Program logic error
    } else {
        int result = libusb_control_transfer(handle_, bmRequestType, bRequest, wValue, wIndex, data, wLength, TR_TIMEOUT);
        if (result != wLength) {
            ++errcnt;
            errstr += QObject::tr("Failed control transfer (0x%1, 0x%2).\n").arg(bmRequestType, 2, 16, QChar('0')).arg(bRequest, 2, 16, QChar('0'));
            if (result == LIBUSB_ERROR_NO_DEVICE || result == LIBUSB_ERROR_IO || result == LIBUSB_ERROR_PIPE) {  // Note that libusb_control_transfer() may return "LIBUSB_ERROR_IO" [-1] or "LIBUSB_ERROR_PIPE" [-9] on device disconnect
                disconnected_ = true;  // This reports that the device has been disconnected
            }
        }
    }
}

CP2112::CP2112() :
    context_(nullptr),
    handle_(nullptr),
    disconnected_(false),
    kernelWasAttached_(false)
{
}

CP2112::~CP2112()
{
    close();  // The destructor is used to close the device, and this is essential so the device can be freed when the parent object is destroyed
}

// Diagnostic function used to verify if the device has been disconnected
bool CP2112::disconnected() const
{
    return disconnected_;  // Returns true if the device has been disconnected, or false otherwise
}

// Checks if the device is open
bool CP2112::isOpen() const
{
    return handle_ != nullptr;  // Returns true if the device is open, or false otherwise
}

// Closes the device safely, if open
void CP2112::close()
{
    if (isOpen()) {  // This condition avoids a segmentation fault if the calling algorithm tries, for some reason, to close the same device twice (e.g., if the device is already closed when the destructor is called)
        libusb_release_interface(handle_, IFACE);  // Release the interface
        if (kernelWasAttached_) {  // If a kernel driver was attached to the interface before
            libusb_attach_kernel_driver(handle_, IFACE);  // Reattach the kernel driver
        }
        libusb_close(handle_);  // Close the device
        libusb_exit(context_);  // Deinitialize libusb
        handle_ = nullptr;  // Required to mark the device as closed
    }
}

// TODO
void CP2112::hidSendFeatureReport(const QVector<quint8> &data, int &errcnt, QString &errstr)
{
    controlTransfer(static_cast<quint8>(LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT), 0x09, 0x0300 | data.at(0), 0x0000, const_cast<unsigned char *>(data.data()), static_cast<quint16>(data.size()), errcnt, errstr);
}

// Opens the device having the given VID, PID and, optionally, the given serial number, and assigns its handle
int CP2112::open(quint16 vid, quint16 pid, const QString &serial)
{
    int retval;
    if (isOpen()) {  // Just in case the calling algorithm tries to open a device that was already sucessfully open, or tries to open different devices concurrently, all while using (or referencing to) the same object
        retval = SUCCESS;
    } else if (libusb_init(&context_) != 0) {  // Initialize libusb. In case of failure
        retval = ERROR_INIT;
    } else {  // If libusb is initialized
        if (serial.isNull()) {  // Note that serial, by omission, is a null QString
            handle_ = libusb_open_device_with_vid_pid(context_, vid, pid);  // If no serial number is specified, this will open the first device found with matching VID and PID
        } else {
            handle_ = libusb_open_device_with_vid_pid_serial(context_, vid, pid, reinterpret_cast<unsigned char *>(serial.toLatin1().data()));
        }
        if (handle_ == nullptr) {  // If the previous operation fails to get a device handle
            libusb_exit(context_);  // Deinitialize libusb
            retval = ERROR_NOT_FOUND;
        } else {  // If the device is successfully opened and a handle obtained
            if (libusb_kernel_driver_active(handle_, IFACE) == 1) {  // If a kernel driver is active on the interface
                libusb_detach_kernel_driver(handle_, IFACE);  // Detach the kernel driver
                kernelWasAttached_ = true;  // Flag that the kernel driver was attached
            } else {
                kernelWasAttached_ = false;  // The kernel driver was not attached
            }
            if (libusb_claim_interface(handle_, IFACE) != 0) {  // Claim the interface. In case of failure
                if (kernelWasAttached_) {  // If a kernel driver was attached to the interface before
                    libusb_attach_kernel_driver(handle_, IFACE);  // Reattach the kernel driver
                }
                libusb_close(handle_);  // Close the device
                libusb_exit(context_);  // Deinitialize libusb
                handle_ = nullptr;  // Required to mark the device as closed
                retval = ERROR_BUSY;
            } else {
                disconnected_ = false;  // Note that this flag is never assumed to be true for a device that was never opened - See constructor for details!
                retval = SUCCESS;
            }
        }
    }
    return retval;
}

// Issues a reset to the CP2112
void CP2112::reset(int &errcnt, QString &errstr)
{
    QVector<quint8> report{
        RESET_DEVICE,  // Header
        0x01
    };
    hidSendFeatureReport(report, errcnt, errstr);
}

// Helper function to list devices
QStringList CP2112::listDevices(quint16 vid, quint16 pid, int &errcnt, QString &errstr)
{
    QStringList devices;
    libusb_context *context;
    if (libusb_init(&context) != 0) {  // Initialize libusb. In case of failure
        ++errcnt;
        errstr += QObject::tr("Could not initialize libusb.\n");
    } else {  // If libusb is initialized
        libusb_device **devs;
        ssize_t devlist = libusb_get_device_list(context, &devs);  // Get a device list
        if (devlist < 0) {  // If the previous operation fails to get a device list
            ++errcnt;
            errstr += QObject::tr("Failed to retrieve a list of devices.\n");
        } else {
            for (ssize_t i = 0; i < devlist; ++i) {  // Run through all listed devices
                libusb_device_descriptor desc;
                if (libusb_get_device_descriptor(devs[i], &desc) == 0 && desc.idVendor == vid && desc.idProduct == pid) {  // If the device descriptor is retrieved, and both VID and PID correspond to the respective given values
                    libusb_device_handle *handle;
                    if (libusb_open(devs[i], &handle) == 0) {  // Open the listed device. If successfull
                        unsigned char str_desc[256];
                        libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, str_desc, static_cast<int>(sizeof(str_desc)));  // Get the serial number string in ASCII format
                        devices += reinterpret_cast<char *>(str_desc);  // Append the serial number string to the list
                        libusb_close(handle);  // Close the device
                    }
                }
            }
            libusb_free_device_list(devs, 1);  // Free device list
        }
        libusb_exit(context);  // Deinitialize libusb
    }
    return devices;
}
