/**
 * @file DebugOled.cpp
 * @brief OLED display utilities for debugging output.
 *
 * Simplified OLED helper for SSD1306 128x32 using U8g2 in page buffer mode (1 page).
 * Provides print / println / printf from RAM or PROGMEM, with automatic scroll.
 *
 * @author MrJ
 * @date 2025-09-03
 * @license MIT License
 */

#ifdef DEBUG_OLED

#include "DebugOled.h"
#include <avr/pgmspace.h>
#include <stdarg.h>

// OLED object declaration using page buffer mode (1 page)
U8G2_SSD1306_128X32_UNIVISION_1_SW_I2C oled(U8G2_R0, SCL, SDA);

// Error string stored in PROGMEM
static const char errorStr[] PROGMEM = "Error";

// Current display line (0 to 3 for a 128x32 OLED screen)
static uint8_t current_row = 0;

// Static buffer for storing up to 4 lines
static char buffer[4][32]; // 4 lines, max 32 chars each

// Forward
static void oled_vprint(bool newline, const char *fmt, va_list args);

/**
 * @brief Initializes the OLED display
 */
void oled_init(long speed)
{
    (void)speed; // unused for SW I2C
    oled.begin();
    delay(200);
    oled.setContrast(128);
    oled.setFont(u8g2_font_4x6_tr);
    oled.clearBuffer();
    oled.sendBuffer();
    delay(50);

    current_row = 0;
    for (uint8_t i = 0; i < 4; i++)
    {
        buffer[i][0] = '\0';
    }
}

/**
 * @brief Draws all lines stored in the buffer to the OLED display
 */
void oled_draw()
{
    const uint8_t line_height = oled.getMaxCharHeight();
    const uint8_t base_offset = oled.getAscent();

    oled.firstPage();
    do
    {
        for (uint8_t row = 0; row < 4; row++)
        {
            if (buffer[row][0] != '\0')
            {
                oled.drawStr(0, base_offset + row * line_height, buffer[row]);
            }
        }
    } while (oled.nextPage());
}

/**
 * @brief Scrolls buffer upwards if needed
 */
void oled_scroll()
{
    if (current_row >= 4)
    {
        // Shift all lines up
        for (uint8_t i = 0; i < 3; i++)
        {
            strncpy(buffer[i], buffer[i + 1], sizeof(buffer[0]) - 1);
            buffer[i][sizeof(buffer[0]) - 1] = '\0';
        }
        buffer[3][0] = '\0';
        current_row = 3;
    }
}

/**
 * @brief Clears the display and resets the buffer
 */
void oled_clear()
{
    current_row = 0;
    oled.clearBuffer();
    oled.sendBuffer();
    delay(20);

    for (uint8_t i = 0; i < 4; i++)
    {
        buffer[i][0] = '\0';
    }
}

// --- internal unified append-print handler ---
static void oled_vprint(bool newline, const char *fmt, va_list args)
{
    // garde ton comportement : on laisse le scroll en place
    oled_scroll();

    char *dst = buffer[current_row];
    const size_t dst_size = sizeof(buffer[0]); // 32
    size_t curr_len = strlen(dst);             // longueur déjà présente
    if (curr_len >= dst_size - 1)
    {
        // ligne déjà pleine -> rien à ajouter (on redessine quand même)
        oled_draw();
        if (newline)
            current_row++;
        return;
    }

    size_t avail = dst_size - curr_len - 1; // place dispo (excl. '\0')
    char tmp[64];                           // tmp >= possible avail (32) — safe

    // vsnprintf consume va_list, dupliquer avant usage
    va_list args_copy;
    va_copy(args_copy, args);

    int n;
    if (pgm_read_byte(fmt))
    {
        // fmt en PROGMEM
        n = vsnprintf_P(tmp, avail + 1, (PGM_P)fmt, args_copy);
    }
    else
    {
        // fmt en RAM
        n = vsnprintf(tmp, avail + 1, fmt, args_copy);
    }
    va_end(args_copy);

    if (n <= 0)
    {
        // erreur de formatage -> mettre "Error" (depuis PROGMEM)
        strncpy_P(&dst[curr_len], errorStr, avail);
        dst[curr_len + avail] = '\0';
    }
    else
    {
        // n = nombre de caractères 'voulu' (peut > avail). On copie au max 'avail'
        size_t to_copy = (size_t)n;
        if (to_copy > avail)
            to_copy = avail;
        memcpy(dst + curr_len, tmp, to_copy);
        dst[curr_len + to_copy] = '\0';
    }

    oled_draw();

    if (newline)
    {
        current_row++;
    }
}

// --- RAM wrappers ---
void oled_print(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    oled_vprint(false, fmt, args);
    va_end(args);
}

void oled_println(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    oled_vprint(true, fmt, args);
    va_end(args);
}

void oled_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    oled_vprint(false, fmt, args);
    va_end(args);
}

// --- PROGMEM (__FlashStringHelper*) wrappers ---
void oled_print(const __FlashStringHelper *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    oled_vprint(false, (const char *)fmt, args);
    va_end(args);
}

void oled_println(const __FlashStringHelper *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    oled_vprint(true, (const char *)fmt, args);
    va_end(args);
}

void oled_printf(const __FlashStringHelper *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    oled_vprint(false, (const char *)fmt, args);
    va_end(args);
}

#endif