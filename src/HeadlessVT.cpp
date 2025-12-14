#include "HeadlessVT.hpp"
#include "HeadlessVTRenderer.hpp"
#include "isobus/utility/system_timing.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <iterator>

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
    VirtualTerminalServer(serverControlFunction)
{
    // Initialize Logger
    isobus::CANStackLogger::set_can_stack_logger_sink(&logger);
    isobus::CANStackLogger::set_log_level(isobus::CANStackLogger::LoggingLevel::Debug);

    VirtualTerminalServer::initialize();

    // Initialize Language Command Interface (using base class's member)
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

    // Initialize Drawing API
    drawingAPI = std::make_unique<drawing::DrawingAPI>(get_data_mask_area_size_x_pixels(), get_data_mask_area_size_y_pixels());

    // Register event callbacks for object pool changes
    get_on_repaint_event_dispatcher().add_listener([this](std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> ws) {
        on_repaint_callback(ws);
    });

    get_on_change_active_mask_event_dispatcher().add_listener([this](std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> ws, std::uint16_t oldMaskId, std::uint16_t newMaskId) {
        on_change_active_mask_callback(ws, oldMaskId, newMaskId);
    });

    get_on_change_active_softkey_mask_event_dispatcher().add_listener([this](std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> ws, std::uint16_t oldMaskId, std::uint16_t newMaskId) {
        on_change_active_softkey_mask_callback(ws, oldMaskId, newMaskId);
    });
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

            // Auto-select the first working set
            auto workingSetObject = std::static_pointer_cast<isobus::WorkingSet>(ws->get_working_set_object());
            if ((isobus::NULL_CAN_ADDRESS == activeWorkingSetMasterAddress) &&
                (nullptr != workingSetObject) &&
                (workingSetObject->get_selectable()))
            {
                ws->set_working_set_maintenance_message_timestamp_ms(isobus::SystemTiming::get_timestamp_ms());

                // Activate the working set
                std::uint16_t previousActiveMask = activeWorkingSetDataMaskObjectID;
                activeWorkingSetMasterAddress = ws->get_control_function()->get_address();
                activeWorkingSet = ws;
                activeWorkingSetDataMaskObjectID = workingSetObject->get_active_mask();

                // Register callbacks for this working set
                ws->save_callback_handle(get_on_repaint_event_dispatcher().add_listener([this](std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet>) { 
                    isobus::CANStackLogger::debug("[Headless VT] Repaint requested"); 
                }));
                ws->save_callback_handle(get_on_change_active_mask_event_dispatcher().add_listener([this](std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> affectedWorkingSet, std::uint16_t workingSet, std::uint16_t newMask) { 
                    this->on_change_active_mask_callback(affectedWorkingSet, workingSet, newMask); 
                }));

                // Trigger initial draw callbacks
                isobus::CANStackLogger::info("[Headless VT] Activating working set with data mask " + std::to_string(activeWorkingSetDataMaskObjectID));
                on_change_active_mask_callback(ws, previousActiveMask, activeWorkingSetDataMaskObjectID);

                // Process macros for activation
                process_macro(activeWorkingSet->get_working_set_object(), isobus::EventID::OnActivate, isobus::VirtualTerminalObjectType::WorkingSet, activeWorkingSet);
                if (previousActiveMask != activeWorkingSetDataMaskObjectID)
                {
                    process_macro(ws->get_object_by_id(previousActiveMask), isobus::EventID::OnHide, isobus::VirtualTerminalObjectType::DataMask, activeWorkingSet);
                    process_macro(ws->get_object_by_id(previousActiveMask), isobus::EventID::OnHide, isobus::VirtualTerminalObjectType::AlarmMask, activeWorkingSet);
                    process_macro(ws->get_object_by_id(activeWorkingSetDataMaskObjectID), isobus::EventID::OnShow, isobus::VirtualTerminalObjectType::DataMask, activeWorkingSet);
                    process_macro(ws->get_object_by_id(activeWorkingSetDataMaskObjectID), isobus::EventID::OnShow, isobus::VirtualTerminalObjectType::AlarmMask, activeWorkingSet);
                }
                
                // Send status message to confirm activation
                if (send_status_message())
                {
                    statusMessageTimestamp_ms = isobus::SystemTiming::get_timestamp_ms();
                }
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
    return VTVersion::Version6;
}

std::uint8_t HeadlessVT::get_number_of_navigation_soft_keys() const
{
    return 0;
}

std::uint8_t HeadlessVT::get_soft_key_descriptor_x_pixel_width() const
{
    return 60; // VT Version 6 minimum
}

std::uint8_t HeadlessVT::get_soft_key_descriptor_y_pixel_height() const
{
    return 60; // VT Version 6 minimum
}

std::uint8_t HeadlessVT::get_number_of_possible_virtual_soft_keys_in_soft_key_mask() const
{
    return 64; // VT Version 4+ supports exactly 64 virtual soft keys
}

std::uint8_t HeadlessVT::get_number_of_physical_soft_keys() const
{
    return 6; // VT Version 4+ minimum
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

std::vector<std::array<std::uint8_t, HeadlessVT::VERSION_LABEL_SIZE>> HeadlessVT::get_versions(isobus::NAME clientNAME)
{
    std::ostringstream nameString;
    nameString << std::hex << std::setfill('0') << std::setw(16) << clientNAME.get_full_name();
    
    std::string isoDirectory = "iso_data/" + nameString.str();
    if (!std::filesystem::exists(isoDirectory) || !std::filesystem::is_directory(isoDirectory))
    {
        isobus::CANStackLogger::info("[VT Server]: No saved object pool data for client: " + nameString.str());
        return {};
    }

    std::vector<std::array<std::uint8_t, VERSION_LABEL_SIZE>> retVal;
    
    for (const auto& entry : std::filesystem::directory_iterator(isoDirectory))
    {
        if (entry.path().extension() != ".iopx")
        {
            continue;
        }

        std::ifstream iopxFile(entry.path(), std::ios::binary);
        if (!iopxFile.is_open())
        {
            continue;
        }

        std::array<std::uint8_t, VERSION_LABEL_SIZE> versionLabel;
        iopxFile.read(reinterpret_cast<char*>(versionLabel.data()), VERSION_LABEL_SIZE);

        // Only add unique version labels
        bool alreadyExists = std::any_of(retVal.begin(), retVal.end(), 
            [&versionLabel](const auto& v) { return v == versionLabel; });

        if (!alreadyExists)
        {
            retVal.push_back(versionLabel);
        }
    }

    return retVal;
}

std::vector<std::uint8_t> HeadlessVT::get_supported_objects() const
{
    // ISO 11783-6 Table A.1 "Virtual terminal objects"
    return { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 37, 39 };
}

std::vector<std::uint8_t> HeadlessVT::load_version(const std::vector<std::uint8_t> &versionLabel, isobus::NAME clientNAME)
{
    if (versionLabel.size() != VERSION_LABEL_SIZE)
    {
        return {};
    }

    std::ostringstream nameString;
    nameString << std::hex << std::setfill('0') << std::setw(16) << clientNAME.get_full_name();

    std::string path = "iso_data/" + nameString.str();
    if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
    {
        return {};
    }

    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        if (entry.path().extension() != ".iopx")
        {
            continue;
        }

        std::ifstream iopxFile(entry.path(), std::ios::binary);
        if (!iopxFile.is_open())
        {
            continue;
        }

        std::vector<std::uint8_t> loadedVersionLabel(VERSION_LABEL_SIZE);
        iopxFile.read(reinterpret_cast<char*>(loadedVersionLabel.data()), VERSION_LABEL_SIZE);

        bool versionMatches = true;
        for (std::uint8_t i = 0; i < VERSION_LABEL_SIZE; i++)
        {
            if (loadedVersionLabel[i] != versionLabel[i])
            {
                versionMatches = false;
                break;
            }
        }

        if (!versionMatches)
        {
            continue;
        }

        // Found matching version - load the object pool data
        iopxFile.seekg(0, std::ios::end);
        std::streamsize fileSize = iopxFile.tellg();
        std::streamsize dataSize = fileSize - VERSION_LABEL_SIZE;

        std::vector<std::uint8_t> loadedIOPData(dataSize);
        iopxFile.seekg(VERSION_LABEL_SIZE, std::ios::beg);
        iopxFile.read(reinterpret_cast<char*>(loadedIOPData.data()), dataSize);

        isobus::CANStackLogger::info("[VT Server]: Loaded object pool version from disk for client: " + nameString.str());
        return loadedIOPData;
    }

    return {};
}

bool HeadlessVT::save_version(const std::vector<std::uint8_t> &objectPool, const std::vector<std::uint8_t> &versionLabel, isobus::NAME clientNAME)
{
    isobus::CANStackLogger::info("[Headless VT] Saving object pool: " + std::to_string(objectPool.size()) + " bytes");
    
    bool retVal = false;

    // Use current working directory
    std::string path = "iso_data";

    // Create main saved data folder if it doesn't exist
    if (!std::filesystem::is_directory(path) || !std::filesystem::exists(path))
    {
        std::filesystem::create_directories(path);
    }

    // Create NAME specific folder
    std::ostringstream nameString;
    nameString << std::hex << std::setfill('0') << std::setw(16) << clientNAME.get_full_name();
    std::string clientPath = path + "/" + nameString.str();

    if (!std::filesystem::is_directory(clientPath) || !std::filesystem::exists(clientPath))
    {
        std::filesystem::create_directory(clientPath);
    }

    // Count existing .iop files to generate unique filename
    std::size_t fileCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(clientPath))
    {
        if (entry.path().extension() == ".iop")
        {
            fileCount++;
        }
    }

    // Save object pool with version label (.iopx)
    std::string iopxPath = clientPath + "/object_pool_with_label_" + std::to_string(fileCount) + ".iopx";
    std::ofstream iopxFile(iopxPath, std::ios::trunc | std::ios::binary);
    
    if (iopxFile.is_open())
    {
        iopxFile.write(reinterpret_cast<const char*>(versionLabel.data()), static_cast<std::streamsize>(versionLabel.size()));
        iopxFile.write(reinterpret_cast<const char*>(objectPool.data()), static_cast<std::streamsize>(objectPool.size()));
        iopxFile.close();
        retVal = true;
        isobus::CANStackLogger::info("[VT Server]: Saved object pool to " + iopxPath);
    }

    // Also save object pool without version label (.iop) for easy inspection
    std::string iopPath = clientPath + "/object_pool_" + std::to_string(fileCount) + ".iop";
    std::ofstream iopFile(iopPath, std::ios::trunc | std::ios::binary);

    if (iopFile.is_open())
    {
        iopFile.write(reinterpret_cast<const char*>(objectPool.data()), static_cast<std::streamsize>(objectPool.size()));
        iopFile.close();
    }

    return retVal;
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

void HeadlessVT::on_repaint_callback(std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> ws)
{
    if (!ws)
    {
        return;
    }
    isobus::CANStackLogger::info("[Headless VT] REPAINT REQUEST - Rendering screen");

    auto wsObject = std::static_pointer_cast<isobus::WorkingSet>(ws->get_working_set_object());
    if (wsObject)
    {
        // Clear screen (VT color 0 = Black)
        drawingAPI->clearScreen(0);

        // Get and render active data mask
        auto dataMask = ws->get_object_by_id(wsObject->get_active_mask());
        if (dataMask)
        {
            isobus::CANStackLogger::debug("[Headless VT] Rendering mask ID: " + std::to_string(dataMask->get_id()));
            HeadlessVTRenderer::render_object(dataMask, ws, drawingAPI.get(), 0, 0);
        }

        // Present the frame
        drawingAPI->present();
    }
}

void HeadlessVT::on_change_active_mask_callback(std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> ws, std::uint16_t oldMaskId, std::uint16_t newMaskId)
{
    if (!ws)
    {
        return;
    }

    isobus::CANStackLogger::info("[Headless VT] ACTIVE MASK CHANGE: " + std::to_string(oldMaskId) + " -> " + std::to_string(newMaskId));

    // Trigger repaint for new mask
    on_repaint_callback(ws);
}

void HeadlessVT::on_change_active_softkey_mask_callback(std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> ws, std::uint16_t oldMaskId, std::uint16_t newMaskId)
{
    if (!ws)
    {
        return;
    }

    std::ostringstream logMessage;
    logMessage << "[Headless VT] SOFTKEY MASK CHANGE:\n";
    logMessage << "  Old Softkey Mask ID: " << oldMaskId << "\n";
    logMessage << "  New Softkey Mask ID: " << newMaskId << "\n";
    
    auto newSoftKeyMask = ws->get_object_by_id(newMaskId);
    if (newSoftKeyMask)
    {
        logMessage << "  Number of soft keys: " << newSoftKeyMask->get_number_children() << "\n";
        
        // List soft key objects
        if (newSoftKeyMask->get_number_children() > 0)
        {
            logMessage << "  Soft keys: ";
            for (std::uint16_t i = 0; i < newSoftKeyMask->get_number_children(); i++)
            {
                std::uint16_t keyId = newSoftKeyMask->get_child_id(i);
                logMessage << keyId << " ";
            }
            logMessage << "\n";
        }
    }

    isobus::CANStackLogger::info(logMessage.str());
}
