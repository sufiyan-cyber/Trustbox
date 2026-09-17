/**
 * ============================================================================
 * PAYTM TRUSTBOX: FRAUD-PROOF UPI VERIFICATION DEVICE
 * ============================================================================
 * SINGLE-FILE ESP32 FIRMWARE (100% STANDALONE - ZERO QR LIBRARY NEEDED)
 * 
 * Hardware Modules:
 * - ESP32 DevKit (WROOM-32)
 * - 3.2" SPI TFT LCD (ILI9341, 320x240) + XPT2046 Touch
 * - Buzzer on Pin 26 (Official Paytm Chime + Fraud Siren)
 * - Status LED on Pin 25
 * - Sound / Microphone Module (MIC_D0 on 35, MIC_A0 on 34)
 * - Dynamic QR Code on Display (Self-Contained Embedded QR Engine)
 * 
 * Required Arduino Libraries (Only 3 Standard Libraries to install!):
 * 1. "Adafruit ILI9341" by Adafruit
 * 2. "Adafruit GFX Library" by Adafruit
 * 3. "XPT2046_Touchscreen" by Paul Stoffregen
 * (NO QRCode library installation needed - embedded directly in this file!)
 * ============================================================================
 */

#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

// ============================================================================
// EMBEDDED QR CODE GENERATOR (Self-Contained - No External QR Library Needed)
// ============================================================================
/**
 * The MIT License (MIT)
 *
 * This library is written and maintained by Richard Moore.
 * Major parts were derived from Project Nayuki's library.
 *
 * Copyright (c) 2017 Richard Moore     (https://github.com/ricmoo/TB_QRCode)
 * Copyright (c) 2017 Project Nayuki    (https://www.nayuki.io/page/qr-code-generator-library)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 *  Special thanks to Nayuki (https://www.nayuki.io/) from which this library was
 *  heavily inspired and compared against.
 *
 *  See: https://github.com/nayuki/QR-Code-generator/tree/master/cpp
 */


#ifndef __TB_EMBEDDED_QR_H_
#define __TB_EMBEDDED_QR_H_

#ifndef __cplusplus
typedef unsigned char bool;
static const bool false = 0;
static const bool true = 1;
#endif

#include <stdint.h>


// QR Code Format Encoding
#define TB_MODE_NUMERIC        0
#define TB_MODE_ALPHANUMERIC   1
#define TB_MODE_BYTE           2


// Error Correction Code Levels
#define TB_ECC_LOW            0
#define TB_ECC_MEDIUM         1
#define TB_ECC_QUARTILE       2
#define TB_ECC_HIGH           3


// If set to non-zero, this library can ONLY produce QR codes at that version
// This saves a lot of dynamic memory, as the codeword tables are skipped
#ifndef LOCK_VERSION
#define LOCK_VERSION       0
#endif


typedef struct TB_QRCode {
    uint8_t version;
    uint8_t size;
    uint8_t ecc;
    uint8_t mode;
    uint8_t mask;
    uint8_t *modules;
} TB_QRCode;


#ifdef __cplusplus
extern "C"{
#endif  /* __cplusplus */



uint16_t tb_qrcode_getBufferSize(uint8_t version);

int8_t tb_qrcode_initText(TB_QRCode *qrcode, uint8_t *modules, uint8_t version, uint8_t ecc, const char *data);
int8_t tb_qrcode_initBytes(TB_QRCode *qrcode, uint8_t *modules, uint8_t version, uint8_t ecc, uint8_t *data, uint16_t length);

bool tb_qrcode_getModule(TB_QRCode *qrcode, uint8_t x, uint8_t y);



#ifdef __cplusplus
}
#endif  /* __cplusplus */


#endif  /* __TB_EMBEDDED_QR_H_ */

/**
 * The MIT License (MIT)
 *
 * This library is written and maintained by Richard Moore.
 * Major parts were derived from Project Nayuki's library.
 *
 * Copyright (c) 2017 Richard Moore     (https://github.com/ricmoo/TB_QRCode)
 * Copyright (c) 2017 Project Nayuki    (https://www.nayuki.io/page/qr-code-generator-library)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 *  Special thanks to Nayuki (https://www.nayuki.io/) from which this library was
 *  heavily inspired and compared against.
 *
 *  See: https://github.com/nayuki/QR-Code-generator/tree/master/cpp
 */



#include <stdlib.h>
#include <string.h>

#pragma mark - Error Correction Lookup tables

#if LOCK_VERSION == 0

static const uint16_t NUM_ERROR_CORRECTION_CODEWORDS[4][40] = {
    // 1,  2,  3,  4,  5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,   25,   26,   27,   28,   29,   30,   31,   32,   33,   34,   35,   36,   37,   38,   39,   40    Error correction level
    { 10, 16, 26, 36, 48,  64,  72,  88, 110, 130, 150, 176, 198, 216, 240, 280, 308, 338, 364, 416, 442, 476, 504, 560,  588,  644,  700,  728,  784,  812,  868,  924,  980, 1036, 1064, 1120, 1204, 1260, 1316, 1372},  // Medium
    {  7, 10, 15, 20, 26,  36,  40,  48,  60,  72,  80,  96, 104, 120, 132, 144, 168, 180, 196, 224, 224, 252, 270, 300,  312,  336,  360,  390,  420,  450,  480,  510,  540,  570,  570,  600,  630,  660,  720,  750},  // Low
    { 17, 28, 44, 64, 88, 112, 130, 156, 192, 224, 264, 308, 352, 384, 432, 480, 532, 588, 650, 700, 750, 816, 900, 960, 1050, 1110, 1200, 1260, 1350, 1440, 1530, 1620, 1710, 1800, 1890, 1980, 2100, 2220, 2310, 2430},  // High
    { 13, 22, 36, 52, 72,  96, 108, 132, 160, 192, 224, 260, 288, 320, 360, 408, 448, 504, 546, 600, 644, 690, 750, 810,  870,  952, 1020, 1050, 1140, 1200, 1290, 1350, 1440, 1530, 1590, 1680, 1770, 1860, 1950, 2040},  // Quartile
};

static const uint8_t NUM_ERROR_CORRECTION_BLOCKS[4][40] = {
    // Version: (note that index 0 is for padding, and is set to an illegal value)
    // 1, 2, 3, 4, 5, 6, 7, 8, 9,10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40    Error correction level
    {  1, 1, 1, 2, 2, 4, 4, 4, 5, 5,  5,  8,  9,  9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},  // Medium
    {  1, 1, 1, 1, 1, 2, 2, 2, 2, 4,  4,  4,  4,  4,  6,  6,  6,  6,  7,  8,  8,  9,  9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},  // Low
    {  1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},  // High
    {  1, 1, 2, 2, 4, 4, 6, 6, 8, 8,  8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},  // Quartile
};

static const uint16_t NUM_RAW_DATA_MODULES[40] = {
    //  1,   2,   3,   4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   15,   16,   17,
      208, 359, 567, 807, 1079, 1383, 1568, 1936, 2336, 2768, 3232, 3728, 4256, 4651, 5243, 5867, 6523,
    //   18,   19,   20,   21,    22,    23,    24,    25,   26,    27,     28,    29,    30,    31,
       7211, 7931, 8683, 9252, 10068, 10916, 11796, 12708, 13652, 14628, 15371, 16411, 17483, 18587,
    //    32,    33,    34,    35,    36,    37,    38,    39,    40
       19723, 20891, 22091, 23008, 24272, 25568, 26896, 28256, 29648
};

// @TODO: Put other LOCK_VERSIONS here
#elif LOCK_VERSION == 3

static const int16_t NUM_ERROR_CORRECTION_CODEWORDS[4] = {
    26, 15, 44, 36
};

static const int8_t NUM_ERROR_CORRECTION_BLOCKS[4] = {
    1, 1, 2, 2
};

static const uint16_t NUM_RAW_DATA_MODULES = 567;

#else

#error Unsupported LOCK_VERSION (add it...)

#endif


static int max(int a, int b) {
    if (a > b) { return a; }
    return b;
}

/*
static int abs(int value) {
    if (value < 0) { return -value; }
    return value;
}
*/


#pragma mark - Mode testing and conversion

static int8_t getAlphanumeric(char c) {
    
    if (c >= '0' && c <= '9') { return (c - '0'); }
    if (c >= 'A' && c <= 'Z') { return (c - 'A' + 10); }
    
    switch (c) {
        case ' ': return 36;
        case '$': return 37;
        case '%': return 38;
        case '*': return 39;
        case '+': return 40;
        case '-': return 41;
        case '.': return 42;
        case '/': return 43;
        case ':': return 44;
    }
    
    return -1;
}

static bool isAlphanumeric(const char *text, uint16_t length) {
    while (length != 0) {
        if (getAlphanumeric(text[--length]) == -1) { return false; }
    }
    return true;
}


static bool isNumeric(const char *text, uint16_t length) {
    while (length != 0) {
        char c = text[--length];
        if (c < '0' || c > '9') { return false; }
    }
    return true;
}


#pragma mark - Counting

// We store the following tightly packed (less 8) in modeInfo
//               <=9  <=26  <= 40
// NUMERIC      ( 10,   12,    14);
// ALPHANUMERIC (  9,   11,    13);
// BYTE         (  8,   16,    16);
static char getModeBits(uint8_t version, uint8_t mode) {
    // Note: We use 15 instead of 16; since 15 doesn't exist and we cannot store 16 (8 + 8) in 3 bits
    // hex(int("".join(reversed([('00' + bin(x - 8)[2:])[-3:] for x in [10, 9, 8, 12, 11, 15, 14, 13, 15]])), 2))
    unsigned int modeInfo = 0x7bbb80a;
    
#if LOCK_VERSION == 0 || LOCK_VERSION > 9
    if (version > 9) { modeInfo >>= 9; }
#endif
    
#if LOCK_VERSION == 0 || LOCK_VERSION > 26
    if (version > 26) { modeInfo >>= 9; }
#endif
    
    char result = 8 + ((modeInfo >> (3 * mode)) & 0x07);
    if (result == 15) { result = 16; }
    
    return result;
}


#pragma mark - BitBucket

typedef struct BitBucket {
    uint32_t bitOffsetOrWidth;
    uint16_t capacityBytes;
    uint8_t *data;
} BitBucket;

/*
void bb_dump(BitBucket *bitBuffer) {
    printf("Buffer: ");
    for (uint32_t i = 0; i < bitBuffer->capacityBytes; i++) {
        printf("%02x", bitBuffer->data[i]);
        if ((i % 4) == 3) { printf(" "); }
    }
    printf("\n");
}
*/

static uint16_t bb_getGridSizeBytes(uint8_t size) {
    return (((size * size) + 7) / 8);
}

static uint16_t bb_getBufferSizeBytes(uint32_t bits) {
    return ((bits + 7) / 8);
}

static void bb_initBuffer(BitBucket *bitBuffer, uint8_t *data, int32_t capacityBytes) {
    bitBuffer->bitOffsetOrWidth = 0;
    bitBuffer->capacityBytes = capacityBytes;
    bitBuffer->data = data;
    
    memset(data, 0, bitBuffer->capacityBytes);
}

static void bb_initGrid(BitBucket *bitGrid, uint8_t *data, uint8_t size) {
    bitGrid->bitOffsetOrWidth = size;
    bitGrid->capacityBytes = bb_getGridSizeBytes(size);
    bitGrid->data = data;

    memset(data, 0, bitGrid->capacityBytes);
}

static void bb_appendBits(BitBucket *bitBuffer, uint32_t val, uint8_t length) {
    uint32_t offset = bitBuffer->bitOffsetOrWidth;
    for (int8_t i = length - 1; i >= 0; i--, offset++) {
        bitBuffer->data[offset >> 3] |= ((val >> i) & 1) << (7 - (offset & 7));
    }
    bitBuffer->bitOffsetOrWidth = offset;
}
/*
void bb_setBits(BitBucket *bitBuffer, uint32_t val, int offset, uint8_t length) {
    for (int8_t i = length - 1; i >= 0; i--, offset++) {
        bitBuffer->data[offset >> 3] |= ((val >> i) & 1) << (7 - (offset & 7));
    }
}
*/
static void bb_setBit(BitBucket *bitGrid, uint8_t x, uint8_t y, bool on) {
    uint32_t offset = y * bitGrid->bitOffsetOrWidth + x;
    uint8_t mask = 1 << (7 - (offset & 0x07));
    if (on) {
        bitGrid->data[offset >> 3] |= mask;
    } else {
        bitGrid->data[offset >> 3] &= ~mask;
    }
}

static void bb_invertBit(BitBucket *bitGrid, uint8_t x, uint8_t y, bool invert) {
    uint32_t offset = y * bitGrid->bitOffsetOrWidth + x;
    uint8_t mask = 1 << (7 - (offset & 0x07));
    bool on = ((bitGrid->data[offset >> 3] & (1 << (7 - (offset & 0x07)))) != 0);
    if (on ^ invert) {
        bitGrid->data[offset >> 3] |= mask;
    } else {
        bitGrid->data[offset >> 3] &= ~mask;
    }
}

static bool bb_getBit(BitBucket *bitGrid, uint8_t x, uint8_t y) {
    uint32_t offset = y * bitGrid->bitOffsetOrWidth + x;
    return (bitGrid->data[offset >> 3] & (1 << (7 - (offset & 0x07)))) != 0;
}


#pragma mark - Drawing Patterns

// XORs the data modules in this QR Code with the given mask pattern. Due to XOR's mathematical
// properties, calling applyMask(m) twice with the same value is equivalent to no change at all.
// This means it is possible to apply a mask, undo it, and try another mask. Note that a final
// well-formed QR Code symbol needs exactly one mask applied (not zero, not two, etc.).
static void applyMask(BitBucket *modules, BitBucket *isFunction, uint8_t mask) {
    uint8_t size = modules->bitOffsetOrWidth;
    
    for (uint8_t y = 0; y < size; y++) {
        for (uint8_t x = 0; x < size; x++) {
            if (bb_getBit(isFunction, x, y)) { continue; }
            
            bool invert = 0;
            switch (mask) {
                case 0:  invert = (x + y) % 2 == 0;                    break;
                case 1:  invert = y % 2 == 0;                          break;
                case 2:  invert = x % 3 == 0;                          break;
                case 3:  invert = (x + y) % 3 == 0;                    break;
                case 4:  invert = (x / 3 + y / 2) % 2 == 0;            break;
                case 5:  invert = x * y % 2 + x * y % 3 == 0;          break;
                case 6:  invert = (x * y % 2 + x * y % 3) % 2 == 0;    break;
                case 7:  invert = ((x + y) % 2 + x * y % 3) % 2 == 0;  break;
            }
            bb_invertBit(modules, x, y, invert);
        }
    }
}

static void setFunctionModule(BitBucket *modules, BitBucket *isFunction, uint8_t x, uint8_t y, bool on) {
    bb_setBit(modules, x, y, on);
    bb_setBit(isFunction, x, y, true);
}

// Draws a 9*9 finder pattern including the border separator, with the center module at (x, y).
static void drawFinderPattern(BitBucket *modules, BitBucket *isFunction, uint8_t x, uint8_t y) {
    uint8_t size = modules->bitOffsetOrWidth;

    for (int8_t i = -4; i <= 4; i++) {
        for (int8_t j = -4; j <= 4; j++) {
            uint8_t dist = max(abs(i), abs(j));  // Chebyshev/infinity norm
            int16_t xx = x + j, yy = y + i;
            if (0 <= xx && xx < size && 0 <= yy && yy < size) {
                setFunctionModule(modules, isFunction, xx, yy, dist != 2 && dist != 4);
            }
        }
    }
}

// Draws a 5*5 alignment pattern, with the center module at (x, y).
static void drawAlignmentPattern(BitBucket *modules, BitBucket *isFunction, uint8_t x, uint8_t y) {
    for (int8_t i = -2; i <= 2; i++) {
        for (int8_t j = -2; j <= 2; j++) {
            setFunctionModule(modules, isFunction, x + j, y + i, max(abs(i), abs(j)) != 1);
        }
    }
}

// Draws two copies of the format bits (with its own error correction code)
// based on the given mask and this object's error correction level field.
static void drawFormatBits(BitBucket *modules, BitBucket *isFunction, uint8_t ecc, uint8_t mask) {
    
    uint8_t size = modules->bitOffsetOrWidth;

    // Calculate error correction code and pack bits
    uint32_t data = ecc << 3 | mask;  // errCorrLvl is uint2, mask is uint3
    uint32_t rem = data;
    for (int i = 0; i < 10; i++) {
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    }
    
    data = data << 10 | rem;
    data ^= 0x5412;  // uint15
    
    // Draw first copy
    for (uint8_t i = 0; i <= 5; i++) {
        setFunctionModule(modules, isFunction, 8, i, ((data >> i) & 1) != 0);
    }
    
    setFunctionModule(modules, isFunction, 8, 7, ((data >> 6) & 1) != 0);
    setFunctionModule(modules, isFunction, 8, 8, ((data >> 7) & 1) != 0);
    setFunctionModule(modules, isFunction, 7, 8, ((data >> 8) & 1) != 0);
    
    for (int8_t i = 9; i < 15; i++) {
        setFunctionModule(modules, isFunction, 14 - i, 8, ((data >> i) & 1) != 0);
    }
    
    // Draw second copy
    for (int8_t i = 0; i <= 7; i++) {
        setFunctionModule(modules, isFunction, size - 1 - i, 8, ((data >> i) & 1) != 0);
    }
    
    for (int8_t i = 8; i < 15; i++) {
        setFunctionModule(modules, isFunction, 8, size - 15 + i, ((data >> i) & 1) != 0);
    }
    
    setFunctionModule(modules, isFunction, 8, size - 8, true);
}


// Draws two copies of the version bits (with its own error correction code),
// based on this object's version field (which only has an effect for 7 <= version <= 40).
static void drawVersion(BitBucket *modules, BitBucket *isFunction, uint8_t version) {
    
    int8_t size = modules->bitOffsetOrWidth;

#if LOCK_VERSION != 0 && LOCK_VERSION < 7
    return;
    
#else
    if (version < 7) { return; }
    
    // Calculate error correction code and pack bits
    uint32_t rem = version;  // version is uint6, in the range [7, 40]
    for (uint8_t i = 0; i < 12; i++) {
        rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
    }
    
    uint32_t data = version << 12 | rem;  // uint18
    
    // Draw two copies
    for (uint8_t i = 0; i < 18; i++) {
        bool bit = ((data >> i) & 1) != 0;
        uint8_t a = size - 11 + i % 3, b = i / 3;
        setFunctionModule(modules, isFunction, a, b, bit);
        setFunctionModule(modules, isFunction, b, a, bit);
    }
    
#endif
}

static void drawFunctionPatterns(BitBucket *modules, BitBucket *isFunction, uint8_t version, uint8_t ecc) {
    
    uint8_t size = modules->bitOffsetOrWidth;

    // Draw the horizontal and vertical timing patterns
    for (uint8_t i = 0; i < size; i++) {
        setFunctionModule(modules, isFunction, 6, i, i % 2 == 0);
        setFunctionModule(modules, isFunction, i, 6, i % 2 == 0);
    }
    
    // Draw 3 finder patterns (all corners except bottom right; overwrites some timing modules)
    drawFinderPattern(modules, isFunction, 3, 3);
    drawFinderPattern(modules, isFunction, size - 4, 3);
    drawFinderPattern(modules, isFunction, 3, size - 4);
    
#if LOCK_VERSION == 0 || LOCK_VERSION > 1

    if (version > 1) {

        // Draw the numerous alignment patterns
        
        uint8_t alignCount = version / 7 + 2;
        uint8_t step;
        if (version != 32) {
            step = (version * 4 + alignCount * 2 + 1) / (2 * alignCount - 2) * 2;  // ceil((size - 13) / (2*numAlign - 2)) * 2
        } else { // C-C-C-Combo breaker!
            step = 26;
        }
        
        uint8_t alignPositionIndex = alignCount - 1;
        uint8_t alignPosition[alignCount];
        
        alignPosition[0] = 6;
        
        uint8_t size = version * 4 + 17;
        for (uint8_t i = 0, pos = size - 7; i < alignCount - 1; i++, pos -= step) {
            alignPosition[alignPositionIndex--] = pos;
        }
        
        for (uint8_t i = 0; i < alignCount; i++) {
            for (uint8_t j = 0; j < alignCount; j++) {
                if ((i == 0 && j == 0) || (i == 0 && j == alignCount - 1) || (i == alignCount - 1 && j == 0)) {
                    continue;  // Skip the three finder corners
                } else {
                    drawAlignmentPattern(modules, isFunction, alignPosition[i], alignPosition[j]);
                }
            }
        }
    }
    
#endif
    
    // Draw configuration data
    drawFormatBits(modules, isFunction, ecc, 0);  // Dummy mask value; overwritten later in the constructor
    drawVersion(modules, isFunction, version);
}


// Draws the given sequence of 8-bit codewords (data and error correction) onto the entire
// data area of this QR Code symbol. Function modules need to be marked off before this is called.
static void drawCodewords(BitBucket *modules, BitBucket *isFunction, BitBucket *codewords) {
    
    uint32_t bitLength = codewords->bitOffsetOrWidth;
    uint8_t *data = codewords->data;
    
    uint8_t size = modules->bitOffsetOrWidth;
    
    // Bit index into the data
    uint32_t i = 0;
    
    // Do the funny zigzag scan
    for (int16_t right = size - 1; right >= 1; right -= 2) {  // Index of right column in each column pair
        if (right == 6) { right = 5; }
        
        for (uint8_t vert = 0; vert < size; vert++) {  // Vertical counter
            for (int j = 0; j < 2; j++) {
                uint8_t x = right - j;  // Actual x coordinate
                bool upwards = ((right & 2) == 0) ^ (x < 6);
                uint8_t y = upwards ? size - 1 - vert : vert;  // Actual y coordinate
                if (!bb_getBit(isFunction, x, y) && i < bitLength) {
                    bb_setBit(modules, x, y, ((data[i >> 3] >> (7 - (i & 7))) & 1) != 0);
                    i++;
                }
                // If there are any remainder bits (0 to 7), they are already
                // set to 0/false/white when the grid of modules was initialized
            }
        }
    }
}



#pragma mark - Penalty Calculation

#define PENALTY_N1      3
#define PENALTY_N2      3
#define PENALTY_N3     40
#define PENALTY_N4     10

// Calculates and returns the penalty score based on state of this QR Code's current modules.
// This is used by the automatic mask choice algorithm to find the mask pattern that yields the lowest score.
// @TODO: This can be optimized by working with the bytes instead of bits.
static uint32_t getPenaltyScore(BitBucket *modules) {
    uint32_t result = 0;
    
    uint8_t size = modules->bitOffsetOrWidth;
    
    // Adjacent modules in row having same color
    for (uint8_t y = 0; y < size; y++) {
        
        bool colorX = bb_getBit(modules, 0, y);
        for (uint8_t x = 1, runX = 1; x < size; x++) {
            bool cx = bb_getBit(modules, x, y);
            if (cx != colorX) {
                colorX = cx;
                runX = 1;
                
            } else {
                runX++;
                if (runX == 5) {
                    result += PENALTY_N1;
                } else if (runX > 5) {
                    result++;
                }
            }
        }
    }
    
    // Adjacent modules in column having same color
    for (uint8_t x = 0; x < size; x++) {
        bool colorY = bb_getBit(modules, x, 0);
        for (uint8_t y = 1, runY = 1; y < size; y++) {
            bool cy = bb_getBit(modules, x, y);
            if (cy != colorY) {
                colorY = cy;
                runY = 1;
            } else {
                runY++;
                if (runY == 5) {
                    result += PENALTY_N1;
                } else if (runY > 5) {
                    result++;
                }
            }
        }
    }
    
    uint16_t black = 0;
    for (uint8_t y = 0; y < size; y++) {
        uint16_t bitsRow = 0, bitsCol = 0;
        for (uint8_t x = 0; x < size; x++) {
            bool color = bb_getBit(modules, x, y);

            // 2*2 blocks of modules having same color
            if (x > 0 && y > 0) {
                bool colorUL = bb_getBit(modules, x - 1, y - 1);
                bool colorUR = bb_getBit(modules, x, y - 1);
                bool colorL = bb_getBit(modules, x - 1, y);
                if (color == colorUL && color == colorUR && color == colorL) {
                    result += PENALTY_N2;
                }
            }

            // Finder-like pattern in rows and columns
            bitsRow = ((bitsRow << 1) & 0x7FF) | color;
            bitsCol = ((bitsCol << 1) & 0x7FF) | bb_getBit(modules, y, x);

            // Needs 11 bits accumulated
            if (x >= 10) {
                if (bitsRow == 0x05D || bitsRow == 0x5D0) {
                    result += PENALTY_N3;
                }
                if (bitsCol == 0x05D || bitsCol == 0x5D0) {
                    result += PENALTY_N3;
                }
            }

            // Balance of black and white modules
            if (color) { black++; }
        }
    }

    // Find smallest k such that (45-5k)% <= dark/total <= (55+5k)%
    uint16_t total = size * size;
    for (uint16_t k = 0; black * 20 < (9 - k) * total || black * 20 > (11 + k) * total; k++) {
        result += PENALTY_N4;
    }
    
    return result;
}


#pragma mark - Reed-Solomon Generator

static uint8_t rs_multiply(uint8_t x, uint8_t y) {
    // Russian peasant multiplication
    // See: https://en.wikipedia.org/wiki/Ancient_Egyptian_multiplication
    uint16_t z = 0;
    for (int8_t i = 7; i >= 0; i--) {
        z = (z << 1) ^ ((z >> 7) * 0x11D);
        z ^= ((y >> i) & 1) * x;
    }
    return z;
}

static void rs_init(uint8_t degree, uint8_t *coeff) {
    memset(coeff, 0, degree);
    coeff[degree - 1] = 1;
    
    // Compute the product polynomial (x - r^0) * (x - r^1) * (x - r^2) * ... * (x - r^{degree-1}),
    // drop the highest term, and store the rest of the coefficients in order of descending powers.
    // Note that r = 0x02, which is a generator element of this field GF(2^8/0x11D).
    uint16_t root = 1;
    for (uint8_t i = 0; i < degree; i++) {
        // Multiply the current product by (x - r^i)
        for (uint8_t j = 0; j < degree; j++) {
            coeff[j] = rs_multiply(coeff[j], root);
            if (j + 1 < degree) {
                coeff[j] ^= coeff[j + 1];
            }
        }
        root = (root << 1) ^ ((root >> 7) * 0x11D);  // Multiply by 0x02 mod GF(2^8/0x11D)
    }
}

static void rs_getRemainder(uint8_t degree, uint8_t *coeff, uint8_t *data, uint8_t length, uint8_t *result, uint8_t stride) {
    // Compute the remainder by performing polynomial division
    
    //for (uint8_t i = 0; i < degree; i++) { result[] = 0; }
    //memset(result, 0, degree);
    
    for (uint8_t i = 0; i < length; i++) {
        uint8_t factor = data[i] ^ result[0];
        for (uint8_t j = 1; j < degree; j++) {
            result[(j - 1) * stride] = result[j * stride];
        }
        result[(degree - 1) * stride] = 0;
        
        for (uint8_t j = 0; j < degree; j++) {
            result[j * stride] ^= rs_multiply(coeff[j], factor);
        }
    }
}



#pragma mark - QrCode

static int8_t encodeDataCodewords(BitBucket *dataCodewords, const uint8_t *text, uint16_t length, uint8_t version) {
    int8_t mode = TB_MODE_BYTE;
    
    if (isNumeric((char*)text, length)) {
        mode = TB_MODE_NUMERIC;
        bb_appendBits(dataCodewords, 1 << TB_MODE_NUMERIC, 4);
        bb_appendBits(dataCodewords, length, getModeBits(version, TB_MODE_NUMERIC));

        uint16_t accumData = 0;
        uint8_t accumCount = 0;
        for (uint16_t i = 0; i < length; i++) {
            accumData = accumData * 10 + ((char)(text[i]) - '0');
            accumCount++;
            if (accumCount == 3) {
                bb_appendBits(dataCodewords, accumData, 10);
                accumData = 0;
                accumCount = 0;
            }
        }
        
        // 1 or 2 digits remaining
        if (accumCount > 0) {
            bb_appendBits(dataCodewords, accumData, accumCount * 3 + 1);
        }
        
    } else if (isAlphanumeric((char*)text, length)) {
        mode = TB_MODE_ALPHANUMERIC;
        bb_appendBits(dataCodewords, 1 << TB_MODE_ALPHANUMERIC, 4);
        bb_appendBits(dataCodewords, length, getModeBits(version, TB_MODE_ALPHANUMERIC));

        uint16_t accumData = 0;
        uint8_t accumCount = 0;
        for (uint16_t i = 0; i  < length; i++) {
            accumData = accumData * 45 + getAlphanumeric((char)(text[i]));
            accumCount++;
            if (accumCount == 2) {
                bb_appendBits(dataCodewords, accumData, 11);
                accumData = 0;
                accumCount = 0;
            }
        }
        
        // 1 character remaining
        if (accumCount > 0) {
            bb_appendBits(dataCodewords, accumData, 6);
        }
        
    } else {
        bb_appendBits(dataCodewords, 1 << TB_MODE_BYTE, 4);
        bb_appendBits(dataCodewords, length, getModeBits(version, TB_MODE_BYTE));
        for (uint16_t i = 0; i < length; i++) {
            bb_appendBits(dataCodewords, (char)(text[i]), 8);
        }
    }
    
    //bb_setBits(dataCodewords, length, 4, getModeBits(version, mode));
    
    return mode;
}

static void performErrorCorrection(uint8_t version, uint8_t ecc, BitBucket *data) {
    
    // See: http://www.thonky.com/qr-code-tutorial/structure-final-message
    
#if LOCK_VERSION == 0
    uint8_t numBlocks = NUM_ERROR_CORRECTION_BLOCKS[ecc][version - 1];
    uint16_t totalEcc = NUM_ERROR_CORRECTION_CODEWORDS[ecc][version - 1];
    uint16_t moduleCount = NUM_RAW_DATA_MODULES[version - 1];
#else
    uint8_t numBlocks = NUM_ERROR_CORRECTION_BLOCKS[ecc];
    uint16_t totalEcc = NUM_ERROR_CORRECTION_CODEWORDS[ecc];
    uint16_t moduleCount = NUM_RAW_DATA_MODULES;
#endif
    
    uint8_t blockEccLen = totalEcc / numBlocks;
    uint8_t numShortBlocks = numBlocks - moduleCount / 8 % numBlocks;
    uint8_t shortBlockLen = moduleCount / 8 / numBlocks;
    
    uint8_t shortDataBlockLen = shortBlockLen - blockEccLen;
    
    uint8_t result[data->capacityBytes];
    memset(result, 0, sizeof(result));
    
    uint8_t coeff[blockEccLen];
    rs_init(blockEccLen, coeff);
    
    uint16_t offset = 0;
    uint8_t *dataBytes = data->data;
    
    
    // Interleave all short blocks
    for (uint8_t i = 0; i < shortDataBlockLen; i++) {
        uint16_t index = i;
        uint8_t stride = shortDataBlockLen;
        for (uint8_t blockNum = 0; blockNum < numBlocks; blockNum++) {
            result[offset++] = dataBytes[index];
            
#if LOCK_VERSION == 0 || LOCK_VERSION >= 5
            if (blockNum == numShortBlocks) { stride++; }
#endif
            index += stride;
        }
    }
    
    // Version less than 5 only have short blocks
#if LOCK_VERSION == 0 || LOCK_VERSION >= 5
    {
        // Interleave long blocks
        uint16_t index = shortDataBlockLen * (numShortBlocks + 1);
        uint8_t stride = shortDataBlockLen;
        for (uint8_t blockNum = 0; blockNum < numBlocks - numShortBlocks; blockNum++) {
            result[offset++] = dataBytes[index];
            
            if (blockNum == 0) { stride++; }
            index += stride;
        }
    }
#endif
    
    // Add all ecc blocks, interleaved
    uint8_t blockSize = shortDataBlockLen;
    for (uint8_t blockNum = 0; blockNum < numBlocks; blockNum++) {
        
#if LOCK_VERSION == 0 || LOCK_VERSION >= 5
        if (blockNum == numShortBlocks) { blockSize++; }
#endif
        rs_getRemainder(blockEccLen, coeff, dataBytes, blockSize, &result[offset + blockNum], numBlocks);
        dataBytes += blockSize;
    }
    
    memcpy(data->data, result, data->capacityBytes);
    data->bitOffsetOrWidth = moduleCount;
}

// We store the Format bits tightly packed into a single byte (each of the 4 modes is 2 bits)
// The format bits can be determined by ECC_FORMAT_BITS >> (2 * ecc)
static const uint8_t ECC_FORMAT_BITS = (0x02 << 6) | (0x03 << 4) | (0x00 << 2) | (0x01 << 0);


#pragma mark - Public TB_QRCode functions

uint16_t tb_qrcode_getBufferSize(uint8_t version) {
    return bb_getGridSizeBytes(4 * version + 17);
}

// @TODO: Return error if data is too big.
int8_t tb_qrcode_initBytes(TB_QRCode *qrcode, uint8_t *modules, uint8_t version, uint8_t ecc, uint8_t *data, uint16_t length) {
    uint8_t size = version * 4 + 17;
    qrcode->version = version;
    qrcode->size = size;
    qrcode->ecc = ecc;
    qrcode->modules = modules;
    
    uint8_t eccFormatBits = (ECC_FORMAT_BITS >> (2 * ecc)) & 0x03;
    
#if LOCK_VERSION == 0
    uint16_t moduleCount = NUM_RAW_DATA_MODULES[version - 1];
    uint16_t dataCapacity = moduleCount / 8 - NUM_ERROR_CORRECTION_CODEWORDS[eccFormatBits][version - 1];
#else
    version = LOCK_VERSION;
    uint16_t moduleCount = NUM_RAW_DATA_MODULES;
    uint16_t dataCapacity = moduleCount / 8 - NUM_ERROR_CORRECTION_CODEWORDS[eccFormatBits];
#endif
    
    struct BitBucket codewords;
    uint8_t codewordBytes[bb_getBufferSizeBytes(moduleCount)];
    bb_initBuffer(&codewords, codewordBytes, (int32_t)sizeof(codewordBytes));
    
    // Place the data code words into the buffer
    int8_t mode = encodeDataCodewords(&codewords, data, length, version);
    
    if (mode < 0) { return -1; }
    qrcode->mode = mode;
    
    // Add terminator and pad up to a byte if applicable
    uint32_t padding = (dataCapacity * 8) - codewords.bitOffsetOrWidth;
    if (padding > 4) { padding = 4; }
    bb_appendBits(&codewords, 0, padding);
    bb_appendBits(&codewords, 0, (8 - codewords.bitOffsetOrWidth % 8) % 8);

    // Pad with alternate bytes until data capacity is reached
    for (uint8_t padByte = 0xEC; codewords.bitOffsetOrWidth < (dataCapacity * 8); padByte ^= 0xEC ^ 0x11) {
        bb_appendBits(&codewords, padByte, 8);
    }

    BitBucket modulesGrid;
    bb_initGrid(&modulesGrid, modules, size);
    
    BitBucket isFunctionGrid;
    uint8_t isFunctionGridBytes[bb_getGridSizeBytes(size)];
    bb_initGrid(&isFunctionGrid, isFunctionGridBytes, size);
    
    // Draw function patterns, draw all codewords, do masking
    drawFunctionPatterns(&modulesGrid, &isFunctionGrid, version, eccFormatBits);
    performErrorCorrection(version, eccFormatBits, &codewords);
    drawCodewords(&modulesGrid, &isFunctionGrid, &codewords);
    
    // Find the best (lowest penalty) mask
    uint8_t mask = 0;
    int32_t minPenalty = INT32_MAX;
    for (uint8_t i = 0; i < 8; i++) {
        drawFormatBits(&modulesGrid, &isFunctionGrid, eccFormatBits, i);
        applyMask(&modulesGrid, &isFunctionGrid, i);
        int penalty = getPenaltyScore(&modulesGrid);
        if (penalty < minPenalty) {
            mask = i;
            minPenalty = penalty;
        }
        applyMask(&modulesGrid, &isFunctionGrid, i);  // Undoes the mask due to XOR
    }
    
    qrcode->mask = mask;
    
    // Overwrite old format bits
    drawFormatBits(&modulesGrid, &isFunctionGrid, eccFormatBits, mask);
    
    // Apply the final choice of mask
    applyMask(&modulesGrid, &isFunctionGrid, mask);

    return 0;
}

int8_t tb_qrcode_initText(TB_QRCode *qrcode, uint8_t *modules, uint8_t version, uint8_t ecc, const char *data) {
    return tb_qrcode_initBytes(qrcode, modules, version, ecc, (uint8_t*)data, strlen(data));
}

bool tb_qrcode_getModule(TB_QRCode *qrcode, uint8_t x, uint8_t y) {
    if (x < 0 || x >= qrcode->size || y < 0 || y >= qrcode->size) {
        return false;
    }

    uint32_t offset = y * qrcode->size + x;
    return (qrcode->modules[offset >> 3] & (1 << (7 - (offset & 0x07)))) != 0;
}

/*
uint8_t qrcode_getHexLength(TB_QRCode *qrcode) {
    return ((qrcode->size * qrcode->size) + 7) / 4;
}

void qrcode_getHex(TB_QRCode *qrcode, char *result) {
    
}
*/


// ============================================================================
// 1. WI-FI & BACKEND CONFIGURATION (EDIT THESE FOR YOUR NETWORK)
// ============================================================================

const char* WIFI_SSID     = "YOUR_WIFI_NAME";      // <-- Put your Wi-Fi SSID here
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";  // <-- Put your Wi-Fi Password here

// Backend API Endpoint (Change to your computer's local IP address)
// e.g. "http://192.168.1.50:3000/api/verify"
String BACKEND_API_URL    = "http://192.168.1.50:3000/api/verify";

// Merchant & Device Identity
String merchantId         = "M12345678";
String merchantName       = "Rajesh Kirana Store";
String merchantUpiVpa     = "rajesh.kirana@paytm"; // <-- Put your real/test UPI VPA (e.g. yourname@paytm)
String transactionId      = "ORD-1001";
double transactionAmount  = 250.0;

// ============================================================================
// 2. HARDWARE PIN DEFINITIONS (MATCHING YOUR EXACT WIRING)
// ============================================================================

// 3.2" TFT Display Pins
#define TFT_CS     5
#define TFT_DC     2
#define TFT_RST    4
#define SPI_SCK    18
#define SPI_MOSI   23
#define SPI_MISO   19

// Touchscreen Pins
#define TOUCH_CS   15
#define TOUCH_IRQ  27

// Output Pins
#define LED_PIN    25
#define BUZZER_PIN 26

// Microphone Module Pins
#define MIC_A0     34
#define MIC_D0     35

// Screen Dimensions
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// Touch Calibration & Debounce
#define TOUCH_X_MIN   200
#define TOUCH_X_MAX   3800
#define TOUCH_Y_MIN   200
#define TOUCH_Y_MAX   3800
#define TOUCH_SWAP_XY  false
#define TOUCH_INVERT_X false
#define TOUCH_INVERT_Y false

const unsigned long TOUCH_DEBOUNCE = 600;
unsigned long lastTouchTime = 0;

// Button Coordinates on Screen
#define VERIFY_X   18
#define VERIFY_Y   182
#define VERIFY_W   136
#define VERIFY_H   46

#define DISPUTE_X  166
#define DISPUTE_Y  182
#define DISPUTE_W  136
#define DISPUTE_H  46

// ============================================================================
// 3. COLOR PALETTE (PAYTM BRANDED THEME)
// ============================================================================

#define COLOR_NAVY       0x0113   // Paytm Signature Deep Navy Blue
#define COLOR_CYAN       0x05BF   // Paytm Accent Cyan Blue
#define COLOR_BG         0xFFFF   // Crisp White Background
#define COLOR_TEXT_DARK  0x1925   // Charcoal Black Text
#define COLOR_MUTED      0x7BEF   // Light Slate Grey
#define COLOR_BORDER     0xC618   // Border Grey
#define COLOR_SUCCESS    0x05E0   // Vivid Green
#define COLOR_FRAUD_RED  0xF800   // Warning Red
#define COLOR_AMBER      0xFD20   // Dispute Orange / Amber

// ============================================================================
// 4. HARDWARE OBJECTS
// ============================================================================

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS);

bool wifiConnected = false;
unsigned long returnToHomeTimer = 0;
bool pendingReturnToHome = false;

// Forward Declarations
void showStartupScreen();
void connectToWiFi();
void showHomeScreen();
void showVerifyingScreen(const char* message);
void showSuccessScreen(double amount, float fraudScore);
void showFraudAlertScreen(double amount, float fraudScore, const char* reason);
void showDisputeScreen(const char* ticketId);
void showNetworkErrorScreen(const char* errorMsg);
void drawQRCode(const String& data, int startX, int startY, int scale);
void drawButton(int x, int y, int w, int h, const char* label, uint16_t bgColor, uint16_t textColor);
void drawCenteredText(const char* text, int centerX, int y, uint8_t size = 1, uint16_t color = COLOR_TEXT_DARK);
void playPaytmChime();
void playFraudSiren();
void playDisputeChime();
void playClickTone();
void playTone(unsigned int freq, unsigned long durationMs);
void verifyTransactionWithServer(double expectedAmount = 0.0);
void reportDisputeToServer();
void sendHeartbeatToServer();
void checkTouch();
void checkMicrophone();
void processSerialCli();

// ============================================================================
// 5. SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println(F("\n=================================================="));
  Serial.println(F("   PAYTM TRUSTBOX: FRAUD-PROOF UPI TERMINAL       "));
  Serial.println(F("   Single-File Standalone ESP32 Firmware          "));
  Serial.println(F("=================================================="));

  // Initialize GPIOs
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MIC_D0, INPUT);
  pinMode(MIC_A0, INPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Start Hardware SPI Bus
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, TFT_CS);

  // Initialize 3.2\" TFT LCD (ILI9341)
  tft.begin();
  tft.setRotation(1); // Landscape mode: 320x240
  tft.fillScreen(COLOR_BG);

  // Initialize Touchscreen Controller
  touch.begin();
  touch.setRotation(1);

  // Startup Sound & Splash Screen
  playClickTone();
  showStartupScreen();

  // Connect to Wi-Fi
  connectToWiFi();

  // Handshake with laptop backend (Registers device on Dashboard)
  if (wifiConnected) {
    sendHeartbeatToServer();
  }

  // Show Main Merchant Home Screen
  showHomeScreen();

  Serial.println(F("[STATUS] Terminal Ready! Type 'HELP' in Serial for test commands."));
}

// ============================================================================
// 6. MAIN LOOP
// ============================================================================

void loop() {
  // 1. Check Touchscreen Press
  checkTouch();

  // 2. Check Microphone Audio Trigger
  checkMicrophone();

  // 3. Handle Auto-Return to Home Screen after result displays
  if (pendingReturnToHome && millis() > returnToHomeTimer) {
    pendingReturnToHome = false;
    showHomeScreen();
  }

  // 4. Periodic Heartbeat to Dashboard (Every 12s keeps dashboard live badge GREEN)
  static unsigned long lastHeartbeatTime = 0;
  if (wifiConnected && (millis() - lastHeartbeatTime > 12000)) {
    lastHeartbeatTime = millis();
    sendHeartbeatToServer();
  }

  // 5. Check Serial Diagnostic Commands
  processSerialCli();

  delay(20);
}

// ============================================================================
// 7. UI SCREENS
// ============================================================================

void showStartupScreen() {
  tft.fillScreen(COLOR_NAVY);

  // Paytm Brand Header
  tft.fillRect(0, 0, SCREEN_WIDTH, 42, 0x001944);
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(2);
  drawCenteredText("PAYTM TRUSTBOX", SCREEN_WIDTH / 2, 12, 2, COLOR_CYAN);

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  drawCenteredText("AI Anti-Fraud Terminal", SCREEN_WIDTH / 2, 75, 2, ILI9341_WHITE);

  tft.setTextSize(1);
  drawCenteredText("Two-Way UPI Payment Verification", SCREEN_WIDTH / 2, 110, 1, COLOR_MUTED);
  drawCenteredText("Hardware Protected Against Fake Screenshots", SCREEN_WIDTH / 2, 130, 1, COLOR_MUTED);

  // Progress Bar
  tft.drawRoundRect(40, 170, 240, 14, 4, COLOR_CYAN);
  for (int i = 0; i < 232; i += 8) {
    tft.fillRect(44, 174, i, 6, COLOR_CYAN);
    delay(20);
  }

  delay(400);
}

void connectToWiFi() {
  tft.fillScreen(COLOR_BG);

  // Header Banner
  tft.fillRect(0, 0, SCREEN_WIDTH, 36, COLOR_NAVY);
  drawCenteredText("Wi-Fi Configuration", SCREEN_WIDTH / 2, 10, 2, ILI9341_WHITE);

  tft.setCursor(20, 60);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_DARK);
  tft.print("SSID: ");
  tft.println(WIFI_SSID);

  tft.setCursor(20, 78);
  tft.print("Connecting to network");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 25) {
    delay(500);
    tft.print(".");
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println();
    Serial.printf("[WIFI] Connected! IP Address: %s\n", WiFi.localIP().toString().c_str());

    tft.fillRect(20, 110, 280, 50, 0xE8F9F1);
    tft.drawRect(20, 110, 280, 50, COLOR_SUCCESS);
    drawCenteredText("Wi-Fi Connected!", SCREEN_WIDTH / 2, 120, 2, COLOR_SUCCESS);
    drawCenteredText(WiFi.localIP().toString().c_str(), SCREEN_WIDTH / 2, 142, 1, COLOR_TEXT_DARK);

    digitalWrite(LED_PIN, HIGH);
    delay(300);
    digitalWrite(LED_PIN, LOW);
    delay(800);
  } else {
    wifiConnected = false;
    Serial.println();
    Serial.println(F("[WIFI] Connection failed. Operating in offline mode."));

    tft.fillRect(20, 110, 280, 50, 0xFFECEC);
    tft.drawRect(20, 110, 280, 50, COLOR_FRAUD_RED);
    drawCenteredText("Wi-Fi Not Connected", SCREEN_WIDTH / 2, 120, 2, COLOR_FRAUD_RED);
    drawCenteredText("Device will run in Diagnostic Mode", SCREEN_WIDTH / 2, 142, 1, COLOR_TEXT_DARK);

    delay(1200);
  }
}

void showHomeScreen() {
  tft.fillScreen(COLOR_BG);

  // Top Header Banner
  tft.fillRect(0, 0, SCREEN_WIDTH, 34, COLOR_NAVY);
  tft.setCursor(12, 10);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_CYAN);
  tft.print("Paytm");
  tft.setTextColor(ILI9341_WHITE);
  tft.print(" TrustBox");

  // Wi-Fi Status Pill in Header
  int wifiPillX = 220;
  int wifiPillY = 8;
  tft.fillRoundRect(wifiPillX, wifiPillY, 90, 18, 9, wifiConnected ? 0x05E0 : 0xF800);
  tft.setCursor(wifiPillX + (wifiConnected ? 16 : 22), wifiPillY + 5);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  tft.print(wifiConnected ? "Online" : "Offline");

  // Left Column: Transaction Details Card
  tft.fillRect(12, 44, 175, 126, 0xF4F6FA);
  tft.drawRoundRect(12, 44, 175, 126, 6, COLOR_BORDER);

  tft.setTextColor(COLOR_MUTED);
  tft.setTextSize(1);
  tft.setCursor(20, 52);
  tft.print("MERCHANT:");

  tft.setTextColor(COLOR_NAVY);
  tft.setTextSize(1);
  tft.setCursor(20, 64);
  tft.print(merchantName.substring(0, 18));

  tft.setTextColor(COLOR_MUTED);
  tft.setCursor(20, 80);
  tft.print("ORDER ID:");

  tft.setTextColor(COLOR_TEXT_DARK);
  tft.setCursor(20, 92);
  tft.print(transactionId);

  tft.setTextColor(COLOR_MUTED);
  tft.setCursor(20, 108);
  tft.print("AMOUNT TO PAY:");

  tft.setTextColor(COLOR_NAVY);
  tft.setTextSize(2);
  tft.setCursor(20, 120);
  tft.printf("INR %.0f", transactionAmount);

  tft.setTextSize(1);
  tft.setTextColor(0x02E0);
  tft.setCursor(20, 148);
  tft.print("* Real-Time AI Protected");

  // Right Column: Real Standard UPI QR Code (Scannable with Paytm / GPay / PhonePe)
  String upiPayload = "upi://pay?pa=" + merchantUpiVpa + "&pn=PaytmMerchant&am=" + String(transactionAmount, 2) + "&tr=" + transactionId + "&cu=INR";
  drawQRCode(upiPayload, 204, 52, 2);
  drawCenteredText("Scan with Paytm", 254, 155, 1, COLOR_MUTED);

  // Touch Buttons at Bottom
  drawButton(VERIFY_X, VERIFY_Y, VERIFY_W, VERIFY_H, "VERIFY", COLOR_SUCCESS, ILI9341_WHITE);
  drawButton(DISPUTE_X, DISPUTE_Y, DISPUTE_W, DISPUTE_H, "DISPUTE", COLOR_FRAUD_RED, ILI9341_WHITE);
}

void showVerifyingScreen(const char* message) {
  tft.fillScreen(COLOR_BG);

  // Header Banner
  tft.fillRect(0, 0, SCREEN_WIDTH, 34, COLOR_NAVY);
  drawCenteredText("PAYMENT VERIFICATION", SCREEN_WIDTH / 2, 10, 2, ILI9341_WHITE);

  // Status Box
  tft.fillRect(20, 50, 280, 170, 0xFFF8EB);
  tft.drawRoundRect(20, 50, 280, 170, 8, COLOR_AMBER);

  // Hourglass icon
  tft.setCursor(145, 75);
  tft.setTextSize(3);
  tft.setTextColor(COLOR_AMBER);
  tft.print("@");

  tft.setTextSize(2);
  tft.setTextColor(COLOR_NAVY);
  drawCenteredText("Verifying Payment...", SCREEN_WIDTH / 2, 115, 2, COLOR_NAVY);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT_DARK);
  drawCenteredText(message ? message : "Checking Paytm UPI Gateway Ledger", SCREEN_WIDTH / 2, 145, 1, COLOR_TEXT_DARK);
  drawCenteredText("Running AI Fraud Detection Model...", SCREEN_WIDTH / 2, 165, 1, COLOR_MUTED);

  // LED indication
  digitalWrite(LED_PIN, HIGH);
}

void showSuccessScreen(double amount, float fraudScore) {
  tft.fillScreen(0xE8F9F1); // Soft green background

  // Header Banner
  tft.fillRect(0, 0, SCREEN_WIDTH, 38, COLOR_SUCCESS);
  drawCenteredText("PAYMENT CONFIRMED", SCREEN_WIDTH / 2, 12, 2, ILI9341_WHITE);

  // Big Amount Display
  char amtStr[32];
  snprintf(amtStr, sizeof(amtStr), "INR %.2f", amount);
  tft.setTextSize(3);
  drawCenteredText(amtStr, SCREEN_WIDTH / 2, 60, 3, COLOR_NAVY);

  // Checkmark Badge
  tft.fillCircle(SCREEN_WIDTH / 2, 120, 22, COLOR_SUCCESS);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.setCursor(SCREEN_WIDTH / 2 - 8, 110);
  tft.print("v");

  // Subtext
  tft.setTextSize(2);
  drawCenteredText("Genuine UPI Transaction", SCREEN_WIDTH / 2, 155, 2, COLOR_NAVY);

  char scoreStr[48];
  snprintf(scoreStr, sizeof(scoreStr), "Fraud Score: %.2f (SAFE)", fraudScore);
  tft.setTextSize(1);
  drawCenteredText(scoreStr, SCREEN_WIDTH / 2, 185, 1, 0x03E0);
  drawCenteredText("Safe to hand over goods to customer.", SCREEN_WIDTH / 2, 205, 1, COLOR_TEXT_DARK);

  // Success Chime and LED
  digitalWrite(LED_PIN, HIGH);
  playPaytmChime();
  delay(500);
  digitalWrite(LED_PIN, LOW);

  returnToHomeTimer = millis() + 7000;
  pendingReturnToHome = true;
}

void showFraudAlertScreen(double amount, float fraudScore, const char* reason) {
  tft.fillScreen(0xFFECEC); // Soft alert red background

  // Header Banner
  tft.fillRect(0, 0, SCREEN_WIDTH, 38, COLOR_FRAUD_RED);
  drawCenteredText("CRITICAL FRAUD ALERT!", SCREEN_WIDTH / 2, 12, 2, ILI9341_WHITE);

  // Warning icon
  tft.fillTriangle(SCREEN_WIDTH / 2, 50, SCREEN_WIDTH / 2 - 25, 90, SCREEN_WIDTH / 2 + 25, 90, COLOR_FRAUD_RED);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(SCREEN_WIDTH / 2 - 5, 68);
  tft.print("!");

  // Main Warning
  tft.setTextSize(2);
  drawCenteredText("DO NOT HAND OVER GOODS!", SCREEN_WIDTH / 2, 105, 2, COLOR_FRAUD_RED);

  char amtStr[48];
  snprintf(amtStr, sizeof(amtStr), "Claimed: INR %.0f | NO RECORD", amount);
  tft.setTextSize(1);
  drawCenteredText(amtStr, SCREEN_WIDTH / 2, 135, 1, COLOR_NAVY);

  drawCenteredText(reason ? reason : "Fake screenshot detected. No credit in bank.", SCREEN_WIDTH / 2, 155, 1, COLOR_FRAUD_RED);

  char scoreStr[48];
  snprintf(scoreStr, sizeof(scoreStr), "Fraud Risk Score: %.2f / 1.00 (HIGH)", fraudScore);
  drawCenteredText(scoreStr, SCREEN_WIDTH / 2, 175, 1, COLOR_TEXT_DARK);

  drawCenteredText("Press [DISPUTE] to notify Paytm Ops & Police.", SCREEN_WIDTH / 2, 200, 1, COLOR_NAVY);

  // Fraud Siren Sound & Flashing LED
  playFraudSiren();

  returnToHomeTimer = millis() + 9000;
  pendingReturnToHome = true;
}

void showDisputeScreen(const char* ticketId) {
  tft.fillScreen(0xFFF8EB); // Soft amber background

  // Header Banner
  tft.fillRect(0, 0, SCREEN_WIDTH, 38, COLOR_AMBER);
  drawCenteredText("DISPUTE REGISTERED", SCREEN_WIDTH / 2, 12, 2, ILI9341_WHITE);

  tft.setTextSize(2);
  drawCenteredText("Dispute Raised With Ops", SCREEN_WIDTH / 2, 65, 2, COLOR_NAVY);

  char tktStr[48];
  snprintf(tktStr, sizeof(tktStr), "Ticket ID: %s", ticketId ? ticketId : "DSP-9042");
  tft.setTextSize(2);
  drawCenteredText(tktStr, SCREEN_WIDTH / 2, 105, 2, COLOR_AMBER);

  tft.setTextSize(1);
  drawCenteredText("Audit trail logged in Paytm Cybercrime database.", SCREEN_WIDTH / 2, 145, 1, COLOR_TEXT_DARK);
  drawCenteredText("Paytm Support Officer will call merchant shortly.", SCREEN_WIDTH / 2, 165, 1, COLOR_TEXT_DARK);
  drawCenteredText("Do not refund cash to customer.", SCREEN_WIDTH / 2, 195, 1, COLOR_FRAUD_RED);

  // Play Dispute Chime
  playDisputeChime();

  returnToHomeTimer = millis() + 8000;
  pendingReturnToHome = true;
}

void showNetworkErrorScreen(const char* errorMsg) {
  tft.fillScreen(COLOR_BG);

  tft.fillRect(0, 0, SCREEN_WIDTH, 34, COLOR_FRAUD_RED);
  drawCenteredText("NETWORK ERROR", SCREEN_WIDTH / 2, 10, 2, ILI9341_WHITE);

  tft.setTextSize(2);
  drawCenteredText("Cannot Reach Server", SCREEN_WIDTH / 2, 80, 2, COLOR_FRAUD_RED);

  tft.setTextSize(1);
  drawCenteredText(errorMsg ? errorMsg : "Connection timed out after 3 retries", SCREEN_WIDTH / 2, 120, 1, COLOR_TEXT_DARK);
  drawCenteredText("Check Wi-Fi router & Server IP setting.", SCREEN_WIDTH / 2, 145, 1, COLOR_MUTED);

  playTone(500, 300);

  returnToHomeTimer = millis() + 4000;
  pendingReturnToHome = true;
}

// ============================================================================
// 8. GRAPHICS HELPERS (QR CODE, BUTTONS, TEXT)
// ============================================================================

void drawQRCode(const String& data, int startX, int startY, int scale) {
  TB_QRCode qrcode;
  uint8_t qrcodeData[tb_qrcode_getBufferSize(5)];

  tb_qrcode_initText(&qrcode, qrcodeData, 5, TB_ECC_LOW, data.c_str());
  int qrSize = qrcode.size;

  // White Border around QR
  tft.fillRect(startX - 4, startY - 4, qrSize * scale + 8, qrSize * scale + 8, ILI9341_WHITE);
  tft.drawRect(startX - 4, startY - 4, qrSize * scale + 8, qrSize * scale + 8, COLOR_BORDER);

  for (uint8_t y = 0; y < qrSize; y++) {
    for (uint8_t x = 0; x < qrSize; x++) {
      if (tb_qrcode_getModule(&qrcode, x, y)) {
        tft.fillRect(startX + x * scale, startY + y * scale, scale, scale, COLOR_NAVY);
      }
    }
  }
}

void drawButton(int x, int y, int w, int h, const char* label, uint16_t bgColor, uint16_t textColor) {
  tft.fillRoundRect(x, y, w, h, 8, bgColor);
  tft.drawRoundRect(x, y, w, h, 8, COLOR_BORDER);

  tft.setTextColor(textColor);
  tft.setTextSize(2);

  int16_t x1, y1;
  uint16_t textW, textH;
  tft.getTextBounds(label, 0, 0, &x1, &y1, &textW, &textH);

  tft.setCursor(x + (w - textW) / 2, y + (h - textH) / 2);
  tft.print(label);
}

void drawCenteredText(const char* text, int centerX, int y, uint8_t size, uint16_t color) {
  tft.setTextSize(size);
  tft.setTextColor(color);

  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, y, &x1, &y1, &w, &h);

  tft.setCursor(centerX - w / 2, y);
  tft.print(text);
}

// ============================================================================
// 9. BUZZER & SOUND GENERATION
// ============================================================================

void playTone(unsigned int freq, unsigned long durationMs) {
#if defined(ESP32)
  ledcAttach(BUZZER_PIN, freq, 8);
  ledcWrite(BUZZER_PIN, 128); // 50% duty cycle
  delay(durationMs);
  ledcWrite(BUZZER_PIN, 0);
  ledcDetach(BUZZER_PIN);
#else
  tone(BUZZER_PIN, freq, durationMs);
  delay(durationMs);
  noTone(BUZZER_PIN);
#endif
}

void playPaytmChime() {
  Serial.println(F("[AUDIO] >> Playing Paytm Official Chime (E5 -> G#5 -> B5 -> E6)"));
  playTone(659, 110);  // E5
  delay(30);
  playTone(831, 110);  // G#5
  delay(30);
  playTone(988, 130);  // B5
  delay(30);
  playTone(1319, 350); // E6
}

void playFraudSiren() {
  Serial.println(F("[AUDIO] >> WARNING FRAUD SIREN (880Hz <-> 440Hz)"));
  for (int i = 0; i < 4; i++) {
    digitalWrite(LED_PIN, HIGH);
    playTone(880, 130);
    delay(30);
    digitalWrite(LED_PIN, LOW);
    playTone(440, 130);
    delay(30);
  }
}

void playDisputeChime() {
  Serial.println(F("[AUDIO] >> Playing Dispute Confirmation Chime"));
  playTone(440, 150);
  delay(40);
  playTone(554, 150);
  delay(40);
  playTone(659, 250);
}

void playClickTone() {
  playTone(1200, 30);
}

// ============================================================================
// 10. TOUCHSCREEN READING & BUTTON DETECTION
// ============================================================================

bool readTouch(int &screenX, int &screenY) {
  if (!touch.touched()) return false;

  TS_Point p = touch.getPoint();

  int x = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_WIDTH - 1);
  int y = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_HEIGHT - 1);

  x = constrain(x, 0, SCREEN_WIDTH - 1);
  y = constrain(y, 0, SCREEN_HEIGHT - 1);

  if (TOUCH_SWAP_XY) { int tmp = x; x = y; y = tmp; }
  if (TOUCH_INVERT_X) { x = SCREEN_WIDTH - 1 - x; }
  if (TOUCH_INVERT_Y) { y = SCREEN_HEIGHT - 1 - y; }

  screenX = x;
  screenY = y;
  return true;
}

void checkTouch() {
  int x, y;
  if (!readTouch(x, y)) return;

  if (millis() - lastTouchTime < TOUCH_DEBOUNCE) return;
  lastTouchTime = millis();

  Serial.printf("[TOUCH] Screen touched at X: %d, Y: %d\n", x, y);
  playClickTone();

  // 1. Verify Button Pressed
  if (x >= VERIFY_X && x <= (VERIFY_X + VERIFY_W) &&
      y >= VERIFY_Y && y <= (VERIFY_Y + VERIFY_H)) {
    Serial.println(F("[BUTTON] [VERIFY] pressed!"));
    verifyTransactionWithServer(transactionAmount);
    return;
  }

  // 2. Dispute Button Pressed
  if (x >= DISPUTE_X && x <= (DISPUTE_X + DISPUTE_W) &&
      y >= DISPUTE_Y && y <= (DISPUTE_Y + DISPUTE_H)) {
    Serial.println(F("[BUTTON] [DISPUTE] pressed!"));
    reportDisputeToServer();
    return;
  }
}

// ============================================================================
// 11. MICROPHONE AUDIO TRIGGER
// ============================================================================

void checkMicrophone() {
  int digitalSound = digitalRead(MIC_D0);
  int analogSound = analogRead(MIC_A0);

  // If sudden loud acoustic trigger (merchant saying "Kitna aaya?"), blink LED
  if (digitalSound == HIGH || analogSound > 2800) {
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
  }
}

// ============================================================================
// 12. BACKEND API CALLS (VERIFICATION & DISPUTES)
// ============================================================================

void verifyTransactionWithServer(double expectedAmount) {
  showVerifyingScreen("Querying Paytm Gateway & ML Risk Engine");

  if (WiFi.status() != WL_CONNECTED) {
    showNetworkErrorScreen("Wi-Fi Offline. Connect device to internet.");
    return;
  }

  HTTPClient http;
  WiFiClient client;
  WiFiClientSecure secureClient;

  bool isHttps = BACKEND_API_URL.startsWith("https://");
  if (isHttps) {
    secureClient.setInsecure();
    http.begin(secureClient, BACKEND_API_URL);
  } else {
    http.begin(client, BACKEND_API_URL);
  }

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  // Build JSON Payload
  String requestBody = "{";
  requestBody += "\"merchantId\":\"" + merchantId + "\",";
  requestBody += "\"orderId\":\"" + transactionId + "\",";
  requestBody += "\"transactionId\":\"" + transactionId + "\",";
  requestBody += "\"payload\":{\"amount\":" + String(expectedAmount > 0 ? expectedAmount : transactionAmount, 2) + "},";
  requestBody += "\"action\":\"verify\"";
  requestBody += "}";

  Serial.println(F("\n[HTTP] Sending Verification Request:"));
  Serial.println(requestBody);

  int httpCode = http.POST(requestBody);
  Serial.printf("[HTTP] Response Code: %d\n", httpCode);

  digitalWrite(LED_PIN, LOW);

  if (httpCode > 0) {
    String response = http.getString();
    Serial.println(F("[HTTP] Server Response:"));
    Serial.println(response);
    http.end();

    // Parse Response Fields
    float fraudScore = 0.0;
    int scoreIdx = response.indexOf("\"fraudScore\":");
    if (scoreIdx >= 0) {
      fraudScore = response.substring(scoreIdx + 13).toFloat();
    }

    double verifiedAmount = transactionAmount;
    int amtIdx = response.indexOf("\"amount\":");
    if (amtIdx >= 0) {
      verifiedAmount = response.substring(amtIdx + 9).toFloat();
    }

    String status = "";
    int statusIdx = response.indexOf("\"status\":\"");
    if (statusIdx >= 0) {
      int endStatus = response.indexOf("\"", statusIdx + 10);
      status = response.substring(statusIdx + 10, endStatus);
    }

    String reason = "No matching bank credit found.";
    int msgIdx = response.indexOf("\"message\":\"");
    if (msgIdx >= 0) {
      int endMsg = response.indexOf("\"", msgIdx + 11);
      reason = response.substring(msgIdx + 11, endMsg);
    }

    // Evaluate Result
    if (fraudScore >= 0.70 || status == "flagged_fraud") {
      showFraudAlertScreen(expectedAmount > 0 ? expectedAmount : transactionAmount, fraudScore, reason.c_str());
    } else if (status == "not_found") {
      showFraudAlertScreen(0.0, 1.0, "Transaction record not found in Paytm Gateway");
    } else {
      showSuccessScreen(verifiedAmount, fraudScore);
    }
  } else {
    String err = http.errorToString(httpCode);
    Serial.printf("[HTTP] Request Error: %s\n", err.c_str());
    http.end();
    showNetworkErrorScreen(err.c_str());
  }
}

void reportDisputeToServer() {
  showVerifyingScreen("Submitting dispute ticket to Paytm Ops");

  if (WiFi.status() != WL_CONNECTED) {
    showNetworkErrorScreen("Wi-Fi Offline. Cannot submit dispute.");
    return;
  }

  HTTPClient http;
  WiFiClient client;
  WiFiClientSecure secureClient;

  // Choose dispute endpoint
  String disputeUrl = BACKEND_API_URL;
  if (disputeUrl.endsWith("/verify")) {
    disputeUrl.replace("/verify", "/dispute");
  }

  bool isHttps = disputeUrl.startsWith("https://");
  if (isHttps) {
    secureClient.setInsecure();
    http.begin(secureClient, disputeUrl);
  } else {
    http.begin(client, disputeUrl);
  }

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  String requestBody = "{";
  requestBody += "\"merchantId\":\"" + merchantId + "\",";
  requestBody += "\"orderId\":\"" + transactionId + "\",";
  requestBody += "\"transactionId\":\"" + transactionId + "\",";
  requestBody += "\"reason\":\"Merchant reported suspicious payment on terminal\",";
  requestBody += "\"action\":\"dispute\"";
  requestBody += "}";

  Serial.println(F("\n[DISPUTE] Sending Dispute Payload:"));
  Serial.println(requestBody);

  int httpCode = http.POST(requestBody);
  Serial.printf("[DISPUTE] Response Code: %d\n", httpCode);

  digitalWrite(LED_PIN, LOW);

  if (httpCode > 0) {
    String response = http.getString();
    Serial.println(F("[DISPUTE] Server Response:"));
    Serial.println(response);
    http.end();

    String ticketId = "DSP-9042";
    int tktIdx = response.indexOf("\"ticketId\":\"");
    if (tktIdx >= 0) {
      int endTkt = response.indexOf("\"", tktIdx + 12);
      ticketId = response.substring(tktIdx + 12, endTkt);
    }

    showDisputeScreen(ticketId.c_str());
  } else {
    http.end();
    showNetworkErrorScreen("Failed to reach dispute server");
  }
}

void sendHeartbeatToServer() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  WiFiClient client;
  WiFiClientSecure secureClient;

  // Derive heartbeat URL from BACKEND_API_URL
  String hbUrl = BACKEND_API_URL;
  int apiPos = hbUrl.indexOf("/api/");
  if (apiPos != -1) {
    hbUrl = hbUrl.substring(0, apiPos) + "/api/heartbeat";
  } else {
    hbUrl = hbUrl + "/heartbeat";
  }

  bool isHttps = hbUrl.startsWith("https://");
  if (isHttps) {
    secureClient.setInsecure();
    http.begin(secureClient, hbUrl);
  } else {
    http.begin(client, hbUrl);
  }

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);

  String payload = "{";
  payload += "\"deviceId\":\"TBX-ESP32-001\",";
  payload += "\"merchantId\":\"" + merchantId + "\",";
  payload += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  payload += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  payload += "\"firmware\":\"v1.0.0-PROD\"";
  payload += "}";

  int httpCode = http.POST(payload);
  if (httpCode == 200) {
    Serial.printf("[HEARTBEAT] Pinged laptop backend OK (HTTP 200) | ESP32 IP: %s | Signal: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else if (httpCode > 0) {
    Serial.printf("[HEARTBEAT] Server reached (HTTP %d)\n", httpCode);
  } else {
    Serial.printf("[HEARTBEAT] Ping failed: %s (Check laptop IP in sketch: %s)\n", http.errorToString(httpCode).c_str(), hbUrl.c_str());
  }
  http.end();
}

// ============================================================================
// 13. SERIAL DIAGNOSTIC CLI (TEST WITHOUT TOUCHING SCREEN)
// ============================================================================

void printHelpMenu() {
  Serial.println(F("\n--- Paytm TrustBox Serial CLI Commands ---"));
  Serial.println(F("  STATUS              : Print network, pins, and order status"));
  Serial.println(F("  VERIFY              : Test verification of current order"));
  Serial.println(F("  VERIFY <amt>        : Test verify with custom amount (e.g. 'VERIFY 500')"));
  Serial.println(F("  DISPUTE             : Trigger dispute escalation"));
  Serial.println(F("  CHIME               : Test Paytm payment arrival chime"));
  Serial.println(F("  SIREN               : Test fraud warning siren"));
  Serial.println(F("  SCENARIO_A          : Test genuine payment (ORD-1001, INR 250)"));
  Serial.println(F("  SCENARIO_B          : Test fake screenshot (FAKE-ORD-999, INR 500)"));
  Serial.println(F("  SET_TXN <id> <amt>  : Change active transaction"));
  Serial.println(F("  SET_SERVER <url>    : Update backend server URL"));
  Serial.println(F("------------------------------------------\n"));
}

void processSerialCli() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;

  Serial.printf("\n[CLI] Command: '%s'\n", cmd.c_str());

  if (cmd.equalsIgnoreCase("HELP")) {
    printHelpMenu();
  } else if (cmd.equalsIgnoreCase("STATUS")) {
    Serial.printf("Merchant: %s (%s)\n", merchantName.c_str(), merchantId.c_str());
    Serial.printf("Order ID: %s | Amount: INR %.2f\n", transactionId.c_str(), transactionAmount);
    Serial.printf("Wi-Fi Status: %s | IP: %s\n", wifiConnected ? "CONNECTED" : "OFFLINE", WiFi.localIP().toString().c_str());
    Serial.printf("Backend URL: %s\n", BACKEND_API_URL.c_str());
    Serial.printf("Pins: TFT_CS=%d, TOUCH_CS=%d, BUZZER=%d, LED=%d, MIC_D0=%d, MIC_A0=%d\n",
                  TFT_CS, TOUCH_CS, BUZZER_PIN, LED_PIN, MIC_D0, MIC_A0);
  } else if (cmd.equalsIgnoreCase("VERIFY")) {
    verifyTransactionWithServer(transactionAmount);
  } else if (cmd.startsWith("VERIFY ") || cmd.startsWith("verify ")) {
    double customAmt = cmd.substring(7).toDouble();
    verifyTransactionWithServer(customAmt);
  } else if (cmd.equalsIgnoreCase("DISPUTE")) {
    reportDisputeToServer();
  } else if (cmd.equalsIgnoreCase("CHIME")) {
    playPaytmChime();
  } else if (cmd.equalsIgnoreCase("SIREN")) {
    playFraudSiren();
  } else if (cmd.equalsIgnoreCase("SCENARIO_A")) {
    Serial.println(F("[SCENARIO A] Testing Genuine Payment (ORD-1001)..."));
    transactionId = "ORD-1001";
    transactionAmount = 250.0;
    showHomeScreen();
    delay(500);
    verifyTransactionWithServer(250.0);
  } else if (cmd.equalsIgnoreCase("SCENARIO_B")) {
    Serial.println(F("[SCENARIO B] Testing Fake Screenshot Scam (FAKE-ORD-999)..."));
    transactionId = "FAKE-ORD-999";
    transactionAmount = 500.0;
    showHomeScreen();
    delay(500);
    verifyTransactionWithServer(500.0);
  } else if (cmd.startsWith("SET_TXN ")) {
    int space = cmd.indexOf(' ', 8);
    if (space > 0) {
      transactionId = cmd.substring(8, space);
      transactionAmount = cmd.substring(space + 1).toDouble();
    } else {
      transactionId = cmd.substring(8);
    }
    Serial.printf("[CLI] Transaction updated: %s (INR %.2f)\n", transactionId.c_str(), transactionAmount);
    showHomeScreen();
  } else if (cmd.startsWith("SET_SERVER ")) {
    BACKEND_API_URL = cmd.substring(11);
    BACKEND_API_URL.trim();
    Serial.printf("[CLI] Backend URL set to: %s\n", BACKEND_API_URL.c_str());
  } else {
    Serial.println(F("[CLI] Unknown command. Type 'HELP' for instructions."));
  }
}
