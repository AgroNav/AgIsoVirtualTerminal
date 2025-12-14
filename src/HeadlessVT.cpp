#include "HeadlessVT.hpp"
#include "isobus/utility/system_timing.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

void HeadlessLogger::sink_CAN_stack_log(LoggingLevel level, const std::string &logText)
{
    std::string levelStr;
    switch (level)
    {
        case LoggingLevel::Debug: levelStr = "DEBUG"; break;
        case LoggingLevel::Info: levelStr = "INFO"; break;
        case LoggingLevel::Warning: levelStr = "WARN"; break;
        case LoggingLevel::Error: levelStr = "ERROR"; break;
        case LoggingLevel::Critical: levelStr = "CRITICAL"; break;
    }
    std::cout << "[" << levelStr << "] " << logText << std::endl;
}

HeadlessVT::HeadlessVT(std::shared_ptr<isobus::InternalControlFunction> serverControlFunction) :
    VirtualTerminalServer(serverControlFunction),
    languageCommandInterface(serverControlFunction)
{
    // Initialize Logger
    isobus::CANStackLogger::set_can_stack_logger_sink(&logger);
    isobus::CANStackLogger::set_log_level(isobus::CANStackLogger::LoggingLevel::Debug);

    VirtualTerminalServer::initialize();
    
    // Initialize Language Command Interface
    if (languageCommandInterface.get_country_code().empty())
    {
        languageCommandInterface.set_country_code("US");
    }
    if (languageCommandInterface.get_language_code().empty())
    {
        languageCommandInterface.set_language_code("en");
    }
    languageCommandInterface.initialize();

    // Initialize Time/Date
    auto timerCallback = std::bind(&HeadlessVT::timeAndDateCallback, this, std::placeholders::_1);
    timeServingInterface = std::make_unique<isobus::TimeDateInterface>(serverControlFunction, timerCallback);
    timeServingInterface->initialize();

    // Initialize Diagnostic Protocol
    diagnosticProtocol = std::make_unique<isobus::DiagnosticProtocol>(serverControlFunction);
    diagnosticProtocol->set_product_identification_brand("Open-Agriculture");
    diagnosticProtocol->set_product_identification_model("AgIsoVirtualTerminal");
    diagnosticProtocol->initialize();
}

HeadlessVT::~HeadlessVT()
{
    isobus::CANStackLogger::set_can_stack_logger_sink(nullptr);
}

void HeadlessVT::update()
{
    diagnosticProtocol->update();
    
    // Status message logic
    if ((isobus::SystemTiming::time_expired_ms(statusMessageTimestamp_ms, 1000)) &&
        (send_status_message()))
    {
        statusMessageTimestamp_ms = isobus::SystemTiming::get_timestamp_ms();
    }

    // Working set processing logic
    for (auto &ws : managedWorkingSetList)
    {
        if (isobus::VirtualTerminalServerManagedWorkingSet::ObjectPoolProcessingThreadState::Success == ws->get_object_pool_processing_state())
        {
            ws->join_parsing_thread();

            // In headless, we might want to auto-select the first working set or similar logic
            auto workingSetObject = std::static_pointer_cast<isobus::WorkingSet>(ws->get_working_set_object());
            if ((isobus::NULL_CAN_ADDRESS == activeWorkingSetMasterAddress) &&
                (nullptr != workingSetObject) &&
                (workingSetObject->get_selectable()))
            {
                ws->set_working_set_maintenance_message_timestamp_ms(isobus::SystemTiming::get_timestamp_ms());
                // change_selected_working_set(wsIndex); // Not implemented in headless yet
                activeWorkingSetMasterAddress = ws->get_control_function()->get_address();
                activeWorkingSet = ws;
                activeWorkingSetDataMaskObjectID = workingSetObject->get_active_mask();
                // activeWorkingSetSoftkeyMaskObjectID = workingSetObject->get_soft_key_mask();
            }

            if (ws->get_was_object_pool_loaded_from_non_volatile_memory())
            {
                send_load_version_response(0, ws->get_control_function());
            }
            else
            {
                send_end_of_object_pool_response(true, isobus::NULL_OBJECT_ID, isobus::NULL_OBJECT_ID, 0, ws->get_control_function());
            }
            
            isobus::CANStackLogger::info("Working set loaded: " + std::to_string(ws->get_control_function()->get_address()));
        }
        else if (isobus::VirtualTerminalServerManagedWorkingSet::ObjectPoolProcessingThreadState::Fail == ws->get_object_pool_processing_state())
        {
            ws->join_parsing_thread();

            if (ws->get_was_object_pool_loaded_from_non_volatile_memory())
            {
                send_load_version_response(1, ws->get_control_function());
            }
            else
            {
                send_end_of_object_pool_response(true, isobus::NULL_OBJECT_ID, ws->get_object_pool_faulting_object_id(), 0, ws->get_control_function());
            }
            isobus::CANStackLogger::error("Working set load failed: " + std::to_string(ws->get_control_function()->get_address()));
        }
        else if (isobus::SystemTiming::time_expired_ms(ws->get_working_set_maintenance_message_timestamp_ms(), 3000) || ws->is_deletion_requested())
        {
            managedWorkingSetIopLoadStateMap[ws] = false;
            
            // Handle disconnection
             if (managedWorkingSetList.empty())
            {
                activeWorkingSetMasterAddress = isobus::NULL_CAN_ADDRESS;
                activeWorkingSetDataMaskObjectID = isobus::NULL_OBJECT_ID;
            }
            // Simplified disconnection logic for headless
        }
    }
    
    // Remove disconnected working sets
    auto it = std::remove_if(managedWorkingSetList.begin(), managedWorkingSetList.end(), [](std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> ws) {
        return isobus::SystemTiming::time_expired_ms(ws->get_working_set_maintenance_message_timestamp_ms(), 3000) || ws->is_deletion_requested();
    });
    managedWorkingSetList.erase(it, managedWorkingSetList.end());
}

bool HeadlessVT::get_is_enough_memory(std::uint32_t) const
{
    return true;
}

isobus::VirtualTerminalBase::VTVersion HeadlessVT::get_version() const
{
    return versionToReport;
}

std::uint8_t HeadlessVT::get_number_of_navigation_soft_keys() const
{
    return 0;
}

std::uint8_t HeadlessVT::get_soft_key_descriptor_x_pixel_width() const
{
    return 60;
}

std::uint8_t HeadlessVT::get_soft_key_descriptor_y_pixel_height() const
{
    return 60;
}

std::uint8_t HeadlessVT::get_number_of_possible_virtual_soft_keys_in_soft_key_mask() const
{
    return 64;
}

std::uint8_t HeadlessVT::get_number_of_physical_soft_keys() const
{
    return 6;
}

std::uint16_t HeadlessVT::get_data_mask_area_size_x_pixels() const
{
    return 480;
}

std::uint16_t HeadlessVT::get_data_mask_area_size_y_pixels() const
{
    return 480;
}

void HeadlessVT::suspend_working_set(std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> workingSetWithError)
{
    isobus::CANStackLogger::error("Suspending working set due to error.");
}

isobus::VirtualTerminalServer::SupportedWideCharsErrorCode HeadlessVT::get_supported_wide_chars(std::uint8_t, std::uint16_t, std::uint16_t, std::uint8_t &, std::vector<std::uint8_t> &)
{
    return SupportedWideCharsErrorCode::AnyOtherError;
}

bool HeadlessVT::timeAndDateCallback(isobus::TimeDateInterface::TimeAndDate &timeAndDateToPopulate)
{
    return false; 
}

std::vector<std::array<std::uint8_t, 7>> HeadlessVT::get_versions(isobus::NAME clientNAME)
{
    return {};
}

std::vector<std::uint8_t> HeadlessVT::get_supported_objects() const
{
    return {};
}

std::vector<std::uint8_t> HeadlessVT::load_version(const std::vector<std::uint8_t> &versionLabel, isobus::NAME clientNAME)
{
    return {};
}

bool HeadlessVT::save_version(const std::vector<std::uint8_t> &objectPool, const std::vector<std::uint8_t> &versionLabel, isobus::NAME clientNAME)
{
    return false;
}

bool HeadlessVT::delete_version(const std::vector<std::uint8_t> &versionLabel, isobus::NAME clientNAME)
{
    return false;
}

bool HeadlessVT::delete_all_versions(isobus::NAME clientNAME)
{
    return false;
}

bool HeadlessVT::delete_object_pool(isobus::NAME clientNAME)
{
    return false;
}
