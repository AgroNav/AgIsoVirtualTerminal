#pragma once

#include "isobus/isobus/isobus_virtual_terminal_server.hpp"
#include "DrawingAPI.hpp"
#include <memory>

namespace HeadlessVTRenderer
{
    void render_object(std::shared_ptr<isobus::VTObject> object,
                      std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> ws,
                      drawing::DrawingAPI* drawingAPI,
                      std::int16_t parentX,
                      std::int16_t parentY);
}
