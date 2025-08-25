FATE_SAMPLES_CJIPROBE-$(call DEMDEC, MOV, AAC) += fate-cji-probe-aac
fate-cji-probe-aac: CMD = cjiprobe $(TARGET_SAMPLES)/cji/Test_MPEG_DRC_lc_stereo.mp4 -show_entries stream=adts,ext_flags,drc,prog_ref_level

FATE_SAMPLES_CJIPROBE-$(call DEMDEC, MPEGTS, HEVC) += fate-cji-probe-hevc
fate-cji-probe-hevc: CMD = cjiprobe $(TARGET_SAMPLES)/cji/320x240_hevc.ts -show_entries stream=field_order

FATE_SAMPLES_CJIPROBE += $(FATE_SAMPLES_CJIPROBE-yes)
FATE_SAMPLES_FFPROBE += $(FATE_SAMPLES_CJIPROBE)
fate-cji: $(FATE_SAMPLES_CJIPROBE)
