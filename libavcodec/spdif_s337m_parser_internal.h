/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_SPDIF_S337M_PARSER_INTERNAL_H
#define AVCODEC_SPDIF_S337M_PARSER_INTERNAL_H

#include "parser.h"

#define MARKER_16LE         0x72F81F4E
#define MARKER_20LE         0x20876FF0E154
#define MARKER_24LE         0x72F8961F4EA5

#define IS_16LE_MARKER(state)   ((state & 0xFFFFFFFF) == MARKER_16LE)
#define IS_20LE_MARKER(state)   ((state & 0xF0FFFFF0FFFF) == MARKER_20LE)
#define IS_24LE_MARKER(state)   ((state & 0xFFFFFFFFFFFF) == MARKER_24LE)

/**
 * @struct SPDIFS337MContext
 * Context for use by decoder and parser.
 */
typedef struct SPDIFS337MContext {
    void        *avctx;

    int         frame_size;
    enum AVCodecID codec;
} SPDIFS337MContext;

/**
 * @struct SPDIFS337MParseContext
 * Context for use by parser to split packets.
 */
typedef struct SPDIFS337MParseContext {
    ParseContext pc;
    uint64_t     state_ext;

    int          inited;
    int          aes_initial_offset;
    int          aes_offset;

    uint8_t      *null_buf;
    int          null_buf_size;
    int          warned_corrupted_guardband;
} SPDIFS337MParseContext;

void avpriv_spdif_s337m_bswap_buf16(uint16_t *dst, const uint16_t *src, int w);
void avpriv_spdif_s337m_bswap_buf24(uint8_t *data, int size);

/**
 * Find an 'extended sync code' as suggested by SMPTE 337M Annex A.
 * Its length is set to 128 bits for optimization, while Annex A
 * suggests 6 words (96 bits for 16-bit or 144 bits for 20/24-bit).
 *
 * Do not require sync byte alignment on container word bytes,
 * but still, 16-bit payloads require 16-bit container
 * and 20/24-bit payloads require 24-bit container.
 * Note: SPDIF is always 16-bit.
 *
 * @param  pc1              To persist states between calls
 * @param  buf              Buffer for reading
 * @param  buf_size         Max available bytes to read
 * @param  word_bits        Buffer word size: 16 or 24
 * @return syncword byte position on success, or END_NOT_FOUND
 **/
int avpriv_spdif_s337m_find_syncword(SPDIFS337MParseContext *pc1, const uint8_t *buf, int buf_size, int word_bits);

/**
 * Parse s337m header: get codec and frame_size.
 *
 * @param  avc              If not null, codec and frame_size will be set
 * @param  buf              Buffer for reading with s337m syncword at byte position 0
 * @param  buf_size         Max available bytes to read
 * @param  s337m_word_bits  Buffer word size: 16 or 24
 * @return header size > 0 on success, 0 if the first two words are null, or < 0 on error
 */
int avpriv_s337m_parse_header(SPDIFS337MContext *avc, const uint8_t *buf, int buf_size, int s337m_word_bits);

#endif /* AVCODEC_SPDIF_S337M_PARSER_INTERNAL_H */
