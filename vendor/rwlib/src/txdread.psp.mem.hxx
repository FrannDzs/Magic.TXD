/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.psp.mem.hxx
*  PURPOSE:     Defines for memory management on the PlayStation Portable platform.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifdef RWLIB_INCLUDE_NATIVETEX_PSP

#include "txdread.memcodec.hxx"

#include "txdread.ps2gsman.hxx"

namespace rw
{

namespace pspMemoryPermutationData
{
    // This is the main permutation definition of the PSP.
    // I got it from some unprofessional website, so no authenticity guarrantee.
    // Lets just disregard this for now, it is wrong for Leeds PSP native texture anyway.
    const static uint32 psp_permute_desc[8][32] =
    {
        {   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47 },
        {  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111 },
        { 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175 },
        { 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239 },
        {  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63 },
        {  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127 },
        { 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191 },
        { 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255 }
    };

#if 0
    // Used for encoding PSMCT32 data on the PSP handheld.
    // permutationStride: 4
    const static uint32 psp_psmct32_packing_desc[8][16] =
    {
        {  0,  8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88,  96, 104, 112, 120 },
        {  1,  9, 17, 25, 33, 41, 49, 57, 65, 73, 81, 89,  97, 105, 113, 121 },
        {  2, 10, 18, 26, 34, 42, 50, 58, 66, 74, 82, 90,  98, 106, 114, 122 },
        {  3, 11, 19, 27, 35, 43, 51, 59, 67, 75, 83, 91,  99, 107, 115, 123 },
        {  4, 12, 20, 28, 36, 44, 52, 60, 68, 76, 84, 92, 100, 108, 116, 124 },
        {  5, 13, 21, 29, 37, 45, 53, 61, 69, 77, 85, 93, 101, 109, 117, 125 },
        {  6, 14, 22, 30, 38, 46, 54, 62, 70, 78, 86, 94, 102, 110, 118, 126 },
        {  7, 15, 23, 31, 39, 47, 55, 63, 71, 79, 87, 95, 103, 111, 119, 127 }
    };

    constexpr static uint32 psp_psmct32_packing_width = 16;
    constexpr static uint32 psp_psmct32_packing_height = 8;

    constexpr static uint32 psp_psmct32_permDepth = 128;

    constexpr static bool psp_psmct32_perm_isPackingConvention = false;
#endif
};

};

#endif //RWLIB_INCLUDE_NATIVETEX_PSP