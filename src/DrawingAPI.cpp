/*******************************************************************************
** @file       DrawingAPI.cpp
** @author     Headless VT Drawing API
** @copyright  The Open-Agriculture Developers
*******************************************************************************/
#include "DrawingAPI.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

namespace drawing {

// Predefined colors (VT default palette subset)
const Color Color::Black(0, 0, 0);
const Color Color::White(255, 255, 255);
const Color Color::Red(255, 0, 0);
const Color Color::Green(0, 255, 0);
const Color Color::Blue(0, 0, 255);
const Color Color::Yellow(255, 255, 0);
const Color Color::Cyan(0, 255, 255);
const Color Color::Magenta(255, 0, 255);
const Color Color::Grey(128, 128, 128);
const Color Color::Transparent(0, 0, 0, 0);

// VT standard color palette (ISO 11783-6)
Color Color::fromVTColorIndex(std::uint8_t colorIndex)
{
    // Default VT color palette mapping (simplified)
    static const Color defaultPalette[256] = {
        Color(0, 0, 0),         // 0: Black
        Color(255, 255, 255),   // 1: White
        Color(0, 153, 51),      // 2: Green
        Color(0, 153, 204),     // 3: Teal
        Color(153, 51, 51),     // 4: Maroon
        Color(153, 0, 204),     // 5: Purple
        Color(255, 204, 0),     // 6: Yellow/Orange
        Color(204, 204, 204),   // 7: Silver
        Color(153, 153, 153),   // 8: Grey
        Color(0, 0, 255),       // 9: Blue
        // ... (rest would be filled with proper VT palette)
    };
    
    if (colorIndex < 10) {
        return defaultPalette[colorIndex];
    }
    
    // For indices 10-255, generate a reasonable color
    std::uint8_t r = ((colorIndex & 0xE0) >> 5) * 36;
    std::uint8_t g = ((colorIndex & 0x1C) >> 2) * 36;
    std::uint8_t b = (colorIndex & 0x03) * 85;
    return Color(r, g, b);
}

DrawingAPI::DrawingAPI(std::uint16_t screenWidth, std::uint16_t screenHeight)
    : screenWidth(screenWidth), screenHeight(screenHeight), customPaletteSet(false)
{
    // Initialize with default VT color palette
    for (int i = 0; i < 256; i++) {
        colorPalette[i] = Color::fromVTColorIndex(i);
    }
    
    std::cout << "[DrawingAPI] Initialized VT renderer with screen size: " 
              << screenWidth << "x" << screenHeight << std::endl;
}

void DrawingAPI::setColorPalette(const std::array<Color, 256>& palette)
{
    colorPalette = palette;
    customPaletteSet = true;
    std::cout << "[DrawingAPI] Custom color palette loaded" << std::endl;
}

Color DrawingAPI::getColorFromIndex(std::uint8_t vtColorIndex) const
{
    return colorPalette[vtColorIndex];
}

void DrawingAPI::clearScreen(std::uint8_t vtColorIndex)
{
    Color color = getColorFromIndex(vtColorIndex);
    std::cout << "[DrawingAPI] Clear screen with VT color " << static_cast<int>(vtColorIndex)
              << " RGB(" << static_cast<int>(color.r) << ", "
              << static_cast<int>(color.g) << ", "
              << static_cast<int>(color.b) << ")" << std::endl;
}

void DrawingAPI::clearScreenRGB(const Color& color)
{
    std::cout << "[DrawingAPI] Clear screen with RGB(" 
              << static_cast<int>(color.r) << ", "
              << static_cast<int>(color.g) << ", "
              << static_cast<int>(color.b) << ")" << std::endl;
}

void DrawingAPI::drawVTText(std::int16_t x, std::int16_t y,
                            const std::string& text,
                            std::uint8_t vtColorIndex,
                            std::uint8_t fontIndex,
                            std::uint8_t fontSize,
                            std::uint8_t fontStyle,
                            TextAlignment alignment)
{
    Color color = getColorFromIndex(vtColorIndex);
    std::string alignStr = (alignment == TextAlignment::Center) ? "Center" : 
                          (alignment == TextAlignment::Right) ? "Right" : "Left";
    
    std::cout << "[DrawingAPI] Draw VT text at (" << x << ", " << y << "): \"" 
              << text << "\" [Font:" << static_cast<int>(fontIndex) 
              << " Size:" << static_cast<int>(fontSize)
              << " Style:" << static_cast<int>(fontStyle)
              << " Align:" << alignStr
              << " Color:" << static_cast<int>(vtColorIndex) << "]" << std::endl;
}

void DrawingAPI::drawText(std::int16_t x, std::int16_t y,
                         const std::string& text,
                         const Color& color,
                         std::uint8_t fontSize,
                         FontStyle style,
                         TextAlignment alignment)
{
    std::string styleStr = (style == FontStyle::Bold) ? "Bold" :
                          (style == FontStyle::Italic) ? "Italic" :
                          (style == FontStyle::BoldItalic) ? "BoldItalic" : "Normal";
    std::string alignStr = (alignment == TextAlignment::Center) ? "Center" : 
                          (alignment == TextAlignment::Right) ? "Right" : "Left";
    
    std::cout << "[DrawingAPI] Draw RGB text at (" << x << ", " << y << "): \"" 
              << text << "\" [Size:" << static_cast<int>(fontSize) 
              << " Style:" << styleStr
              << " Align:" << alignStr
              << " RGB(" << static_cast<int>(color.r) << ","
              << static_cast<int>(color.g) << "," << static_cast<int>(color.b) << ")]" << std::endl;
}

void DrawingAPI::drawVTFilledRectangle(std::int16_t x, std::int16_t y,
                                       std::uint16_t width, std::uint16_t height,
                                       std::uint8_t vtFillColorIndex)
{
    Color fillColor = getColorFromIndex(vtFillColorIndex);
    std::cout << "[DrawingAPI] Draw VT filled rectangle at (" << x << ", " << y 
              << ") size " << width << "x" << height
              << " VT color " << static_cast<int>(vtFillColorIndex)
              << " RGB(" << static_cast<int>(fillColor.r) << ","
              << static_cast<int>(fillColor.g) << "," << static_cast<int>(fillColor.b) << ")"
              << std::endl;
}

void DrawingAPI::drawVTRectangle(std::int16_t x, std::int16_t y,
                                 std::uint16_t width, std::uint16_t height,
                                 std::uint8_t vtLineColorIndex,
                                 std::uint16_t lineWidth)
{
    Color lineColor = getColorFromIndex(vtLineColorIndex);
    std::cout << "[DrawingAPI] Draw VT rectangle at (" << x << ", " << y 
              << ") size " << width << "x" << height
              << " VT color " << static_cast<int>(vtLineColorIndex)
              << " RGB(" << static_cast<int>(lineColor.r) << ","
              << static_cast<int>(lineColor.g) << "," << static_cast<int>(lineColor.b) << ")"
              << " width " << lineWidth
              << std::endl;
}

void DrawingAPI::drawVTRectangleComplete(std::int16_t x, std::int16_t y,
                                        std::uint16_t width, std::uint16_t height,
                                        std::uint8_t vtLineColorIndex,
                                        std::uint16_t lineWidth,
                                        std::uint8_t vtFillColorIndex)
{
    Color lineColor = getColorFromIndex(vtLineColorIndex);
    Color fillColor = getColorFromIndex(vtFillColorIndex);
    std::cout << "[DrawingAPI] Draw VT complete rectangle at (" << x << ", " << y 
              << ") size " << width << "x" << height
              << " Line[VT:" << static_cast<int>(vtLineColorIndex)
              << " RGB(" << static_cast<int>(lineColor.r) << ","
              << static_cast<int>(lineColor.g) << "," << static_cast<int>(lineColor.b) << ")]"
              << " Fill[VT:" << static_cast<int>(vtFillColorIndex)
              << " RGB(" << static_cast<int>(fillColor.r) << ","
              << static_cast<int>(fillColor.g) << "," << static_cast<int>(fillColor.b) << ")]"
              << " width " << lineWidth
              << std::endl;
}

void DrawingAPI::drawFilledRectangle(std::int16_t x, std::int16_t y,
                                    std::uint16_t width, std::uint16_t height,
                                    const Color& color)
{
    std::cout << "[DrawingAPI] Draw filled rectangle at (" << x << ", " << y 
              << ") size: " << width << "x" << height
              << " color: RGB(" << static_cast<int>(color.r) << ","
              << static_cast<int>(color.g) << "," << static_cast<int>(color.b) << ")"
              << std::endl;
}

void DrawingAPI::drawRectangle(std::int16_t x, std::int16_t y,
                              std::uint16_t width, std::uint16_t height,
                              const Color& color,
                              std::uint8_t lineWidth)
{
    std::cout << "[DrawingAPI] Draw rectangle outline at (" << x << ", " << y 
              << ") size: " << width << "x" << height
              << " line width: " << static_cast<int>(lineWidth)
              << " color: RGB(" << static_cast<int>(color.r) << ","
              << static_cast<int>(color.g) << "," << static_cast<int>(color.b) << ")"
              << std::endl;
}

void DrawingAPI::drawVTFilledEllipse(std::int16_t centerX, std::int16_t centerY,
                                     std::uint16_t width, std::uint16_t height,
                                     std::uint8_t vtFillColorIndex)
{
    Color fillColor = getColorFromIndex(vtFillColorIndex);
    std::cout << "[DrawingAPI] Draw VT filled ellipse at (" << centerX << ", " << centerY 
              << ") size " << width << "x" << height
              << " VT color " << static_cast<int>(vtFillColorIndex)
              << " RGB(" << static_cast<int>(fillColor.r) << ","
              << static_cast<int>(fillColor.g) << "," << static_cast<int>(fillColor.b) << ")"
              << std::endl;
}

void DrawingAPI::drawVTEllipse(std::int16_t centerX, std::int16_t centerY,
                               std::uint16_t width, std::uint16_t height,
                               std::uint8_t vtLineColorIndex,
                               std::uint16_t lineWidth)
{
    Color lineColor = getColorFromIndex(vtLineColorIndex);
    std::cout << "[DrawingAPI] Draw VT ellipse at (" << centerX << ", " << centerY 
              << ") size " << width << "x" << height
              << " VT color " << static_cast<int>(vtLineColorIndex)
              << " RGB(" << static_cast<int>(lineColor.r) << ","
              << static_cast<int>(lineColor.g) << "," << static_cast<int>(lineColor.b) << ")"
              << " width " << lineWidth
              << std::endl;
}

void DrawingAPI::drawFilledCircle(std::int16_t centerX, std::int16_t centerY,
                                  std::uint16_t radius,
                                  const Color& color)
{
    std::cout << "[DrawingAPI] Draw filled circle at (" << centerX << ", " << centerY 
              << ") radius: " << radius
              << " color: RGB(" << static_cast<int>(color.r) << ","
              << static_cast<int>(color.g) << "," << static_cast<int>(color.b) << ")"
              << std::endl;
}

void DrawingAPI::drawCircle(std::int16_t centerX, std::int16_t centerY,
                           std::uint16_t radius,
                           const Color& color,
                           std::uint8_t lineWidth)
{
    std::cout << "[DrawingAPI] Draw circle outline at (" << centerX << ", " << centerY 
              << ") radius: " << radius
              << " line width: " << static_cast<int>(lineWidth)
              << " color: RGB(" << static_cast<int>(color.r) << ","
              << static_cast<int>(color.g) << "," << static_cast<int>(color.b) << ")"
              << std::endl;
}

void DrawingAPI::drawVTLine(std::int16_t x1, std::int16_t y1,
                            std::int16_t x2, std::int16_t y2,
                            std::uint8_t vtLineColorIndex,
                            std::uint16_t lineWidth)
{
    Color lineColor = getColorFromIndex(vtLineColorIndex);
    std::cout << "[DrawingAPI] Draw VT line from (" << x1 << ", " << y1 
              << ") to (" << x2 << ", " << y2 << ")"
              << " VT color " << static_cast<int>(vtLineColorIndex)
              << " RGB(" << static_cast<int>(lineColor.r) << ","
              << static_cast<int>(lineColor.g) << "," << static_cast<int>(lineColor.b) << ")"
              << " width " << lineWidth
              << std::endl;
}

void DrawingAPI::drawLine(std::int16_t x1, std::int16_t y1,
                         std::int16_t x2, std::int16_t y2,
                         const Color& color,
                         std::uint8_t lineWidth)
{
    std::cout << "[DrawingAPI] Draw line from (" << x1 << ", " << y1 
              << ") to (" << x2 << ", " << y2 << ")"
              << " line width: " << static_cast<int>(lineWidth)
              << " color: RGB(" << static_cast<int>(color.r) << ","
              << static_cast<int>(color.g) << "," << static_cast<int>(color.b) << ")"
              << std::endl;
}

void DrawingAPI::drawImage(std::int16_t x, std::int16_t y,
                          const ImageData& image,
                          float scale)
{
    std::cout << "[DrawingAPI] Draw image at (" << x << ", " << y 
              << ") size: " << image.width << "x" << image.height
              << " scale: " << scale
              << " data size: " << image.pixels.size() << " bytes"
              << std::endl;
}

void DrawingAPI::drawScaledImage(std::int16_t x, std::int16_t y,
                                 const ImageData& image,
                                 std::uint16_t width, std::uint16_t height)
{
    std::cout << "[DrawingAPI] Draw scaled image at (" << x << ", " << y 
              << ") original: " << image.width << "x" << image.height
              << " scaled to: " << width << "x" << height
              << std::endl;
}

void DrawingAPI::present()
{
    std::cout << "[DrawingAPI] Present frame" << std::endl;
}

} // namespace drawing
