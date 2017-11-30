#pragma once

#include <stdint.h>
#include <string.h>

#include "Macros.h"

namespace ableton
{
  /*!
   *  Utility class representing a bitmap in the pixelformat used by push.
   *
   *  The display uses 16 bit per pixels in a 5:6:5 format
   *
   *      MSB                             LSB
   *        b b b b|b g g g|g g g r|r r r r
   */

  class Push2DisplayBitmap
  {
  public:

    using pixel_t = uint16_t;

    static const int16_t kWidth = 960;
    static const int16_t kHeight = 160;

    Push2DisplayBitmap()
    {
      memset(pixelData_, 0, kWidth * kHeight * sizeof(pixel_t));
    }

    /*!
     * \return the width of the bitmap
     */

    int GetWidth() const
    {
      return kWidth;
    }

    /*!
     * \return the height of the bitmap
     */

    int GetHeight() const
    {
      return kHeight;
    }

    /*!
     * \return pixel_t value in push display format from (r, g, b)
     */

    inline static pixel_t SPixelFromRGB(unsigned char r, unsigned char g, unsigned char b)
    {
      pixel_t pixel = (b & 0xF8) >> 3;
      pixel <<= 6;
      pixel += (g & 0xFC) >> 2;
      pixel <<= 5;
      pixel += (r & 0xF8) >> 3;
      return pixel;
    }

    /*!
     * \return a pointer to the internal pixel data
     */

    pixel_t* PixelData()
    {
      return pixelData_;
    }

    const pixel_t* PixelData(void *pDummy = nullptr) const
    {
      MUnused(pDummy);
      return pixelData_;
    }
  private:
    pixel_t pixelData_[kWidth * kHeight]; /*!< static memory block to store the pixel values */
  };

}