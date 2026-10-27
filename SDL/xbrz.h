#ifndef XBRZ_H
#define XBRZ_H

#include <cstdint>
#include <cstddef>

namespace xbrz {

enum class ColorFormat {
    ARGB,
    RGBA
};

// Scaler configuration
struct ScalerCfg {
    double luminanceWeight = 1.0;
    double equalColorTolerance = 30.0;
    double dominantDirectionThreshold = 3.6;
    double steepDirectionThreshold = 2.2;
};

// 2x scaling function for 32-bit pixel buffers
void scale(size_t factor, const uint32_t* src, uint32_t* trg, int srcWidth, int srcHeight, ColorFormat colFmt);

} // namespace xbrz

#endif // XBRZ_H
