/*
 * SMPTE ST 337 common header
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

#ifndef AVFORMAT_S337M_H
#define AVFORMAT_S337M_H

#define S337M_MIN_OFFSET 1601*4
#define S337M_MAX_OFFSET 2002*6

#define S337M_PROBE_GUARDBAND_MIN_BYTES     0

/**
 * Detect s337m packets in a PCM_S16LE/S24LE stereo stream
 * Requires 3 samples with enough (S337M_PROBE_GUARDBAND_MIN_BYTES) and clean (set to zero) guard band
 * @param p_buf Buffer
 * @param size Buffer size
 * @param codec Returns AV_CODEC_ID_DOLBY_E upon successful probing
 * @param container_word_bits 16 or 24
 * @return = AVPROBE_SCORE
 */
int ff_s337m_probe(const uint8_t *p_buf, int size, enum AVCodecID *codec, int container_word_bits);

/**
 * Read s337m packets in a PCM_S16LE/S24LE stereo stream
 * Returns the first inner packet found
 * Note that it does not require a clean guard band
 * @param pb Associated IO context
 * @param pkt On success, returns a DOLBY E packet
 * @param size Maximum IO read size available for reading at current position
 * @param codec Returns AV_CODEC_ID_DOLBY_E
 * @param avc For av_log
 * @param container_word_bits 16,24, or 0 for autodetect
 * @return = 0 on success (an error is raised if no s337m was found)
 */
int ff_s337m_get_packet(AVIOContext *pb, AVPacket *pkt, int size, enum AVCodecID *codec, void *avc, int container_word_bits);

#endif /* AVFORMAT_S337M_H */
