/*
 * Copyright (C) 2018-2020 Alibaba Group Holding Limited
 */

#if defined(CONFIG_DECODER_WMA) && CONFIG_DECODER_WMA

#include "avutil/common.h"
#include "avutil/byte_rw.h"
#include "avutil/mem_block.h"
#include "avformat/adts_rw.h"
#include "avcodec/ad_cls.h"
#include "wmafixed/wmadec.h"

#define TAG                    "ad_wma"
#define WMA_OBUF_SIZE          (2048 * 2 * sizeof(int16_t))
#define WMA_COMPRESS_RATIO_MAX (20)

struct ad_wma_priv {
    WMADecodeContext      wcxt;
    sf_t                  ori_sf;       ///< FIXME: bit=> 32bps after decode
    mblock_t              *omb;
};

static int _wma_open(struct ad_wma_priv *priv, sh_audio_t *ash)
{
    int rc;
    asf_waveformatex_t  wfx;
    sf_t sf = priv->ori_sf;
    WMADecodeContext *hdl = &priv->wcxt;
    uint8_t *extradata    = ash->extradata;
    size_t extradata_size = ash->extradata_size;

    if (!(ash->block_align && sf_get_channel(sf) && sf_get_rate(sf) && sf_get_bit(sf) && ash->bps)) {
        LOGE(TAG, "ash invalid. block_align = %u, bps = %u, sf=> %s", ash->block_align, ash->bps, sf_get_format_str(sf));
        return -1;
    }

    memset(&wfx, 0, sizeof(asf_waveformatex_t));
    wfx.rate          = sf_get_rate(sf);
    wfx.bitrate       = ash->bps;
    wfx.channels      = sf_get_channel(sf);
    wfx.blockalign    = ash->block_align;
    wfx.bitspersample = sf_get_bit(sf);
    wfx.codec_id      = ash->id == AVCODEC_ID_WMAV1 ? ASF_CODEC_ID_WMAV1 : ASF_CODEC_ID_WMAV2;
    if (extradata_size && extradata) {
        extradata_size = extradata_size > 6 ? 6 : extradata_size;
        wfx.datalen    = extradata_size;
        memcpy(wfx.data, extradata, extradata_size);
    }

    rc = wma_decode_init(hdl, &wfx);
    CHECK_RET_TAG_WITH_RET(rc == 0, -1);

    return 0;
}

static int _ad_wma_open(ad_cls_t *o)
{
    int rc;
    sf_t sf;
    int bits = 32;
    struct ad_wma_priv *priv;
    mblock_t *omb  = NULL;

    priv = aos_zalloc(sizeof(struct ad_wma_priv));
    CHECK_RET_TAG_WITH_RET(priv, -1);
    sf           = o->ash.sf;
    priv->ori_sf = o->ash.sf;

    omb = mblock_new(WMA_OBUF_SIZE, 0);
    CHECK_RET_TAG_WITH_GOTO(omb, err);

    rc = _wma_open(priv, &o->ash);
    CHECK_RET_TAG_WITH_GOTO(rc == 0, err);
    priv->omb = omb;
    o->ash.sf = sf_make_channel(sf_get_channel(sf)) | sf_make_rate(sf_get_rate(sf)) | sf_make_bit(bits) | sf_make_signed(bits > 8);
    o->priv   = priv;

    return 0;
err:
    mblock_free(omb);
    aos_free(priv);
    return -1;
}

static int _ad_wma_decode(ad_cls_t *o, avframe_t *frame, int *got_frame, const avpacket_t *pkt)
{
    int rc = -1;
    int64_t v;
    int32_t *psample;
    sf_t sf = o->ash.sf;
    int nb_samples, i, total_samples = 0, first = 1;
    struct ad_wma_priv *priv = o->priv;
    mblock_t *omb            = priv->omb;
    WMADecodeContext *hdl    = &priv->wcxt;

    rc = wma_decode_superframe_init(hdl, pkt->data, pkt->len);
    CHECK_RET_TAG_WITH_GOTO((rc > 0) && (hdl->nb_frames > 0), err);

    rc = mblock_grow(omb, pkt->len * WMA_COMPRESS_RATIO_MAX);
    CHECK_RET_TAG_WITH_GOTO(rc == 0, err);

    for (i = 0; i < hdl->nb_frames; i++) {
        nb_samples = wma_decode_superframe_frame(hdl, (int32_t*)((char*)omb->data + total_samples * sf_get_frame_size(sf)), pkt->data, pkt->len);
        CHECK_RET_TAG_WITH_GOTO(nb_samples > 0, err);
        total_samples += nb_samples;
        if (first && hdl->nb_frames > 1) {
            int new_msize = (nb_samples + 128) * hdl->nb_frames * sf_get_frame_size(sf);

            rc = mblock_grow(omb, new_msize);
            CHECK_RET_TAG_WITH_GOTO(rc == 0, err);
            first = 0;
        }
    }

    rc = total_samples * sf_get_channel(sf);
    psample = omb->data;
    //FIXME: may be over flow
    for (i = 0; i < rc; i++) {
        v          = (int64_t)psample[i] << 2;
        v          = v < 0x7fffffff ? v : 0x7fffffff;
        v          = v > -0x7fffffff ? v : -0x7fffffff;
        psample[i] = v;
    }

    frame->sf         = sf;
    frame->nb_samples = total_samples;
    rc = avframe_get_buffer(frame);
    if (rc < 0) {
        LOGD(TAG, "avframe_get_buffer failed, may be oom. nb_samples = %d, sf=> %s", total_samples, sf_get_format_str(sf));
        goto err;
    }
    memcpy((void*)frame->data[0], omb->data, total_samples * sf_get_frame_size(sf));
    *got_frame = 1;

    return pkt->len;
err:
    return -1;
}

static int _ad_wma_control(ad_cls_t *o, int cmd, void *arg, size_t *arg_size)
{
    //TODO
    return 0;
}

static int _ad_wma_reset(ad_cls_t *o)
{
    int rc;
    struct ad_wma_priv *priv = o->priv;
    WMADecodeContext *hdl    = &priv->wcxt;

    memset(hdl, 0, sizeof(WMADecodeContext));
    rc = _wma_open(priv, &o->ash);

    return rc;
}

static int _ad_wma_close(ad_cls_t *o)
{
    struct ad_wma_priv *priv = o->priv;

    mblock_free(priv->omb);
    aos_free(priv);
    o->priv = NULL;
    return 0;
}

const struct ad_ops ad_ops_wma = {
    .name           = "wma",
    .id             = AVCODEC_ID_WMAV1 | AVCODEC_ID_WMAV2,

    .open           = _ad_wma_open,
    .decode         = _ad_wma_decode,
    .control        = _ad_wma_control,
    .reset          = _ad_wma_reset,
    .close          = _ad_wma_close,
};

#endif

