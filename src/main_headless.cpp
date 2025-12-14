#include "HeadlessVT.hpp"
#include "isobus/hardware_integration/available_can_drivers.hpp"
#include "isobus/hardware_integration/can_hardware_interface.hpp"
#include "isobus/isobus/can_network_manager.hpp"
#include "isobus/isobus/can_partnered_control_function.hpp"
#include "isobus/isobus/can_stack_logger.hpp"

#ifdef __APPLE__
#include "BluetoothCANInterface.hpp"
#endif

#include <memory>
#include <vector>
#include <thread>
#include <csignal>
#include <iostream>
#include <chrono>

std::atomic<bool> running{true};

void signalHandler(int signum) {
    running = false;
}

int main()
{
    signal(SIGINT, signalHandler);

    std::vector<std::shared_ptr<isobus::CANHardwarePlugin>> canDrivers;

#ifdef __APPLE__
    canDrivers.push_back(std::make_shared<BluetoothCANInterface>());
#else
    // Add other drivers for Linux/Windows if needed
    // canDrivers.push_back(std::make_shared<isobus::SocketCANInterface>("can0"));
#endif

    if (canDrivers.empty())
    {
        std::cerr << "No CAN drivers available." << std::endl;
        return 1;
    }

    isobus::CANHardwareInterface::set_number_of_can_channels(1);
    
    // Configure CAN network manager before creating control functions
    auto config = isobus::CANNetworkManager::CANNetwork.get_configuration();
    config.set_max_number_transport_protocol_sessions(256);
    config.set_number_of_packets_per_dpo_message(255);
    config.set_number_of_packets_per_cts_message(255);
    
    isobus::CANHardwareInterface::assign_can_channel_frame_handler(0, canDrivers.at(0));
    
    isobus::NAME serverNAME(0);
    serverNAME.set_function_instance(0);
    serverNAME.set_function_code(static_cast<std::uint8_t>(isobus::NAME::Function::VirtualTerminal));
    serverNAME.set_industry_group(2);
    serverNAME.set_manufacturer_code(1407);
    serverNAME.set_arbitrary_address_capable(true);

    auto serverInternalControlFunction = isobus::CANNetworkManager::CANNetwork.create_internal_control_function(serverNAME, 0, 0x26);
    
    if (!isobus::CANHardwareInterface::start())
    {
        std::cerr << "Failed to start CAN hardware interface." << std::endl;
        return 1;
    }

    HeadlessVT vtServer(serverInternalControlFunction);

    std::cout << "Headless VT Server started. Press Ctrl+C to exit." << std::endl;

    auto driver = canDrivers.at(0);
    auto lastValidTime = std::chrono::steady_clock::now();
    auto startTime = std::chrono::steady_clock::now();
    bool hasRestarted = false;

    while (running)
    {
        // Update the CAN network manager to process incoming/outgoing messages
        isobus::CANNetworkManager::CANNetwork.update();
        
        // Update the VT server
        vtServer.update();
        
        // Simulate the "Restart button" behavior from GUI after 3 seconds
        if (!hasRestarted && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startTime).count() >= 3)
        {
            std::cout << "[Main] Performing restart to initialize connection properly..." << std::endl;
            hasRestarted = true;
            
            // Stop and restart the CAN interface (mimicking GUI restart button)
            isobus::CANHardwareInterface::stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            isobus::CANHardwareInterface::assign_can_channel_frame_handler(0, driver);
            if (!isobus::CANHardwareInterface::start())
            {
                std::cerr << "Failed to restart CAN hardware interface." << std::endl;
            }
            lastValidTime = std::chrono::steady_clock::now();
            continue;
        }

        if (driver->get_is_valid())
        {
            lastValidTime = std::chrono::steady_clock::now();
        }
        else
        {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastValidTime).count() > 5)
            {
                std::cout << "Connection lost or not established. Restarting CAN interface..." << std::endl;
                isobus::CANHardwareInterface::stop();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                isobus::CANHardwareInterface::assign_can_channel_frame_handler(0, driver);
                if (!isobus::CANHardwareInterface::start())
                {
                    std::cerr << "Failed to restart CAN hardware interface." << std::endl;
                }
                lastValidTime = std::chrono::steady_clock::now();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    isobus::CANHardwareInterface::stop();
    return 0;
}
