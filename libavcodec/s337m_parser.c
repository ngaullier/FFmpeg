/*
 * S337M parser
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

#include "libavutil/intreadwrite.h"
#include "libavutil/mem.h"

#include "parser.h"
#include "spdif_s337m_parser_internal.h"

static int all_zero(const uint8_t *buf, int size)
{
    int i = 0;
#if HAVE_FAST_UNALIGNED
    /* we check i < size instead of i + 3 / 7 because it is
     * simpler and there must be AV_INPUT_BUFFER_PADDING_SIZE
     * bytes at the end.
     */
#if HAVE_FAST_64BIT
    while (i < size && !AV_RN64(buf + i))
        i += 8;
#else
    while (i < size && !AV_RN32(buf + i))
        i += 4;
#endif
#endif
    for (; i < size; i++)
        if (buf[i])
            return 1;
    return 0;
}

extern const AVCodecParser ff_s337m_24_parser;

static int s337m_parse(AVCodecParserContext *s,
                           AVCodecContext *avctx,
                           const uint8_t **poutbuf, int *poutbuf_size,
                           const uint8_t *buf, int buf_size)
{
    struct SPDIFS337MParseContext *pc1 = s->priv_data;
    ParseContext *pc = &pc1->pc;
    int aes_word_bits = s->parser == &ff_s337m_24_parser ? 24 : 16;
    int eof = !buf_size;
    int next;

    if (s->flags & PARSER_FLAG_COMPLETE_FRAMES) {
        next = buf_size;
    } else {
        next = avpriv_spdif_s337m_find_syncword(pc1, buf, buf_size, aes_word_bits);

        if (!pc1->inited) {
            /* bytes preceding the first syncword will be zeroed */
            if (all_zero(buf, next != END_NOT_FOUND ? next : buf_size)) {
                if (!pc1->warned_corrupted_guardband) {
                    av_log(avctx, AV_LOG_VERBOSE,
                            "Guard band has unexpected non-null bytes - they will be ignored.\n");
                    pc1->warned_corrupted_guardband = 1;
                }
                if (buf_size > pc1->null_buf_size) {
                    int old_buf_size = pc1->null_buf_size;
                    int8_t *new_buf = av_realloc(pc1->null_buf, buf_size);
                    if (!new_buf)
                        return AVERROR(ENOMEM);
                    pc1->null_buf = new_buf;
                    memset(&pc1->null_buf[old_buf_size], 0, buf_size - old_buf_size);
                    pc1->null_buf_size = buf_size;
                }
                buf = pc1->null_buf;
            }
            if (next != END_NOT_FOUND) {
                pc1->inited = 1;
                pc1->aes_initial_offset = pc1->aes_offset + next;
            }
        }
        if (ff_combine_frame(pc, next, &buf, &buf_size) < 0) {
            pc1->aes_offset += buf_size;
            *poutbuf = NULL;
            *poutbuf_size = 0;
            return buf_size;
        }
    }
    /* Contrary to the exact frame duration computed by the decoder, the packet duration
     * computed here will reflect s337m jitter or phase change (if any),
     * but there will not be any drift.
     * The content/duration of the initial guard band (zeroed first packet) will be
     * ignored by the decoder if it is less than a full audio frame, so that the overall
     * duration may differ between the parser and the decoder.
     */
    pc1->aes_offset += eof ? pc1->aes_initial_offset : next;
    s->duration = (pc1->aes_offset << 2) / aes_word_bits;
    pc1->aes_offset = 0;

    *poutbuf = buf;
    *poutbuf_size = buf_size;
    return next;
}

static void s337m_close(AVCodecParserContext *s)
{
    struct SPDIFS337MParseContext *pc1 = s->priv_data;

    av_freep(&pc1->null_buf);
    ff_parse_close(s);
}

const AVCodecParser ff_s337m_16_parser = {
    .codec_ids      = { AV_CODEC_ID_S337M_16 },
    .priv_data_size = sizeof(SPDIFS337MParseContext),
    .parser_parse   = s337m_parse,
    .parser_close   = s337m_close,
};
const AVCodecParser ff_s337m_24_parser = {
    .codec_ids      = { AV_CODEC_ID_S337M_24 },
    .priv_data_size = sizeof(SPDIFS337MParseContext),
    .parser_parse   = s337m_parse,
    .parser_close   = s337m_close,
};
