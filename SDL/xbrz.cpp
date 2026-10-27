#include "xbrz.h"
#include <algorithm>
#include <cmath>

namespace xbrz {

namespace {

// Perceptual color distance helpers
inline double colorDistance(uint32_t c1, uint32_t c2, ColorFormat colFmt) {
    if (c1 == c2) return 0.0;

    int r1, g1, b1, r2, g2, b2;
    if (colFmt == ColorFormat::ARGB) {
        r1 = (c1 >> 16) & 0xFF;
        g1 = (c1 >> 8) & 0xFF;
        b1 = c1 & 0xFF;

        r2 = (c2 >> 16) & 0xFF;
        g2 = (c2 >> 8) & 0xFF;
        b2 = c2 & 0xFF;
    } else {
        r1 = c1 & 0xFF;
        g1 = (c1 >> 8) & 0xFF;
        b1 = (c1 >> 16) & 0xFF;

        r2 = c2 & 0xFF;
        g2 = (c2 >> 8) & 0xFF;
        b2 = c2 & 0xFF;
    }

    // Standard YUV color weighting
    double y = (r1 - r2) * 0.299 + (g1 - g2) * 0.587 + (b1 - b2) * 0.114;
    double u = ((b1 - b2) - y) * 0.565;
    double v = ((r1 - r2) - y) * 0.713;

    return std::sqrt(y * y + u * u + v * v);
}

inline bool isColorSimilar(uint32_t c1, uint32_t c2, ColorFormat colFmt, double tolerance) {
    return colorDistance(c1, c2, colFmt) <= tolerance;
}

inline uint32_t alphaBlend2(uint32_t pix1, uint32_t pix2) {
    // 50/50 blend
    return (((pix1 ^ pix2) & 0xFEFEFEFE) >> 1) + (pix1 & pix2);
}

// 2x kernel corner blend
void scalePixel2x(const uint32_t* s, uint32_t* out, int outPitch, ColorFormat colFmt, const ScalerCfg& cfg) {
    /* 
       Source 3x3 context around center pixel 'E':
       A B C
       D E F
       G H I
    */
    uint32_t B = s[-1 * 3]; // Row above, same col
    uint32_t D = s[-1];     // Same row, col left
    uint32_t E = s[0];      // Center pixel
    uint32_t F = s[1];      // Same row, col right
    uint32_t H = s[3];      // Row below, same col

    // Target 2x2 quadrant outputs:
    // out[0]          out[1]
    // out[outPitch]   out[outPitch + 1]
    uint32_t e0 = E;
    uint32_t e1 = E;
    uint32_t e2 = E;
    uint32_t e3 = E;

    // Corner 1 (Top-Left)
    if (D == B && D != H && B != F) {
        e0 = alphaBlend2(E, D);
    }
    // Corner 2 (Top-Right)
    if (B == F && B != D && F != H) {
        e1 = alphaBlend2(E, F);
    }
    // Corner 3 (Bottom-Left)
    if (H == D && H != F && D != B) {
        e2 = alphaBlend2(E, D);
    }
    // Corner 4 (Bottom-Right)
    if (F == H && F != B && H != D) {
        e3 = alphaBlend2(E, F);
    }

    out[0] = e0;
    out[1] = e1;
    out[outPitch] = e2;
    out[outPitch + 1] = e3;
}

} // namespace

void scale(size_t factor, const uint32_t* src, uint32_t* trg, int srcWidth, int srcHeight, ColorFormat colFmt) {
    if (factor != 2 || !src || !trg || srcWidth <= 0 || srcHeight <= 0) return;

    ScalerCfg cfg;
    const int outPitch = srcWidth * 2;

    for (int y = 0; y < srcHeight; ++y) {
        const int yPrev = (y > 0) ? (y - 1) : 0;
        const int yNext = (y + 1 < srcHeight) ? (y + 1) : (srcHeight - 1);

        for (int x = 0; x < srcWidth; ++x) {
            const int xPrev = (x > 0) ? (x - 1) : 0;
            const int xNext = (x + 1 < srcWidth) ? (x + 1) : (srcWidth - 1);

            // Construct 3x3 local window around (x, y)
            uint32_t neighborhood[9] = {
                src[yPrev * srcWidth + xPrev], src[yPrev * srcWidth + x], src[yPrev * srcWidth + xNext],
                src[y * srcWidth + xPrev],     src[y * srcWidth + x],     src[y * srcWidth + xNext],
                src[yNext * srcWidth + xPrev], src[yNext * srcWidth + x], src[yNext * srcWidth + xNext]
            };

            uint32_t* out = trg + (y * 2) * outPitch + (x * 2);
            scalePixel2x(&neighborhood[4], out, outPitch, colFmt, cfg);
        }
    }
}

} // namespace xbrz
