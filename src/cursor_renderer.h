#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <cmath>

#pragma comment(lib, "gdiplus.lib")

enum class CursorStyle {
    MacClassic = 0, // Authentic macOS black pointer with crisp white border and drop shadow
    WindowsModern = 1, // Windows 11 style white pointer with dark border
    SystemNative = 2   // Dynamically capture and scale the active Windows system cursor
};

class CursorRenderer {
public:
    static constexpr int BUFFER_SIZE = 320; // 320x320 ARGB surface accommodates up to 4.5x scale + shadow
    static constexpr int HOTSPOT_OFFSET = 24; // Margin around top-left so shadow doesn't clip

    CursorRenderer() 
        : hdc_mem_(nullptr)
        , hbmp_(nullptr)
        , old_bmp_(nullptr)
        , bits_(nullptr)
        , style_(CursorStyle::MacClassic) {
    }

    ~CursorRenderer() {
        Cleanup();
    }

    bool Initialize(HDC screen_dc) {
        Cleanup();

        hdc_mem_ = CreateCompatibleDC(screen_dc);
        if (!hdc_mem_) return false;

        BITMAPINFO bmi = { 0 };
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = BUFFER_SIZE;
        bmi.bmiHeader.biHeight = -BUFFER_SIZE; // Top-down DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        hbmp_ = CreateDIBSection(hdc_mem_, &bmi, DIB_RGB_COLORS, &bits_, nullptr, 0);
        if (!hbmp_ || !bits_) {
            Cleanup();
            return false;
        }

        old_bmp_ = SelectObject(hdc_mem_, hbmp_);
        return true;
    }

    void Cleanup() {
        if (hdc_mem_) {
            if (old_bmp_) {
                SelectObject(hdc_mem_, old_bmp_);
                old_bmp_ = nullptr;
            }
            DeleteDC(hdc_mem_);
            hdc_mem_ = nullptr;
        }
        if (hbmp_) {
            DeleteObject(hbmp_);
            hbmp_ = nullptr;
            bits_ = nullptr;
        }
    }

    void SetStyle(CursorStyle style) {
        style_ = style;
    }

    CursorStyle GetStyle() const {
        return style_;
    }

    // Render the cursor at the specified scale and overall alpha into the layered window DC
    // scale: 1.0f (normal) to 4.0f (maximum)
    // alpha: 0 to 255
    void Render(float scale, BYTE alpha) {
        if (!bits_ || !hdc_mem_) return;

        // Clear 32-bit ARGB buffer to fully transparent
        ZeroMemory(bits_, BUFFER_SIZE * BUFFER_SIZE * 4);

        Gdiplus::Graphics g(hdc_mem_);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

        float tip_x = static_cast<float>(HOTSPOT_OFFSET);
        float tip_y = static_cast<float>(HOTSPOT_OFFSET);

        if (style_ == CursorStyle::MacClassic) {
            RenderMacCursor(g, tip_x, tip_y, scale, alpha);
        } else if (style_ == CursorStyle::WindowsModern) {
            RenderWindowsCursor(g, tip_x, tip_y, scale, alpha);
        } else {
            RenderSystemCursor(tip_x, tip_y, scale);
        }
    }

    HDC GetDC() const { return hdc_mem_; }

private:
    void RenderMacCursor(Gdiplus::Graphics& g, float tip_x, float tip_y, float scale, BYTE alpha) {
        // macOS pointer polygon vertices normalized to base unit
        // Base unit factor: ~1.55f produces ~32px arrow at scale 1.0
        float u = 1.55f * scale;

        // Vector coordinates matching macOS pointer anatomy:
        // Tip at (0,0)
        Gdiplus::PointF pts[] = {
            Gdiplus::PointF(tip_x + 0.0f * u,  tip_y + 0.0f * u),   // Tip
            Gdiplus::PointF(tip_x + 0.0f * u,  tip_y + 17.0f * u),  // Left vertical spine
            Gdiplus::PointF(tip_x + 4.3f * u,  tip_y + 13.0f * u),  // Left notch
            Gdiplus::PointF(tip_x + 7.2f * u,  tip_y + 20.0f * u),  // Stem bottom-left
            Gdiplus::PointF(tip_x + 9.8f * u,  tip_y + 18.9f * u),  // Stem bottom-right
            Gdiplus::PointF(tip_x + 6.9f * u,  tip_y + 12.0f * u),  // Right notch
            Gdiplus::PointF(tip_x + 12.8f * u, tip_y + 12.0f * u)   // Right wing tip
        };

        // 1. Draw soft drop shadow (macOS style)
        // Offset shadow slightly down-right with decreasing alpha
        for (int layer = 3; layer >= 1; --layer) {
            float s_off_x = 1.5f * layer;
            float s_off_y = 2.5f * layer;
            BYTE s_alpha = static_cast<BYTE>((alpha * (35 - layer * 8)) / 255);

            Gdiplus::PointF shadow_pts[7];
            for (int i = 0; i < 7; ++i) {
                shadow_pts[i] = Gdiplus::PointF(pts[i].X + s_off_x, pts[i].Y + s_off_y);
            }

            Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(s_alpha, 0, 0, 0));
            g.FillPolygon(&shadowBrush, shadow_pts, 7);
        }

        // 2. White outer border (crisp outline with rounded joins)
        float border_width = (std::max)(2.0f, 1.4f * std::sqrt(scale));
        Gdiplus::Pen whitePen(Gdiplus::Color(alpha, 255, 255, 255), border_width);
        whitePen.SetLineJoin(Gdiplus::LineJoinRound);
        whitePen.SetStartCap(Gdiplus::LineCapRound);
        whitePen.SetEndCap(Gdiplus::LineCapRound);
        g.DrawPolygon(&whitePen, pts, 7);

        // 3. Black pointer body
        Gdiplus::SolidBrush blackBrush(Gdiplus::Color(alpha, 12, 12, 12));
        g.FillPolygon(&blackBrush, pts, 7);
    }

    void RenderWindowsCursor(Gdiplus::Graphics& g, float tip_x, float tip_y, float scale, BYTE alpha) {
        float u = 1.50f * scale;

        Gdiplus::PointF pts[] = {
            Gdiplus::PointF(tip_x + 0.0f * u,  tip_y + 0.0f * u),
            Gdiplus::PointF(tip_x + 0.0f * u,  tip_y + 16.0f * u),
            Gdiplus::PointF(tip_x + 3.8f * u,  tip_y + 12.5f * u),
            Gdiplus::PointF(tip_x + 6.5f * u,  tip_y + 18.5f * u),
            Gdiplus::PointF(tip_x + 8.8f * u,  tip_y + 17.5f * u),
            Gdiplus::PointF(tip_x + 6.0f * u,  tip_y + 11.8f * u),
            Gdiplus::PointF(tip_x + 12.0f * u, tip_y + 11.8f * u)
        };

        // Subtle shadow
        float s_off = 2.0f;
        BYTE s_alpha = static_cast<BYTE>((alpha * 45) / 255);
        Gdiplus::PointF shadow_pts[7];
        for (int i = 0; i < 7; ++i) {
            shadow_pts[i] = Gdiplus::PointF(pts[i].X + s_off, pts[i].Y + s_off);
        }
        Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(s_alpha, 0, 0, 0));
        g.FillPolygon(&shadowBrush, shadow_pts, 7);

        // Black outer border
        float border_width = (std::max)(2.0f, 1.4f * std::sqrt(scale));
        Gdiplus::Pen blackPen(Gdiplus::Color(alpha, 0, 0, 0), border_width);
        blackPen.SetLineJoin(Gdiplus::LineJoinMiter);
        g.DrawPolygon(&blackPen, pts, 7);

        // White interior
        Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(alpha, 250, 250, 250));
        g.FillPolygon(&whiteBrush, pts, 7);
    }

    void RenderSystemCursor(float tip_x, float tip_y, float scale) {
        CURSORINFO ci = { sizeof(CURSORINFO) };
        if (GetCursorInfo(&ci) && ci.flags == CURSOR_SHOWING && ci.hCursor) {
            ICONINFO ii = { 0 };
            if (GetIconInfo(ci.hCursor, &ii)) {
                int base_w = GetSystemMetrics(SM_CXCURSOR);
                int base_h = GetSystemMetrics(SM_CYCURSOR);
                int target_w = static_cast<int>(base_w * scale);
                int target_h = static_cast<int>(base_h * scale);

                int draw_x = static_cast<int>(tip_x) - static_cast<int>(ii.xHotspot * scale);
                int draw_y = static_cast<int>(tip_y) - static_cast<int>(ii.yHotspot * scale);

                DrawIconEx(hdc_mem_, draw_x, draw_y, ci.hCursor, target_w, target_h, 0, NULL, DI_NORMAL);

                if (ii.hbmColor) DeleteObject(ii.hbmColor);
                if (ii.hbmMask) DeleteObject(ii.hbmMask);
            }
        }
    }

    HDC hdc_mem_;
    HBITMAP hbmp_;
    HGDIOBJ old_bmp_;
    void* bits_;
    CursorStyle style_;
};
