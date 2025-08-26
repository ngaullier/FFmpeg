FATE_SAMPLES_CJIPROBE-$(call DEMDEC, MOV, AAC) += fate-cji-probe-aac
fate-cji-probe-aac: CMD = cjiprobe $(TARGET_SAMPLES)/cji/Test_MPEG_DRC_lc_stereo.mp4 -show_entries stream=adts,ext_flags,drc,prog_ref_level

FATE_SAMPLES_CJIPROBE-$(call DEMDEC, MPEGTS, HEVC) += fate-cji-probe-hevc
fate-cji-probe-hevc: CMD = cjiprobe $(TARGET_SAMPLES)/cji/320x240_hevc.ts -show_entries stream=field_order

PROBE_WAV_S337M_COMMAND=cjiprobe $(TARGET_SAMPLES)/dolby_e/20-bit_5.1_2.0.wav -show_entries stream=codec_name,channels
PROBE_WAV_SPDIF_COMMAND=cjiprobe $(TARGET_SAMPLES)/ac3/diatonis_invisible_order_anfos_ac3-small.wav -show_entries stream=codec_name
PROBE_WAV_DTS_COMMAND=cjiprobe $(TARGET_SAMPLES)/cji/probe_streams/Fire__VooDoo_Studio_30_sec.wav -show_entries stream=codec_name
PROBE_TS_NOPATPMT=cjiprobe $(TARGET_SAMPLES)/cji/probe_streams/H264_AAC_TXT_Missing_PATPMT.ts -show_entries stream=codec_name

FATE_SAMPLES_CJIPROBE-$(call DEMDEC, WAV) += $(addprefix fate-cji-probe-wav-, s337m-0 s337m-1 spdif-0 spdif-1 dts-0 dts-1 )
FATE_SAMPLES_CJIPROBE-$(call DEMDEC, MPEGTS) += $(addprefix fate-cji-probe-ts-, nopatpmt-0 nopatpmt-1)
fate-cji-probe-wav-s337m-0: CMD = $(PROBE_WAV_S337M_COMMAND) -probe_streams 0
fate-cji-probe-wav-s337m-1: CMD = $(PROBE_WAV_S337M_COMMAND) -probe_streams 1
fate-cji-probe-wav-spdif-0: CMD = $(PROBE_WAV_SPDIF_COMMAND) -probe_streams 0
fate-cji-probe-wav-spdif-1: CMD = $(PROBE_WAV_SPDIF_COMMAND) -probe_streams 1
fate-cji-probe-wav-dts-0: CMD = $(PROBE_WAV_DTS_COMMAND) -probe_streams 0
fate-cji-probe-wav-dts-1: CMD = $(PROBE_WAV_DTS_COMMAND) -probe_streams 1
fate-cji-probe-ts-nopatpmt-0: CMD = $(PROBE_TS_NOPATPMT) -probe_streams 0
fate-cji-probe-ts-nopatpmt-1: CMD = $(PROBE_TS_NOPATPMT) -probe_streams 1

FATE_SAMPLES_CJIPROBE-$(call DEMDEC, MPEGTS) += $(addprefix fate-cji-probe-duration-, 50i_mp2 440_01 991_01)
fate-cji-probe-duration-50i_mp2: CMD = cjiprobe $(TARGET_SAMPLES)/cji/probe_durations/50i_mp2.ts -show_entries stream=duration
fate-cji-probe-duration-440_01: CMD = cjiprobe $(TARGET_SAMPLES)/cji/probe_durations/440_01.ts -show_entries stream=duration
fate-cji-probe-duration-991_01: CMD = cjiprobe $(TARGET_SAMPLES)/cji/concat/991_01.ts -show_entries stream=duration

FATE_SAMPLES_CJIFFMPEG-$(call FRAMECRC, MPEGTS) += $(addprefix fate-cji-demux-, et-a et-b)
fate-cji-demux-et-a: CMD = framecrc -i $(TARGET_SAMPLES)/cji/FirstNALsplited2packets_a.ts -map v -c copy
fate-cji-demux-et-b: CMD = framecrc -i $(TARGET_SAMPLES)/cji/FirstNALsplited2packets_b.ts -map v -c copy

FATE_SAMPLES_CJIFFMPEG-$(call FRAMECRC, MPEGTS) += $(addprefix fate-cji-concat-, 991)
fate-cji-concat-991: CMD = cjiconcat $(TARGET_SAMPLES)/cji/concat/991.ffconcat

FATE_SAMPLES_CJIPROBE += $(FATE_SAMPLES_CJIPROBE-yes)
FATE_SAMPLES_FFPROBE += $(FATE_SAMPLES_CJIPROBE)
FATE_SAMPLES_CJIFFMPEG += $(FATE_SAMPLES_CJIFFMPEG-yes)
FATE_SAMPLES_FFMPEG += $(FATE_SAMPLES_CJIFFMPEG)
fate-cji: $(FATE_SAMPLES_CJIPROBE) $(FATE_SAMPLES_FFMPEG)
