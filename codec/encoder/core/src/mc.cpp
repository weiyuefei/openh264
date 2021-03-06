/*!
 * \copy
 *     Copyright (c)  2009-2013, Cisco Systems
 *     All rights reserved.
 *
 *     Redistribution and use in source and binary forms, with or without
 *     modification, are permitted provided that the following conditions
 *     are met:
 *
 *        * Redistributions of source code must retain the above copyright
 *          notice, this list of conditions and the following disclaimer.
 *
 *        * Redistributions in binary form must reproduce the above copyright
 *          notice, this list of conditions and the following disclaimer in
 *          the documentation and/or other materials provided with the
 *          distribution.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *     "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *     LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *     FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *     COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *     BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *     CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *     LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *     ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *     POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * \file	mc.c
 *
 * \brief	Interfaces implementation for motion compensation
 *
 * \date	03/17/2009 Created
 *
 *************************************************************************************
 */

#include "mc.h"
#include "cpu_core.h"

namespace WelsEnc {
/*------------------weight for chroma fraction pixel interpolation------------------*/
//kuiA = (8 - dx) * (8 - dy);
//kuiB = dx * (8 - dy);
//kuiC = (8 - dx) * dy;
//kuiD = dx * dy
static const uint8_t g_kuiABCD[8][8][4] = { ////g_kuiA[dy][dx], g_kuiB[dy][dx], g_kuiC[dy][dx], g_kuiD[dy][dx]
  {
    {64, 0, 0, 0}, {56, 8, 0, 0}, {48, 16, 0, 0}, {40, 24, 0, 0},
    {32, 32, 0, 0}, {24, 40, 0, 0}, {16, 48, 0, 0}, {8, 56, 0, 0}
  },
  {
    {56, 0, 8, 0}, {49, 7, 7, 1}, {42, 14, 6, 2}, {35, 21, 5, 3},
    {28, 28, 4, 4}, {21, 35, 3, 5}, {14, 42, 2, 6}, {7, 49, 1, 7}
  },
  {
    {48, 0, 16, 0}, {42, 6, 14, 2}, {36, 12, 12, 4}, {30, 18, 10, 6},
    {24, 24, 8, 8}, {18, 30, 6, 10}, {12, 36, 4, 12}, {6, 42, 2, 14}
  },
  {
    {40, 0, 24, 0}, {35, 5, 21, 3}, {30, 10, 18, 6}, {25, 15, 15, 9},
    {20, 20, 12, 12}, {15, 25, 9, 15}, {10, 30, 6, 18}, {5, 35, 3, 21}
  },
  {
    {32, 0, 32, 0}, {28, 4, 28, 4}, {24, 8, 24, 8}, {20, 12, 20, 12},
    {16, 16, 16, 16}, {12, 20, 12, 20}, {8, 24, 8, 24}, {4, 28, 4, 28}
  },
  {
    {24, 0, 40, 0}, {21, 3, 35, 5}, {18, 6, 30, 10}, {15, 9, 25, 15},
    {12, 12, 20, 20}, {9, 15, 15, 25}, {6, 18, 10, 30}, {3, 21, 5, 35}
  },
  {
    {16, 0, 48, 0}, {14, 2, 42, 6}, {12, 4, 36, 12}, {10, 6, 30, 18},
    {8, 8, 24, 24}, {6, 10, 18, 30}, {4, 12, 12, 36}, {2, 14, 6, 42}
  },
  {
    {8, 0, 56, 0}, {7, 1, 49, 7}, {6, 2, 42, 14}, {5, 3, 35, 21},
    {4, 4, 28, 28}, {3, 5, 21, 35}, {2, 6, 14, 42}, {1, 7, 7, 49}
  }
};

//***************************************************************************//
//                          C code implementation                            //
//***************************************************************************//
static inline void McCopyWidthEq4_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                     int32_t iHeight) {
  int32_t i;
  for (i = 0; i < iHeight; i++) {
    memcpy (pDst, pSrc, 4);	// confirmed_safe_unsafe_usage
    pDst += iDstStride;
    pSrc += iSrcStride;
  }
}

static inline void McCopyWidthEq8_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                     int32_t iHeight)

{
  int32_t i;
  for (i = 0; i < iHeight; i++) {
    memcpy (pDst, pSrc, 8);	// confirmed_safe_unsafe_usage
    pDst += iDstStride;
    pSrc += iSrcStride;
  }
}
static inline void McCopyWidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                      int32_t iHeight) {
  int32_t i;
  for (i = 0; i < iHeight; i++) {
    memcpy (pDst, pSrc, 16);	// confirmed_safe_unsafe_usage
    pDst += iDstStride;
    pSrc += iSrcStride;
  }
}

//--------------------Luma sample MC------------------//
static inline int32_t HorFilter_c (const uint8_t* pSrc) {
  int32_t iPix05 = pSrc[-2] + pSrc[3];
  int32_t iPix14 = pSrc[-1] + pSrc[2];
  int32_t iPix23 = pSrc[ 0] + pSrc[1];

  return (iPix05 - ((iPix14 << 2) + iPix14) + (iPix23 << 4) + (iPix23 << 2));
}

static inline int32_t HorFilterInput16bit1_c (const int16_t* pSrc) {
  int32_t iPix05 = pSrc[0] + pSrc[5];
  int32_t iPix14 = pSrc[1] + pSrc[4];
  int32_t iPix23 = pSrc[2] + pSrc[3];

  return (iPix05 - ((iPix14 << 2) + iPix14) + (iPix23 << 4) + (iPix23 << 2));
}
static inline int32_t VerFilter_c (const uint8_t* pSrc, const int32_t kiSrcStride) {
  const int32_t kiLine1	= kiSrcStride;
  const int32_t kiLine2	= (kiSrcStride << 1);
  const int32_t kiLine3 = kiLine1 + kiLine2;
  const uint32_t kuiPix05 = * (pSrc - kiLine2) + * (pSrc + kiLine3);
  const uint32_t kuiPix14 = * (pSrc - kiLine1) + * (pSrc + kiLine2);
  const uint32_t kuiPix23 = * (pSrc) + * (pSrc + kiLine1);

  return (kuiPix05 - ((kuiPix14 << 2) + kuiPix14) + (kuiPix23 << 4) + (kuiPix23 << 2));
}

static inline void PixelAvgWidthEq8_c (uint8_t* pDst, int32_t iDstStride, const uint8_t* pSrcA, int32_t iSrcAStride,
                                       const uint8_t* pSrcB, int32_t iSrcBStride, int32_t iHeight) {
  int32_t i, j;
  for (i = 0; i < iHeight; i++) {
    for (j = 0; j < 8; j++) {
      pDst[j] = (pSrcA[j] + pSrcB[j] + 1) >> 1;
    }
    pDst  += iDstStride;
    pSrcA += iSrcAStride;
    pSrcB += iSrcBStride;
  }
}
static inline void PixelAvgWidthEq16_c (uint8_t* pDst, int32_t iDstStride, const uint8_t* pSrcA, int32_t iSrcAStride,
                                        const uint8_t* pSrcB, int32_t iSrcBStride, int32_t iHeight) {
  int32_t i, j;
  for (i = 0; i < iHeight; i++) {
    for (j = 0; j < 16; j++) {
      pDst[j] = (pSrcA[j] + pSrcB[j] + 1) >> 1;
    }
    pDst  += iDstStride;
    pSrcA += iSrcAStride;
    pSrcB += iSrcBStride;
  }
}

//horizontal filter to gain half sample, that is (2, 0) location in quarter sample
static inline void McHorVer20WidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  int32_t i, j;
  for (i = 0; i < iHeight; i++) {
    for (j = 0; j < 16; j++) {
      pDst[j] = WelsClip1 ((HorFilter_c (pSrc + j) + 16) >> 5);
    }
    pDst += iDstStride;
    pSrc += iSrcStride;
  }
}
//vertical filter to gain half sample, that is (0, 2) location in quarter sample
static inline void McHorVer02WidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  int32_t i, j;
  for (i = 0; i < iHeight; i++) {
    for (j = 0; j < 16; j++) {
      pDst[j] = WelsClip1 ((VerFilter_c (pSrc + j, iSrcStride) + 16) >> 5);
    }
    pDst += iDstStride;
    pSrc += iSrcStride;
  }
}
//horizontal and vertical filter to gain half sample, that is (2, 2) location in quarter sample
static inline void McHorVer22WidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  int16_t pTmp[16 + 5] = {0}; //16
  int32_t i, j, k;

  for (i = 0; i < iHeight; i++) {
    for (j = 0; j < 16 + 5; j++) {
      pTmp[j] = VerFilter_c (pSrc - 2 + j, iSrcStride);
    }
    for (k = 0; k < 16; k++) {
      pDst[k] = WelsClip1 ((HorFilterInput16bit1_c (&pTmp[k]) + 512) >> 10);
    }
    pSrc += iSrcStride;
    pDst += iDstStride;
  }
}

/////////////////////luma MC//////////////////////////

static inline void McHorVer01WidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 256, 16)

  McHorVer02WidthEq16_c (pSrc, iSrcStride, pTmp, 16, iHeight);
  PixelAvgWidthEq16_c (pDst, iDstStride, pSrc, iSrcStride, pTmp, 16, iHeight);
}
static inline void McHorVer03WidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 256, 16)

  McHorVer02WidthEq16_c (pSrc, iSrcStride, pTmp, 16, iHeight);
  PixelAvgWidthEq16_c (pDst, iDstStride, pSrc + iSrcStride, iSrcStride, pTmp, 16, iHeight);
}
static inline void McHorVer10WidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 256, 16)

  McHorVer20WidthEq16_c (pSrc, iSrcStride, pTmp, 16, iHeight);
  PixelAvgWidthEq16_c (pDst, iDstStride, pSrc, iSrcStride, pTmp, 16, iHeight);
}
static inline void McHorVer11WidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer20WidthEq16_c (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_c (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_c (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
static inline void McHorVer12WidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer02WidthEq16_c (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_c (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_c (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
static inline void McHorVer13WidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer20WidthEq16_c (pSrc + iSrcStride, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_c (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_c (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
static inline void McHorVer21WidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer20WidthEq16_c (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_c (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_c (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
static inline void McHorVer23WidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer20WidthEq16_c (pSrc + iSrcStride, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_c (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_c (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
static inline void McHorVer30WidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 256, 16)

  McHorVer20WidthEq16_c (pSrc, iSrcStride, pTmp, 16, iHeight);
  PixelAvgWidthEq16_c (pDst, iDstStride, pSrc + 1, iSrcStride, pTmp, 16, iHeight);
}
static inline void McHorVer31WidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer20WidthEq16_c (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_c (pSrc + 1, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_c (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
static inline void McHorVer32WidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer02WidthEq16_c (pSrc + 1, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_c (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_c (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
static inline void McHorVer33WidthEq16_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer20WidthEq16_c (pSrc + iSrcStride, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_c (pSrc + 1, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_c (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}

static inline void McHorVer20_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                 int32_t iWidth,
                                 int32_t iHeight) {
  int32_t i, j;
  for (i = 0; i < iHeight; i++) {
    for (j = 0; j < iWidth; j++) {
      pDst[j] = WelsClip1 ((HorFilter_c (pSrc + j) + 16) >> 5);
    }
    pDst += iDstStride;
    pSrc += iSrcStride;
  }
}
//vertical filter to gain half sample, that is (0, 2) location in quarter sample
static inline void McHorVer02_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                 int32_t iWidth,
                                 int32_t iHeight) {
  int32_t i, j;
  for (i = 0; i < iHeight; i++) {
    for (j = 0; j < iWidth; j++) {
      pDst[j] = WelsClip1 ((VerFilter_c (pSrc + j, iSrcStride) + 16) >> 5);
    }
    pDst += iDstStride;
    pSrc += iSrcStride;
  }
}
//horizontal and vertical filter to gain half sample, that is (2, 2) location in quarter sample
static inline void McHorVer22_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                 int32_t iWidth,
                                 int32_t iHeight) {
  int16_t pTmp[17 + 5] = {0}; //w+1
  int32_t i, j, k;

  for (i = 0; i < iHeight; i++) {
    for (j = 0; j < iWidth + 5; j++) {
      pTmp[j] = VerFilter_c (pSrc - 2 + j, iSrcStride);
    }
    for (k = 0; k < iWidth; k++) {
      pDst[k] = WelsClip1 ((HorFilterInput16bit1_c (&pTmp[k]) + 512) >> 10);
    }
    pSrc += iSrcStride;
    pDst += iDstStride;
  }
}
static inline void McCopy_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride, int32_t iWidth,
                             int32_t iHeight) {
  int32_t i;
  if (iWidth == 16)
    McCopyWidthEq16_c (pSrc, iSrcStride, pDst, iDstStride, iHeight);
  else if (iWidth == 8)
    McCopyWidthEq8_c (pSrc, iSrcStride, pDst, iDstStride, iHeight);
  else if (iWidth == 4)
    McCopyWidthEq4_c (pSrc, iSrcStride, pDst, iDstStride, iHeight);
  else {
    for (i = 0; i < iHeight; i++) {
      memcpy (pDst, pSrc, iWidth);	// confirmed_safe_unsafe_usage
      pDst += iDstStride;
      pSrc += iSrcStride;
    }
  }
}

void McChroma_c (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                 int16_t iMvX, int16_t iMvY, int32_t iWidth, int32_t iHeight)
//pSrc has been added the offset of mv
{
  const int32_t kiDx = iMvX & 0x07;
  const int32_t kiDy = iMvY & 0x07;

  if (0 == kiDx && 0 == kiDy) {
    McCopy_c (pSrc, iSrcStride, pDst, iDstStride, iWidth, iHeight);
  } else {
    const int32_t kiDA = g_kuiABCD[kiDy][kiDx][0];
    const int32_t kiDB = g_kuiABCD[kiDy][kiDx][1];
    const int32_t kiDC = g_kuiABCD[kiDy][kiDx][2];
    const int32_t kiDD = g_kuiABCD[kiDy][kiDx][3];

    int32_t i, j;

    const uint8_t* pSrcNext = pSrc + iSrcStride;

    for (i = 0; i < iHeight; i++) {
      for (j = 0; j < iWidth; j++) {
        pDst[j] = (kiDA * pSrc[j] + kiDB * pSrc[j + 1] + kiDC * pSrcNext[j] + kiDD * pSrcNext[j + 1] + 32) >> 6;
      }
      pDst += iDstStride;
      pSrc = pSrcNext;
      pSrcNext += iSrcStride;
    }
  }
}
//***************************************************************************//
//                       MMXEXT and SSE2 implementation                      //
//***************************************************************************//
#if defined(X86_ASM)

static inline void McHorVer22WidthEq8_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_2D (int16_t, pTap, 21, 8, 16)
  McHorVer22Width8HorFirst_sse2 (pSrc - 2, iSrcStride, (uint8_t*)pTap, 16, iHeight + 5);
  McHorVer22Width8VerLastAlign_sse2 ((uint8_t*)pTap, 16, pDst, iDstStride, 8, iHeight);
}

//2010.2.5

static inline void McHorVer02WidthEq16_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* PDst, int32_t iDstStride,
    int32_t iHeight) {
  McHorVer02WidthEq8_sse2 (pSrc,     iSrcStride, PDst,     iDstStride, iHeight);
  McHorVer02WidthEq8_sse2 (&pSrc[8], iSrcStride, &PDst[8], iDstStride, iHeight);
}
static inline void McHorVer22WidthEq16_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  McHorVer22WidthEq8_sse2 (pSrc,     iSrcStride, pDst,     iDstStride, iHeight);
  McHorVer22WidthEq8_sse2 (&pSrc[8], iSrcStride, &pDst[8], iDstStride, iHeight);
}
void McHorVer22Width9Or17Height9Or17_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iWidth,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_2D (int16_t, pTap, 22, 24, 16)
  int32_t tmp1 = 2 * (iWidth - 8);
  McHorVer22HorFirst_sse2 (pSrc - 2, iSrcStride, (uint8_t*)pTap, 48, iWidth, iHeight + 5);
  McHorVer22Width8VerLastAlign_sse2 ((uint8_t*)pTap,  48, pDst, iDstStride, iWidth - 1, iHeight);
  McHorVer22Width8VerLastUnAlign_sse2 ((uint8_t*)pTap + tmp1,  48, pDst + iWidth - 8, iDstStride, 8, iHeight);
}

static inline void McHorVer01WidthEq16_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 256, 16)

  McHorVer02WidthEq16_sse2 (pSrc, iSrcStride, pTmp, 16, iHeight);
  PixelAvgWidthEq16_sse2 (pDst, iDstStride, pSrc, iSrcStride, pTmp, 16, iHeight);
}
static inline void McHorVer03WidthEq16_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 256, 16)

  McHorVer02WidthEq16_sse2 (pSrc, iSrcStride, pTmp, 16, iHeight);
  PixelAvgWidthEq16_sse2 (pDst, iDstStride, pSrc + iSrcStride, iSrcStride, pTmp, 16, iHeight);
}
static inline void McHorVer10WidthEq16_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 256, 16)

  McHorVer20WidthEq16_sse2 (pSrc, iSrcStride, pTmp, 16, iHeight);
  PixelAvgWidthEq16_sse2 (pDst, iDstStride, pSrc, iSrcStride, pTmp, 16, iHeight);
}
static inline void McHorVer11WidthEq16_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer20WidthEq16_sse2 (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_sse2 (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_sse2 (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
static inline void McHorVer12WidthEq16_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer02WidthEq16_sse2 (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_sse2 (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_sse2 (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
static inline void McHorVer13WidthEq16_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer20WidthEq16_sse2 (pSrc + iSrcStride, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_sse2 (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_sse2 (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
static inline void McHorVer21WidthEq16_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer20WidthEq16_sse2 (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_sse2 (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_sse2 (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
static inline void McHorVer23WidthEq16_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer20WidthEq16_sse2 (pSrc + iSrcStride, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_sse2 (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_sse2 (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
static inline void McHorVer30WidthEq16_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 256, 16)

  McHorVer20WidthEq16_sse2 (pSrc, iSrcStride, pTmp, 16, iHeight);
  PixelAvgWidthEq16_sse2 (pDst, iDstStride, pSrc + 1, iSrcStride, pTmp, 16, iHeight);
}
static inline void McHorVer31WidthEq16_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer20WidthEq16_sse2 (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_sse2 (pSrc + 1, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_sse2 (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
static inline void McHorVer32WidthEq16_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer02WidthEq16_sse2 (pSrc + 1, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_sse2 (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_sse2 (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
static inline void McHorVer33WidthEq16_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)

  McHorVer20WidthEq16_sse2 (pSrc + iSrcStride, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_sse2 (pSrc + 1, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_sse2 (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}

static inline void McCopy_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                int32_t iWidth, int32_t iHeight) {
  int32_t i;
  if (iWidth == 16)
    McCopyWidthEq16_sse2 (pSrc, iSrcStride, pDst, iDstStride, iHeight);
  else if (iWidth == 8)
    McCopyWidthEq8_mmx (pSrc, iSrcStride, pDst, iDstStride, iHeight);
  else if (iWidth == 4)
    McCopyWidthEq4_mmx (pSrc, iSrcStride, pDst, iDstStride, iHeight);
  else {
    for (i = 0; i < iHeight; i++) {
      memcpy (pDst, pSrc, iWidth);	// confirmed_safe_unsafe_usage
      pDst += iDstStride;
      pSrc += iSrcStride;
    }
  }
}

typedef void (*McChromaWidthEqx) (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                  const uint8_t* pABCD, int32_t iHeigh);
void McChroma_sse2 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                    int16_t iMvX, int16_t iMvY, int32_t iWidth, int32_t iHeight) {
  const int32_t kiD8x = iMvX & 0x07;
  const int32_t kiD8y = iMvY & 0x07;
  static const McChromaWidthEqx kpfFuncs[2] = {
    McChromaWidthEq4_mmx,
    McChromaWidthEq8_sse2
  };

  if (0 == kiD8x && 0 == kiD8y) {
    McCopy_sse2 (pSrc, iSrcStride, pDst, iDstStride, iWidth, iHeight);
  } else {
    kpfFuncs[ (iWidth >> 3)] (pSrc, iSrcStride, pDst, iDstStride, g_kuiABCD[kiD8y][kiD8x], iHeight);
  }
}

void McChroma_ssse3 (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                     int16_t iMvX, int16_t iMvY, int32_t iWidth, int32_t iHeight) {
  const int32_t kiD8x = iMvX & 0x07;
  const int32_t kiD8y = iMvY & 0x07;

  static const McChromaWidthEqx kpfFuncs[2] = {
    McChromaWidthEq4_mmx,
    McChromaWidthEq8_ssse3
  };
  if (0 == kiD8x && 0 == kiD8y) {
    McCopy_sse2 (pSrc, iSrcStride, pDst, iDstStride, iWidth, iHeight);
  } else {
    kpfFuncs[ (iWidth >> 3)] (pSrc, iSrcStride, pDst, iDstStride, g_kuiABCD[kiD8y][kiD8x], iHeight);
  }

}

#endif //X86_ASM

//***************************************************************************//
//                       NEON implementation                      //
//***************************************************************************//
#if defined(HAVE_NEON)
void McHorVer20Width9Or17_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                int32_t iWidth, int32_t iHeight) {
  if (iWidth == 17)
    McHorVer20Width17_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
  else //if (iWidth == 9)
    McHorVer20Width9_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
}
void McHorVer02Height9Or17_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                 int32_t iWidth, int32_t iHeight) {
  if (iWidth == 16)
    McHorVer02Height17_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
  else //if (iWidth == 8)
    McHorVer02Height9_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
}
void McHorVer22Width9Or17Height9Or17_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iWidth, int32_t iHeight) {
  if (iWidth == 17)
    McHorVer22Width17_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
  else //if (iWidth == 9)
    McHorVer22Width9_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
}
void EncMcHorVer11_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride, int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer20WidthEq16_neon (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_neon (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_neon (pDst, iDstStride, pTmp, &pTmp[256], iHeight);
}
void EncMcHorVer12_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride, int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer02WidthEq16_neon (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_neon (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_neon (pDst, iDstStride, pTmp, &pTmp[256], iHeight);
}
void EncMcHorVer13_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride, int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer20WidthEq16_neon (pSrc + iSrcStride, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_neon (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_neon (pDst, iDstStride, pTmp, &pTmp[256], iHeight);
}
void EncMcHorVer21_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride, int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer20WidthEq16_neon (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_neon (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_neon (pDst, iDstStride, pTmp, &pTmp[256], iHeight);
}
void EncMcHorVer23_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride, int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer20WidthEq16_neon (pSrc + iSrcStride, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_neon (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_neon (pDst, iDstStride, pTmp, &pTmp[256], iHeight);
}
void EncMcHorVer31_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride, int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer20WidthEq16_neon (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_neon (pSrc + 1, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_neon (pDst, iDstStride, pTmp, &pTmp[256], iHeight);
}
void EncMcHorVer32_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride, int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer02WidthEq16_neon (pSrc + 1, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_neon (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_neon (pDst, iDstStride, pTmp, &pTmp[256], iHeight);
}
void EncMcHorVer33_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride, int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer20WidthEq16_neon (pSrc + iSrcStride, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_neon (pSrc + 1, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_neon (pDst, iDstStride, pTmp, &pTmp[256], iHeight);
}
void EncMcChroma_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                       int16_t iMvX, int16_t iMvY, int32_t iWidth, int32_t iHeight) {
  const int32_t kiD8x = iMvX & 0x07;
  const int32_t kiD8y = iMvY & 0x07;
  if (0 == kiD8x && 0 == kiD8y) {
    if (8 == iWidth)
      McCopyWidthEq8_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
    else // iWidth == 4
      McCopyWidthEq4_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
  } else {
    if (8 == iWidth)
      McChromaWidthEq8_neon (pSrc, iSrcStride, pDst, iDstStride, (int32_t*) (g_kuiABCD[kiD8y][kiD8x]), iHeight);
    else //if(4 == iWidth)
      McChromaWidthEq4_neon (pSrc, iSrcStride, pDst, iDstStride, (int32_t*) (g_kuiABCD[kiD8y][kiD8x]), iHeight);
  }
}
#endif

#if defined(HAVE_NEON_AARCH64)
void McHorVer20Width9Or17_AArch64_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                        int32_t iWidth, int32_t iHeight) {
  if (iWidth == 17)
    McHorVer20Width17_AArch64_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
  else //if (iWidth == 9)
    McHorVer20Width9_AArch64_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
}
void McHorVer02Height9Or17_AArch64_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
    int32_t iWidth, int32_t iHeight) {
  if (iWidth == 16)
    McHorVer02Height17_AArch64_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
  else //if (iWidth == 8)
    McHorVer02Height9_AArch64_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
}
void McHorVer22Width9Or17Height9Or17_AArch64_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst,
    int32_t iDstStride,
    int32_t iWidth, int32_t iHeight) {
  if (iWidth == 17)
    McHorVer22Width17_AArch64_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
  else //if (iWidth == 9)
    McHorVer22Width9_AArch64_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
}
void EncMcHorVer11_AArch64_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                 int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer20WidthEq16_AArch64_neon (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_AArch64_neon (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_AArch64_neon (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
void EncMcHorVer12_AArch64_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                 int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer02WidthEq16_AArch64_neon (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_AArch64_neon (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_AArch64_neon (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
void EncMcHorVer13_AArch64_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                 int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer20WidthEq16_AArch64_neon (pSrc + iSrcStride, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_AArch64_neon (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_AArch64_neon (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
void EncMcHorVer21_AArch64_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                 int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer20WidthEq16_AArch64_neon (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_AArch64_neon (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_AArch64_neon (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
void EncMcHorVer23_AArch64_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                 int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer20WidthEq16_AArch64_neon (pSrc + iSrcStride, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_AArch64_neon (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_AArch64_neon (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
void EncMcHorVer31_AArch64_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                 int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer20WidthEq16_AArch64_neon (pSrc, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_AArch64_neon (pSrc + 1, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_AArch64_neon (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
void EncMcHorVer32_AArch64_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                 int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer02WidthEq16_AArch64_neon (pSrc + 1, iSrcStride, pTmp, 16, iHeight);
  McHorVer22WidthEq16_AArch64_neon (pSrc, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_AArch64_neon (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
void EncMcHorVer33_AArch64_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                                 int32_t iHeight) {
  ENFORCE_STACK_ALIGN_1D (uint8_t, pTmp, 512, 16)
  McHorVer20WidthEq16_AArch64_neon (pSrc + iSrcStride, iSrcStride, pTmp, 16, iHeight);
  McHorVer02WidthEq16_AArch64_neon (pSrc + 1, iSrcStride, &pTmp[256], 16, iHeight);
  PixelAvgWidthEq16_AArch64_neon (pDst, iDstStride, pTmp, 16, &pTmp[256], 16, iHeight);
}
void EncMcChroma_AArch64_neon (const uint8_t* pSrc, int32_t iSrcStride, uint8_t* pDst, int32_t iDstStride,
                               int16_t iMvX, int16_t iMvY, int32_t iWidth, int32_t iHeight) {
  const int32_t kiD8x = iMvX & 0x07;
  const int32_t kiD8y = iMvY & 0x07;
  if (0 == kiD8x && 0 == kiD8y) {
    if (8 == iWidth)
      McCopyWidthEq8_AArch64_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
    else // iWidth == 4
      McCopyWidthEq4_AArch64_neon (pSrc, iSrcStride, pDst, iDstStride, iHeight);
  } else {
    if (8 == iWidth)
      McChromaWidthEq8_AArch64_neon (pSrc, iSrcStride, pDst, iDstStride, (int32_t*) (g_kuiABCD[kiD8y][kiD8x]), iHeight);
    else //if(4 == iWidth)
      McChromaWidthEq4_AArch64_neon (pSrc, iSrcStride, pDst, iDstStride, (int32_t*) (g_kuiABCD[kiD8y][kiD8x]), iHeight);
  }
}
#endif

void WelsInitMcFuncs (SMcFunc* pMcFuncs, uint32_t uiCpuFlag) {
  static const PWelsSampleAveragingFunc pfPixAvgFunc[2] = {PixelAvgWidthEq8_c, PixelAvgWidthEq16_c};

  static const PWelsLumaQuarpelMcFunc pWelsMcFuncWidthEq16[16] = { //[y*4+x]
    McCopyWidthEq16_c,     McHorVer10WidthEq16_c, McHorVer20WidthEq16_c, McHorVer30WidthEq16_c,
    McHorVer01WidthEq16_c, McHorVer11WidthEq16_c, McHorVer21WidthEq16_c, McHorVer31WidthEq16_c,
    McHorVer02WidthEq16_c, McHorVer12WidthEq16_c, McHorVer22WidthEq16_c, McHorVer32WidthEq16_c,
    McHorVer03WidthEq16_c, McHorVer13WidthEq16_c, McHorVer23WidthEq16_c, McHorVer33WidthEq16_c
  };
#if defined (X86_ASM)
  static const PWelsLumaQuarpelMcFunc pWelsMcFuncWidthEq16_sse2[16] = {
    McCopyWidthEq16_sse2,     McHorVer10WidthEq16_sse2, McHorVer20WidthEq16_sse2, McHorVer30WidthEq16_sse2,
    McHorVer01WidthEq16_sse2, McHorVer11WidthEq16_sse2, McHorVer21WidthEq16_sse2, McHorVer31WidthEq16_sse2,
    McHorVer02WidthEq16_sse2, McHorVer12WidthEq16_sse2, McHorVer22WidthEq16_sse2, McHorVer32WidthEq16_sse2,
    McHorVer03WidthEq16_sse2, McHorVer13WidthEq16_sse2, McHorVer23WidthEq16_sse2, McHorVer33WidthEq16_sse2
  };
#endif
#if defined(HAVE_NEON)
  static const PWelsLumaQuarpelMcFunc pWelsMcFuncWidthEq16_neon[16] = { //[x][y]
    McCopyWidthEq16_neon,        McHorVer10WidthEq16_neon,   McHorVer20WidthEq16_neon,    McHorVer30WidthEq16_neon,
    McHorVer01WidthEq16_neon,    EncMcHorVer11_neon,         EncMcHorVer21_neon,          EncMcHorVer31_neon,
    McHorVer02WidthEq16_neon,    EncMcHorVer12_neon,         McHorVer22WidthEq16_neon,    EncMcHorVer32_neon,
    McHorVer03WidthEq16_neon,    EncMcHorVer13_neon,         EncMcHorVer23_neon,          EncMcHorVer33_neon
  };
#endif
#if defined(HAVE_NEON_AARCH64)
  static const PWelsLumaQuarpelMcFunc pWelsMcFuncWidthEq16_AArch64_neon[16] = { //[x][y]
    McCopyWidthEq16_AArch64_neon,        McHorVer10WidthEq16_AArch64_neon,   McHorVer20WidthEq16_AArch64_neon,    McHorVer30WidthEq16_AArch64_neon,
    McHorVer01WidthEq16_AArch64_neon,    EncMcHorVer11_AArch64_neon,         EncMcHorVer21_AArch64_neon,          EncMcHorVer31_AArch64_neon,
    McHorVer02WidthEq16_AArch64_neon,    EncMcHorVer12_AArch64_neon,         McHorVer22WidthEq16_AArch64_neon,    EncMcHorVer32_AArch64_neon,
    McHorVer03WidthEq16_AArch64_neon,    EncMcHorVer13_AArch64_neon,         EncMcHorVer23_AArch64_neon,          EncMcHorVer33_AArch64_neon
  };
#endif
  pMcFuncs->pfLumaHalfpelHor = McHorVer20_c;
  pMcFuncs->pfLumaHalfpelVer = McHorVer02_c;
  pMcFuncs->pfLumaHalfpelCen = McHorVer22_c;
  memcpy (pMcFuncs->pfSampleAveraging, pfPixAvgFunc, sizeof (pfPixAvgFunc));
  pMcFuncs->pfChromaMc	= McChroma_c;
  memcpy (pMcFuncs->pfLumaQuarpelMc, pWelsMcFuncWidthEq16, sizeof (pWelsMcFuncWidthEq16));
#if defined (X86_ASM)
  if (uiCpuFlag & WELS_CPU_SSE2) {
    pMcFuncs->pfLumaHalfpelHor = McHorVer20Width9Or17_sse2;
    pMcFuncs->pfLumaHalfpelVer = McHorVer02Height9Or17_sse2;
    pMcFuncs->pfLumaHalfpelCen = McHorVer22Width9Or17Height9Or17_sse2;
    pMcFuncs->pfSampleAveraging[0] = PixelAvgWidthEq8_mmx;
    pMcFuncs->pfSampleAveraging[1] = PixelAvgWidthEq16_sse2;
    pMcFuncs->pfChromaMc = McChroma_sse2;
    memcpy (pMcFuncs->pfLumaQuarpelMc, pWelsMcFuncWidthEq16_sse2, sizeof (pWelsMcFuncWidthEq16_sse2));
  }

  if (uiCpuFlag & WELS_CPU_SSSE3) {
    pMcFuncs->pfChromaMc = McChroma_ssse3;
  }

#endif //(X86_ASM)

#if defined(HAVE_NEON)
  if (uiCpuFlag & WELS_CPU_NEON) {
    memcpy (pMcFuncs->pfLumaQuarpelMc, pWelsMcFuncWidthEq16_neon, sizeof (pWelsMcFuncWidthEq16_neon));
    pMcFuncs->pfChromaMc	= EncMcChroma_neon;
    pMcFuncs->pfSampleAveraging[0] = PixStrideAvgWidthEq8_neon;
    pMcFuncs->pfSampleAveraging[1] = PixStrideAvgWidthEq16_neon;
    pMcFuncs->pfLumaHalfpelHor = McHorVer20Width9Or17_neon;//iWidth+1:8/16
    pMcFuncs->pfLumaHalfpelVer = McHorVer02Height9Or17_neon;//heigh+1:8/16
    pMcFuncs->pfLumaHalfpelCen = McHorVer22Width9Or17Height9Or17_neon;//iWidth+1/heigh+1
  }
#endif
#if defined(HAVE_NEON_AARCH64)
  if (uiCpuFlag & WELS_CPU_NEON) {
    memcpy (pMcFuncs->pfLumaQuarpelMc, pWelsMcFuncWidthEq16_AArch64_neon,
            sizeof (pWelsMcFuncWidthEq16_AArch64_neon));
    pMcFuncs->pfChromaMc	= EncMcChroma_AArch64_neon;
    pMcFuncs->pfSampleAveraging[0] = PixStrideAvgWidthEq8_AArch64_neon;
    pMcFuncs->pfSampleAveraging[1] = PixStrideAvgWidthEq16_AArch64_neon;
    pMcFuncs->pfLumaHalfpelHor = McHorVer20Width9Or17_AArch64_neon;//iWidth+1:8/16
    pMcFuncs->pfLumaHalfpelVer = McHorVer02Height9Or17_AArch64_neon;//heigh+1:8/16
    pMcFuncs->pfLumaHalfpelCen = McHorVer22Width9Or17Height9Or17_AArch64_neon;//iWidth+1/heigh+1
  }
#endif
}
}
