#pragma once

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"

#include "ESP32-HUB75-VirtualMatrixPanel_T.hpp"
// This header is vendored locally, flat in this same folder (not fetched
// via lib_deps), to avoid PlatformIO's broken cross-library REQUIRES linking
// under the ESP-IDF backend. It transitively includes
// ESP32-HUB75-MatrixPanel-I2S-DMA.h and Adafruit_GFX.h.
//
// NOTE: this is the CURRENT, actively-maintained template-based class.
// The library also ships an older ESP32-VirtualMatrixPanel-I2S-DMA.h with a
// non-templated VirtualMatrixPanel class, but that one is explicitly marked
// deprecated in its own source and produced incorrect output for our panel
// (four-scan pixel remap didn't fully work) - do not switch back to it.

// CHAIN_NONE is correct here: the underlying driver already treats a
// horizontal chain of same-height panels as one continuous wide panel.
// CHAIN_TOP_LEFT_DOWN (tried earlier) is for 2D serpentine grids and was
// silently mirroring both X and Y before our mapping even ran, which
// explains both the reversed row order and the checkerboard pattern.
// Fully derived and empirically verified against every test data point:
// - The panel's C/D address lines are non-functional (confirmed via A/B,
//   B/C, and C/D swap tests all producing identical results).
// - Instead, bit 3 of the ELECTRICAL column address is a hidden row-select
//   bit that compensates for this: it must equal bit 2 of the true row
//   (TRUE_y), i.e. G = (TRUE_y >> 2) & 1.
// - All OTHER column bits map directly to the visible position: the low 3
//   bits stay put, and everything above bit 3 shifts up by one position
//   to make room for the inserted row-select bit.
// - The row itself passes through unchanged (0-15) - the driver's own
//   R1/R2 split at row 8 already works correctly on its own.
struct CustomFourScanMapping {
  static VirtualCoords apply(VirtualCoords coords, int panel_pixel_base) {
    int v = coords.x;                  // true visible column, 0-95
    int g = (coords.y >> 2) & 1;       // hidden row-select bit
    coords.x = (v & 7) | (g << 3) | ((v >> 3) << 4);
    // coords.y stays as the true row (0-15) - passed straight through.
    return coords;
  }
};

using FourScanVirtualPanel = VirtualMatrixPanel_T<CHAIN_NONE, CustomFourScanMapping>;

namespace esphome {
namespace four_scan_hub75 {

class FourScanHub75Display : public display::DisplayBuffer {
 public:
  void set_panel_width(int w) { this->panel_width_ = w; }
  void set_panel_height(int h) { this->panel_height_ = h; }
  void set_chain_length(int c) { this->chain_length_ = c; }
  void set_brightness(uint8_t b) { this->brightness_ = b; }
  void set_pins(int r1, int g1, int b1, int r2, int g2, int b2, int a, int b, int c, int d, int e,
                int lat, int oe, int clk) {
    this->pins_.r1 = r1;
    this->pins_.g1 = g1;
    this->pins_.b1 = b1;
    this->pins_.r2 = r2;
    this->pins_.g2 = g2;
    this->pins_.b2 = b2;
    this->pins_.a = a;
    this->pins_.b = b;
    this->pins_.c = c;
    this->pins_.d = d;
    this->pins_.e = e;
    this->pins_.lat = lat;
    this->pins_.oe = oe;
    this->pins_.clk = clk;
  }

  void setup() override;
  void update() override;
  void display() {}

  void set_runtime_brightness(uint8_t b) {
    if (this->dma_display_ != nullptr)
      this->dma_display_->setBrightness8(b);
  }

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  int get_width_internal() override { return this->panel_width_ * this->chain_length_; }
  int get_height_internal() override { return this->panel_height_; }
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

 protected:
  int panel_width_{32};
  int panel_height_{16};
  int chain_length_{1};
  uint8_t brightness_{128};
  HUB75_I2S_CFG::i2s_pins pins_;
  MatrixPanel_I2S_DMA *dma_display_{nullptr};
  FourScanVirtualPanel *virtual_display_{nullptr};
};

}  // namespace four_scan_hub75
}  // namespace esphome
