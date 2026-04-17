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


#ifndef CP2112_H
#define CP2112_H 

// Includes
#include <QString>
#include <QStringList>
#include <QVector>
#include <libusb-1.0/libusb.h>

class CP2112
{
private:
    libusb_context *context_;
    libusb_device_handle *handle_;
    bool disconnected_, kernelWasAttached_;

    void controlTransfer(quint8 bmRequestType, quint8 bRequest, quint16 wValue, quint16 wIndex, unsigned char *data, quint16 wLength, int &errcnt, QString &errstr);

public:
    // Class definitions
    static const quint16 VID = 0x10c4;      // Default USB vendor ID
    static const quint16 PID = 0xea90;      // Default USB product ID
    static const int SUCCESS = 0;           // Returned by open() if successful
    static const int ERROR_INIT = 1;        // Returned by open() in case of a libusb initialization failure
    static const int ERROR_NOT_FOUND = 2;   // Returned by open() if the device was not found
    static const int ERROR_BUSY = 3;        // Returned by open() if the device is already in use

    // TODO Reports
    static const quint8 RESET_DEVICE = 0x01;                        // Reset_Device command

    explicit CP2112();
    ~CP2112();

    bool disconnected() const;
    bool isOpen() const;

    void close();
    int open(quint16 vid, quint16 pid, const QString &serial = QString());
    void reset(int &errcnt, QString &errstr);

    static QStringList listDevices(quint16 vid, quint16 pid, int &errcnt, QString &errstr);
};

#endif  // CP2112_H
