/*
 * SPDIF/S337M common code
 *
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

#include "libavutil/bswap.h"
#include "codec_id.h"
#include "bytestream.h"

#include "spdif_s337m_parser_internal.h"

//TODO move to DSP
void avpriv_spdif_s337m_bswap_buf16(uint16_t *dst, const uint16_t *src, int w)
{
    int i;

    for (i = 0; i + 8 <= w; i += 8) {
        dst[i + 0] = av_bswap16(src[i + 0]);
        dst[i + 1] = av_bswap16(src[i + 1]);
        dst[i + 2] = av_bswap16(src[i + 2]);
        dst[i + 3] = av_bswap16(src[i + 3]);
        dst[i + 4] = av_bswap16(src[i + 4]);
        dst[i + 5] = av_bswap16(src[i + 5]);
        dst[i + 6] = av_bswap16(src[i + 6]);
        dst[i + 7] = av_bswap16(src[i + 7]);
    }
    for (; i < w; i++)
        dst[i + 0] = av_bswap16(src[i + 0]);
}

void avpriv_spdif_s337m_bswap_buf24(uint8_t *data, int size)
{
    int i;

    for (i = 0; i < size / 3; i++, data += 3)
        FFSWAP(uint8_t, data[0], data[2]);
}

int avpriv_spdif_s337m_find_syncword(SPDIFS337MParseContext *pc1, const uint8_t *buf, int buf_size, int word_bits)
{
    ParseContext *pc = &pc1->pc;
    uint64_t state = pc->state64;
    uint64_t state_ext = pc1->state_ext;
    int i = 0;

    for (; i < buf_size; i++) {
        state_ext = (state_ext >> 8) | state & 0xFF00000000000000;
        state = (state << 8) | buf[i];
        if (state_ext
            || word_bits == 16 && state != MARKER_16LE
            || word_bits == 24 && state != MARKER_20LE && state != MARKER_24LE)
            continue;

        pc->state64 = -1;
        return state == MARKER_16LE ? i - 3 : i - 5;
    }

    pc->state64 = state;
    pc1->state_ext = state_ext;
    return END_NOT_FOUND;
}

static int s337m_get_codec(SPDIFS337MContext *dectx, uint64_t state, int data_type, int data_size, int s337m_word_bits)
{
    int word_bits;

    if (IS_16LE_MARKER(state)) {
        word_bits = 16;
    } else if (IS_20LE_MARKER(state)) {
        data_type >>= 8;
        data_size >>= 4;
        word_bits = 20;
    } else if (IS_24LE_MARKER(state)) {
        data_type >>= 8;
        word_bits = 24;
    } else return AVERROR_INVALIDDATA;

    if (!(s337m_word_bits == 16 && word_bits == 16) &&
        !(s337m_word_bits == 24 && word_bits == 20) &&
        !(s337m_word_bits == 24 && word_bits == 24)) {
        if (dectx && dectx->avctx)
            av_log(dectx->avctx, AV_LOG_ERROR, "s337m: unexpected %d-bit payload in %d-bit container\n", word_bits, s337m_word_bits);
        return AVERROR_INVALIDDATA;
    }

    switch(data_type & 0x1F) {
        case 0x1C:
            if (dectx) {
                dectx->frame_size = (word_bits + 7 >> 3) * data_size / word_bits;
                dectx->codec = AV_CODEC_ID_DOLBY_E;
            }
            break;

        default:
            /* When probing 16-bit streams, spdif codecs can be encountered. */
            if (dectx && dectx->avctx)
                avpriv_report_missing_feature(dectx->avctx, "Data type %#x in SMPTE 337M", data_type & 0x1F);
            return dectx ? AVERROR_PATCHWELCOME : AVERROR_INVALIDDATA;
    }

    return 0;
}

int avpriv_s337m_parse_header(SPDIFS337MContext *dectx, const uint8_t *buf, int buf_size, int s337m_word_bits)
{
    uint64_t state;
    int data_type, data_size, next = s337m_word_bits >> 1;
    int ret = 0;

    if (buf_size < next)
        return AVERROR_BUFFER_TOO_SMALL;
    if (s337m_word_bits == 16) {
        state = AV_RB32(buf);
        data_type = AV_RL16(buf + 4);
        data_size = AV_RL16(buf + 6);
    } else {
        state = AV_RB48(buf);
        data_type = AV_RL24(buf + 6);
        data_size = AV_RL24(buf + 9);
    }
    if (!state)
        return 0;
    if (ret = s337m_get_codec(dectx, state, data_type, data_size, s337m_word_bits))
        return ret;

    return next;
}
