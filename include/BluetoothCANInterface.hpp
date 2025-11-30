#pragma once

#include "isobus/hardware_integration/can_hardware_plugin.hpp"
#include "isobus/isobus/can_message_frame.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <queue>

class BluetoothCANInterface : public isobus::CANHardwarePlugin
{
public:
    BluetoothCANInterface();
    virtual ~BluetoothCANInterface();

    // --- Required Virtual Methods ---

    /// @brief Returns the name of the plugin
    std::string get_name() const override;

    /// @brief Returns true if the driver is connected and usable
    bool get_is_valid() const override;

    /// @brief Connects to the hardware
    void open() override;

    /// @brief Disconnects from the hardware
    void close() override;

    /// @brief Reads a frame from the hardware
    /// @param[out] canFrame The frame to populate with data read from hardware
    /// @return true if a frame was read, false otherwise
    bool read_frame(isobus::CANMessageFrame &canFrame) override;

    /// @brief Writes a frame to the hardware
    /// @param[in] canFrame The frame containing data to write
    /// @return true if the frame was written successfully
    bool write_frame(const isobus::CANMessageFrame &canFrame) override;

    /// @brief Internal method to receive frames from the Objective-C delegate
    void receive_frame_internal(const isobus::CANMessageFrame& frame);

private:
    bool connected = false;
    // Add your hardware-specific handles or variables here
    // For example, a socket or a serial port handle for Bluetooth SPP
    
    // Simulation of a receive buffer
    std::queue<isobus::CANMessageFrame> rxQueue;
    std::mutex rxMutex;
};
