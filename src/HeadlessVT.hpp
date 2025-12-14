#pragma once

#include "isobus/isobus/isobus_virtual_terminal_server.hpp"
#include "isobus/isobus/isobus_time_date_interface.hpp"
#include "isobus/isobus/isobus_diagnostic_protocol.hpp"
#include "isobus/isobus/can_stack_logger.hpp"
#include "DrawingAPI.hpp"
#include <memory>

class HeadlessLogger : public isobus::CANStackLogger
{
public:
    void sink_CAN_stack_log(LoggingLevel level, const std::string &logText) override;
};

class HeadlessVT : public isobus::VirtualTerminalServer
{
public:
    HeadlessVT(std::shared_ptr<isobus::InternalControlFunction> serverControlFunction);
    ~HeadlessVT();

    void update();

    // VirtualTerminalServer overrides
    bool get_is_enough_memory(std::uint32_t requestedMemory) const override;
    VTVersion get_version() const override;
    std::uint8_t get_number_of_navigation_soft_keys() const override;
    std::uint8_t get_soft_key_descriptor_x_pixel_width() const override;
    std::uint8_t get_soft_key_descriptor_y_pixel_height() const override;
    std::uint8_t get_number_of_possible_virtual_soft_keys_in_soft_key_mask() const override;
    std::uint8_t get_number_of_physical_soft_keys() const override;
    std::uint16_t get_data_mask_area_size_x_pixels() const override;
    std::uint16_t get_data_mask_area_size_y_pixels() const override;
    void suspend_working_set(std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> workingSetWithError) override;
    SupportedWideCharsErrorCode get_supported_wide_chars(std::uint8_t codePlane,
                                                         std::uint16_t firstWideCharInInquiryRange,
                                                         std::uint16_t lastWideCharInInquiryRange,
                                                         std::uint8_t &numberOfRanges,
                                                         std::vector<std::uint8_t> &wideCharRangeArray) override;

    std::vector<std::array<std::uint8_t, 7>> get_versions(isobus::NAME clientNAME) override;
    std::vector<std::uint8_t> get_supported_objects() const override;
    std::vector<std::uint8_t> load_version(const std::vector<std::uint8_t> &versionLabel, isobus::NAME clientNAME) override;
    bool save_version(const std::vector<std::uint8_t> &objectPool, const std::vector<std::uint8_t> &versionLabel, isobus::NAME clientNAME) override;
    bool delete_version(const std::vector<std::uint8_t> &versionLabel, isobus::NAME clientNAME) override;
    bool delete_all_versions(isobus::NAME clientNAME) override;
    bool delete_object_pool(isobus::NAME clientNAME) override;

private:
    bool timeAndDateCallback(isobus::TimeDateInterface::TimeAndDate &timeAndDateToPopulate);
    void on_repaint_callback(std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> ws);
    void on_change_active_mask_callback(std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> ws, std::uint16_t oldMaskId, std::uint16_t newMaskId);
    void on_change_active_softkey_mask_callback(std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> ws, std::uint16_t oldMaskId, std::uint16_t newMaskId);
    
    // Rendering helper
    void render_object(std::shared_ptr<isobus::VTObject> object, std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> ws, std::int16_t parentX = 0, std::int16_t parentY = 0);

    HeadlessLogger logger;
    std::unique_ptr<isobus::TimeDateInterface> timeServingInterface;
    std::unique_ptr<isobus::DiagnosticProtocol> diagnosticProtocol;
    std::unique_ptr<drawing::DrawingAPI> drawingAPI;
};
