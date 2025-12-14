/*******************************************************************************
** @file       DrawingAPI.hpp
** @author     Headless VT Drawing API
** @copyright  The Open-Agriculture Developers
*******************************************************************************/
#ifndef DRAWING_API_HPP
#define DRAWING_API_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <array>

namespace drawing {

/// @brief Color representation in RGBA format
struct Color {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
    
    Color(std::uint8_t red = 0, std::uint8_t green = 0, std::uint8_t blue = 0, std::uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}
    
    // Create from VT color index (0-255)
    static Color fromVTColorIndex(std::uint8_t colorIndex);
    
    // Predefined VT colors
    static const Color Black;
    static const Color White;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Yellow;
    static const Color Cyan;
    static const Color Magenta;
    static const Color Grey;
    static const Color Transparent;
};

/// @brief Font styles for text rendering
enum class FontStyle {
    Normal,
    Bold,
    Italic,
    BoldItalic
};

/// @brief Text alignment options
enum class TextAlignment {
    Left,
    Center,
    Right
};

/// @brief Image data container
struct ImageData {
    std::uint16_t width;
    std::uint16_t height;
    std::vector<std::uint8_t> pixels; // RGBA format
    
    ImageData() : width(0), height(0) {}
    ImageData(std::uint16_t w, std::uint16_t h) : width(w), height(h), pixels(w * h * 4, 0) {}
};

/// @brief VT-specific drawing API interface
class DrawingAPI {
public:
    DrawingAPI(std::uint16_t screenWidth, std::uint16_t screenHeight);
    virtual ~DrawingAPI() = default;
    
    // Screen management
    /// @brief Clear the entire screen with a VT color index
    virtual void clearScreen(std::uint8_t vtColorIndex = 0);
    
    /// @brief Clear the entire screen with an RGB color
    virtual void clearScreenRGB(const Color& color);
    
    /// @brief Get screen dimensions
    std::uint16_t getScreenWidth() const { return screenWidth; }
    std::uint16_t getScreenHeight() const { return screenHeight; }
    
    /// @brief Set the VT color palette
    void setColorPalette(const std::array<Color, 256>& palette);
    
    // Text rendering
    /// @brief Draw text at specified position using VT attributes
    /// @param x X coordinate (left edge)
    /// @param y Y coordinate (top edge)
    /// @param text Text to draw
    /// @param vtColorIndex VT color index (0-255)
    /// @param fontIndex VT font index
    /// @param fontSize Font size in pixels (height)
    /// @param fontStyle VT font style
    /// @param alignment Text alignment
    virtual void drawVTText(std::int16_t x, std::int16_t y, 
                           const std::string& text,
                           std::uint8_t vtColorIndex,
                           std::uint8_t fontIndex = 0,
                           std::uint8_t fontSize = 16,
                           std::uint8_t fontStyle = 0,
                           TextAlignment alignment = TextAlignment::Left);
    
    /// @brief Draw text with RGB color (for non-VT rendering)
    virtual void drawText(std::int16_t x, std::int16_t y, 
                         const std::string& text,
                         const Color& color = Color::White,
                         std::uint8_t fontSize = 12,
                         FontStyle style = FontStyle::Normal,
                         TextAlignment alignment = TextAlignment::Left);
    
    // Shape rendering with VT color indices
    /// @brief Draw a filled rectangle using VT color index
    virtual void drawVTFilledRectangle(std::int16_t x, std::int16_t y,
                                      std::uint16_t width, std::uint16_t height,
                                      std::uint8_t vtFillColorIndex);
    
    /// @brief Draw a rectangle outline using VT color index
    virtual void drawVTRectangle(std::int16_t x, std::int16_t y,
                                std::uint16_t width, std::uint16_t height,
                                std::uint8_t vtLineColorIndex,
                                std::uint16_t lineWidth = 1);
    
    /// @brief Draw a complete rectangle with fill and outline
    virtual void drawVTRectangleComplete(std::int16_t x, std::int16_t y,
                                        std::uint16_t width, std::uint16_t height,
                                        std::uint8_t vtLineColorIndex,
                                        std::uint16_t lineWidth,
                                        std::uint8_t vtFillColorIndex);
    
    // RGB shape rendering (for non-VT rendering)
    virtual void drawFilledRectangle(std::int16_t x, std::int16_t y,
                                    std::uint16_t width, std::uint16_t height,
                                    const Color& color);
    
    virtual void drawRectangle(std::int16_t x, std::int16_t y,
                              std::uint16_t width, std::uint16_t height,
                              const Color& color,
                              std::uint8_t lineWidth = 1);
    
    // VT ellipse/circle rendering
    virtual void drawVTFilledEllipse(std::int16_t centerX, std::int16_t centerY,
                                     std::uint16_t width, std::uint16_t height,
                                     std::uint8_t vtFillColorIndex);
    
    virtual void drawVTEllipse(std::int16_t centerX, std::int16_t centerY,
                              std::uint16_t width, std::uint16_t height,
                              std::uint8_t vtLineColorIndex,
                              std::uint16_t lineWidth = 1);
    
    // VT line rendering
    virtual void drawVTLine(std::int16_t x1, std::int16_t y1,
                           std::int16_t x2, std::int16_t y2,
                           std::uint8_t vtLineColorIndex,
                           std::uint16_t lineWidth = 1);
    
    // RGB circle/line rendering (for non-VT rendering)
    virtual void drawFilledCircle(std::int16_t centerX, std::int16_t centerY,
                                  std::uint16_t radius,
                                  const Color& color);
    
    virtual void drawCircle(std::int16_t centerX, std::int16_t centerY,
                           std::uint16_t radius,
                           const Color& color,
                           std::uint8_t lineWidth = 1);
    
    virtual void drawLine(std::int16_t x1, std::int16_t y1,
                         std::int16_t x2, std::int16_t y2,
                         const Color& color,
                         std::uint8_t lineWidth = 1);
    
    // Image rendering
    /// @brief Draw an image at specified position
    /// @param x X coordinate (top-left corner)
    /// @param y Y coordinate (top-left corner)
    /// @param image Image data to draw
    /// @param scale Scale factor (1.0 = original size)
    virtual void drawImage(std::int16_t x, std::int16_t y,
                          const ImageData& image,
                          float scale = 1.0f);
    
    /// @brief Draw a scaled image
    /// @param x X coordinate (top-left corner)
    /// @param y Y coordinate (top-left corner)
    /// @param image Image data to draw
    /// @param width Desired width
    /// @param height Desired height
    virtual void drawScaledImage(std::int16_t x, std::int16_t y,
                                 const ImageData& image,
                                 std::uint16_t width, std::uint16_t height);
    
    // Frame management
    /// @brief Present the current frame to the screen (if buffered)
    virtual void present();
    
protected:
    /// @brief Convert VT color index to RGB color
    Color getColorFromIndex(std::uint8_t vtColorIndex) const;
    
    std::uint16_t screenWidth;
    std::uint16_t screenHeight;
    std::array<Color, 256> colorPalette;
    bool customPaletteSet;
};

} // namespace drawing

#endif // DRAWING_API_HPP
