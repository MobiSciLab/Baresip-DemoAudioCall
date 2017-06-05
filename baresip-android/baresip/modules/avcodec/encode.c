/**
 * @file avcodec/encode.c  Video codecs using libavcodec -- encoder
 *
 * Copyright (C) 2010 - 2013 Creytiv.com
 */
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
#if LIBAVUTIL_VERSION_INT >= ((50<<16)+(29<<8)+0)
#include <libavutil/opt.h>
#else
#include <libavcodec/opt.h>
#endif
#ifdef USE_X264
#include <x264.h>
#endif
#include "h26x.h"
#include "avcodec.h"


#if LIBAVUTIL_VERSION_MAJOR < 52
#define AV_PIX_FMT_YUV420P PIX_FMT_YUV420P
#define AV_PIX_FMT_NV12    PIX_FMT_NV12
#endif


enum {
	DEFAULT_GOP_SIZE =   10,
};


struct picsz {
	enum h263_fmt fmt;  /**< Picture size */
	uint8_t mpi;        /**< Minimum Picture Interval (1-32) */
};


struct videnc_state {
	AVCodec *codec;
	AVCodecContext *ctx;
	AVFrame *pict;
	struct mbuf *mb;
	size_t sz_max; /* todo: figure out proper buffer size */
	int64_t pts;
	struct mbuf *mb_frag;
	struct videnc_param encprm;
	struct vidsz encsize;
	enum AVCodecID codec_id;
	videnc_packet_h *pkth;
	void *arg;

	union {
		struct {
			struct picsz picszv[8];
			uint32_t picszn;
		} h263;

		struct {
			uint32_t packetization_mode;
			uint32_t profile_idc;
			uint32_t profile_iop;
			uint32_t level_idc;
			uint32_t max_fs;
			uint32_t max_smbps;
		} h264;
	} u;

#ifdef USE_X264
	x264_t *x264;
#endif
};


static void destructor(void *arg)
{
	struct videnc_state *st = arg;

	mem_deref(st->mb);
	mem_deref(st->mb_frag);

#ifdef USE_X264
	if (st->x264)
		x264_encoder_close(st->x264);
#endif

	if (st->ctx) {
		if (st->ctx->codec)
			avcodec_close(st->ctx);
#if LIBAVUTIL_VERSION_INT >= ((51<<16)+(8<<8)+0)
		av_opt_free(st->ctx);
#endif
		av_free(st->ctx);
	}

	if (st->pict)
		av_free(st->pict);
}


static enum h263_fmt h263_fmt(const struct pl *name)
{
	if (0 == pl_strcasecmp(name, "sqcif")) return H263_FMT_SQCIF;
	if (0 == pl_strcasecmp(name, "qcif"))  return H263_FMT_QCIF;
	if (0 == pl_strcasecmp(name, "cif"))   return H263_FMT_CIF;
	if (0 == pl_strcasecmp(name, "cif4"))  return H263_FMT_4CIF;
	if (0 == pl_strcasecmp(name, "cif16")) return H263_FMT_16CIF;
	return H263_FMT_OTHER;
}


static int decode_sdpparam_h263(struct videnc_state *st, const struct pl *name,
				const struct pl *val)
{
	enum h263_fmt fmt = h263_fmt(name);
	const int mpi = pl_u32(val);

	if (fmt == H263_FMT_OTHER) {
		info("h263: unknown param '%r'\n", name);
		return 0;
	}
	if (mpi < 1 || mpi > 32) {
		info("h263: %r: MPI out of range %d\n", name, mpi);
		return 0;
	}

	if (st->u.h263.picszn >= ARRAY_SIZE(st->u.h263.picszv)) {
		info("h263: picszv overflow: %r\n", name);
		return 0;
	}

	st->u.h263.picszv[st->u.h263.picszn].fmt = fmt;
	st->u.h263.picszv[st->u.h263.picszn].mpi = mpi;

	++st->u.h263.picszn;

	return 0;
}


static int init_encoder(struct videnc_state *st)
{
	/*
	 * Special handling of H.264 encoder
	 */
	if (st->codec_id == AV_CODEC_ID_H264 && avcodec_h264enc) {

#ifdef USE_X264
		warning("avcodec: h264enc specified, but using libx264\n");
		return EINVAL;
#else
		st->codec = avcodec_h264enc;

		info("avcodec: h264 encoder activated\n");

		return 0;
#endif
	}

	st->codec = avcodec_find_encoder(st->codec_id);
	if (!st->codec)
		return ENOENT;

	return 0;
}


static int open_encoder(struct videnc_state *st,
			const struct videnc_param *prm,
			const struct vidsz *size,
			int pix_fmt)
{
	int err = 0;

	if (st->ctx) {
		if (st->ctx->codec)
			avcodec_close(st->ctx);
#if LIBAVUTIL_VERSION_INT >= ((51<<16)+(8<<8)+0)
		av_opt_free(st->ctx);
#endif
		av_free(st->ctx);
	}

	if (st->pict)
		av_free(st->pict);

#if LIBAVCODEC_VERSION_INT >= ((52<<16)+(92<<8)+0)
	st->ctx = avcodec_alloc_context3(st->codec);
#else
	st->ctx = avcodec_alloc_context();
#endif

#if LIBAVUTIL_VERSION_INT >= ((52<<16)+(20<<8)+100)
	st->pict = av_frame_alloc();
#else
	st->pict = avcodec_alloc_frame();
#endif

	if (!st->ctx || !st->pict) {
		err = ENOMEM;
		goto out;
	}

	av_opt_set_defaults(st->ctx);

	st->ctx->bit_rate  = prm->bitrate;
	st->ctx->width     = size->w;
	st->ctx->height    = size->h;
	st->ctx->gop_size  = DEFAULT_GOP_SIZE;
	st->ctx->pix_fmt   = pix_fmt;
	st->ctx->time_base.num = 1;
	st->ctx->time_base.den = prm->fps;

	/* params to avoid libavcodec/x264 default preset error */
	if (st->codec_id == AV_CODEC_ID_H264) {
		st->ctx->me_range = 16;
		st->ctx->qmin = 10;
		st->ctx->qmax = 51;
		st->ctx->max_qdiff = 4;

#ifndef USE_X264
		if (st->codec == avcodec_find_encoder_by_name("nvenc_h264") ||
		st->codec == avcodec_find_encoder_by_name("h264_nvenc")) {

#if LIBAVUTIL_VERSION_INT >= ((51<<16)+(21<<8)+0)
			err = av_opt_set(st->ctx->priv_data,
				"preset", "llhp", 0);

			if (err < 0) {
				debug("avcodec: h264 nvenc setting preset "
					"\"llhp\" failed; error: %u\n", err);
			}
			else {
				debug("avcodec: h264 nvenc preset "
					"\"llhp\" selected\n");
			}
			err = av_opt_set_int(st->ctx->priv_data,
				"2pass", 1, 0);

			if (err < 0) {
				debug("avcodec: h264 nvenc option "
					"\"2pass\" failed; error: %u\n", err);
			}
			else {
				debug("avcodec: h264 nvenc option "
					"\"2pass\" selected\n");
			}
#endif
		}
#endif
	}

#if LIBAVCODEC_VERSION_INT >= ((53<<16)+(8<<8)+0)
	if (avcodec_open2(st->ctx, st->codec, NULL) < 0) {
		err = ENOENT;
		goto out;
	}
#else
	if (avcodec_open(st->ctx, st->codec) < 0) {
		err = ENOENT;
		goto out;
	}
#endif

#if LIBAVCODEC_VERSION_INT >= ((53<<16)+(5<<8)+0)
	st->pict->format = pix_fmt;
	st->pict->width = size->w;
	st->pict->height = size->h;
#endif

 out:
	if (err) {
		if (st->ctx) {
			if (st->ctx->codec)
				avcodec_close(st->ctx);
#if LIBAVUTIL_VERSION_INT >= ((51<<16)+(8<<8)+0)
			av_opt_free(st->ctx);
#endif
			av_free(st->ctx);
			st->ctx = NULL;
		}

		if (st->pict) {
			av_free(st->pict);
			st->pict = NULL;
		}
	}
	else
		st->encsize = *size;

	return err;
}


int decode_sdpparam_h264(struct videnc_state *st, const struct pl *name,
			 const struct pl *val)
{
	if (0 == pl_strcasecmp(name, "packetization-mode")) {
		st->u.h264.packetization_mode = pl_u32(val);

		if (st->u.h264.packetization_mode != 0) {
			warning("avcodec: illegal packetization-mode %u\n",
				st->u.h264.packetization_mode);
			return EPROTO;
		}
	}
	else if (0 == pl_strcasecmp(name, "profile-level-id")) {
		struct pl prof = *val;
		if (prof.l != 6) {
			warning("avcodec: invalid profile-level-id (%r)\n",
				val);
			return EPROTO;
		}

		prof.l = 2;
		st->u.h264.profile_idc = pl_x32(&prof); prof.p += 2;
		st->u.h264.profile_iop = pl_x32(&prof); prof.p += 2;
		st->u.h264.level_idc   = pl_x32(&prof);
	}
	else if (0 == pl_strcasecmp(name, "max-fs")) {
		st->u.h264.max_fs = pl_u32(val);
	}
	else if (0 == pl_strcasecmp(name, "max-smbps")) {
		st->u.h264.max_smbps = pl_u32(val);
	}

	return 0;
}


static void param_handler(const struct pl *name, const struct pl *val,
			  void *arg)
{
	struct videnc_state *st = arg;

	if (st->codec_id == AV_CODEC_ID_H263)
		(void)decode_sdpparam_h263(st, name, val);
	else if (st->codec_id == AV_CODEC_ID_H264)
		(void)decode_sdpparam_h264(st, name, val);
}


static int general_packetize(struct mbuf *mb, size_t pktsize,
			     videnc_packet_h *pkth, void *arg)
{
	int err = 0;

	/* Assemble frame into smaller packets */
	while (!err) {
		size_t sz, left = mbuf_get_left(mb);
		bool last = (left < pktsize);
		if (!left)
			break;

		sz = last ? left : pktsize;

		err = pkth(last, NULL, 0, mbuf_buf(mb), sz, arg);

		mbuf_advance(mb, sz);
	}

	return err;
}


static int h263_packetize(struct videnc_state *st, struct mbuf *mb,
			  videnc_packet_h *pkth, void *arg)
{
	struct h263_strm h263_strm;
	struct h263_hdr h263_hdr;
	size_t pos;
	int err;

	/* Decode bit-stream header, used by packetizer */
	err = h263_strm_decode(&h263_strm, mb);
	if (err)
		return err;

	h263_hdr_copy_strm(&h263_hdr, &h263_strm);

	st->mb_frag->pos = st->mb_frag->end = 0;
	err = h263_hdr_encode(&h263_hdr, st->mb_frag);
	pos = st->mb_frag->pos;

	/* Assemble frame into smaller packets */
	while (!err) {
		size_t sz, left = mbuf_get_left(mb);
		bool last = (left < st->encprm.pktsize);
		if (!left)
			break;

		sz = last ? left : st->encprm.pktsize;

		st->mb_frag->pos = st->mb_frag->end = pos;
		err = mbuf_write_mem(st->mb_frag, mbuf_buf(mb), sz);
		if (err)
			break;

		st->mb_frag->pos = 0;

		err = pkth(last, NULL, 0, mbuf_buf(st->mb_frag),
			   mbuf_get_left(st->mb_frag), arg);

		mbuf_advance(mb, sz);
	}

	return err;
}


#ifdef USE_X264
static int open_encoder_x264(struct videnc_state *st, struct videnc_param *prm,
			     const struct vidsz *size, int csp)
{
	x264_param_t xprm;

	x264_param_default(&xprm);

#if X264_BUILD >= 87
	x264_param_apply_profile(&xprm, "baseline");
#endif

	xprm.i_level_idc = h264_level_idc;
	xprm.i_width = size->w;
	xprm.i_height = size->h;
	xprm.i_csp = csp;
	xprm.i_fps_num = prm->fps;
	xprm.i_fps_den = 1;
	xprm.rc.i_bitrate = prm->bitrate / 1000; /* kbit/s */
	xprm.rc.i_rc_method = X264_RC_CQP;
	xprm.i_log_level = X264_LOG_WARNING;

	/* ultrafast preset */
	xprm.i_frame_reference = 1;
	xprm.i_scenecut_threshold = 0;
	xprm.b_deblocking_filter = 0;
	xprm.b_cabac = 0;
	xprm.i_bframe = 0;
	xprm.analyse.intra = 0;
	xprm.analyse.inter = 0;
	xprm.analyse.b_transform_8x8 = 0;
	xprm.analyse.i_me_method = X264_ME_DIA;
	xprm.analyse.i_subpel_refine = 0;
#if X264_BUILD >= 59
	xprm.rc.i_aq_mode = 0;
#endif
	xprm.analyse.b_mixed_references = 0;
	xprm.analyse.i_trellis = 0;
#if X264_BUILD >= 63
	xprm.i_bframe_adaptive = X264_B_ADAPT_NONE;
#endif
#if X264_BUILD >= 70
	xprm.rc.b_mb_tree = 0;
#endif

	/* slice-based threading (--tune=zerolatency) */
#if X264_BUILD >= 80
	xprm.rc.i_lookahead = 0;
	xprm.i_sync_lookahead = 0;
	xprm.i_bframe = 0;
#endif

	/* put SPS/PPS before each keyframe */
	xprm.b_repeat_headers = 1;

#if X264_BUILD >= 82
	/* needed for x264_encoder_intra_refresh() */
	xprm.b_intra_refresh = 1;
#endif

	if (st->x264)
		x264_encoder_close(st->x264);

	st->x264 = x264_encoder_open(&xprm);
	if (!st->x264) {
		warning("avcodec: x264_encoder_open() failed\n");
		return ENOENT;
	}

	st->encsize = *size;

	return 0;
}
#endif


int encode_update(struct videnc_state **vesp, const struct vidcodec *vc,
		  struct videnc_param *prm, const char *fmtp,
		  videnc_packet_h *pkth, void *arg)
{
	struct videnc_state *st;
	int err = 0;

	if (!vesp || !vc || !prm || !pkth)
		return EINVAL;

	if (*vesp)
		return 0;

	st = mem_zalloc(sizeof(*st), destructor);
	if (!st)
		return ENOMEM;

	st->encprm = *prm;
	st->pkth = pkth;
	st->arg = arg;

	st->codec_id = avcodec_resolve_codecid(vc->name);
	if (st->codec_id == AV_CODEC_ID_NONE) {
		err = EINVAL;
		goto out;
	}

	st->mb  = mbuf_alloc(FF_MIN_BUFFER_SIZE * 20);
	st->mb_frag = mbuf_alloc(1024);
	if (!st->mb || !st->mb_frag) {
		err = ENOMEM;
		goto out;
	}

	st->sz_max = st->mb->size;

	if (st->codec_id == AV_CODEC_ID_H264) {
#ifndef USE_X264
		err = init_encoder(st);
#endif
	}
	else
		err = init_encoder(st);
	if (err) {
		warning("avcodec: %s: could not init encoder\n", vc->name);
		goto out;
	}

	if (str_isset(fmtp)) {
		struct pl sdp_fmtp;

		pl_set_str(&sdp_fmtp, fmtp);

		fmt_param_apply(&sdp_fmtp, param_handler, st);
	}

	debug("avcodec: video encoder %s: %d fps, %d bit/s, pktsize=%u\n",
	      vc->name, prm->fps, prm->bitrate, prm->pktsize);

 out:
	if (err)
		mem_deref(st);
	else
		*vesp = st;

	return err;
}


#ifdef USE_X264
int encode_x264(struct videnc_state *st, bool update,
		const struct vidframe *frame)
{
	x264_picture_t pic_in, pic_out;
	x264_nal_t *nal;
	int i_nal;
	int i, err, ret;
	int csp, pln;

	if (!st || !frame)
		return EINVAL;

	switch (frame->fmt) {

	case VID_FMT_YUV420P:
		csp = X264_CSP_I420;
		pln = 3;
		break;

	case VID_FMT_NV12:
		csp = X264_CSP_NV12;
		pln = 2;
		break;

	default:
		warning("avcodec: pixel format not supported (%s)\n",
			vidfmt_name(frame->fmt));
		return ENOTSUP;
	}

	if (!st->x264 || !vidsz_cmp(&st->encsize, &frame->size)) {

		err = open_encoder_x264(st, &st->encprm, &frame->size, csp);
		if (err)
			return err;
	}

	if (update) {
#if X264_BUILD >= 95
		x264_encoder_intra_refresh(st->x264);
#endif
		debug("avcodec: x264 picture update\n");
	}

	x264_picture_init(&pic_in);

	pic_in.i_type = update ? X264_TYPE_IDR : X264_TYPE_AUTO;
	pic_in.i_qpplus1 = 0;
	pic_in.i_pts = ++st->pts;

	pic_in.img.i_csp = csp;
	pic_in.img.i_plane = pln;
	for (i=0; i<pln; i++) {
		pic_in.img.i_stride[i] = frame->linesize[i];
		pic_in.img.plane[i]    = frame->data[i];
	}

	ret = x264_encoder_encode(st->x264, &nal, &i_nal, &pic_in, &pic_out);
	if (ret < 0) {
		fprintf(stderr, "x264 [error]: x264_encoder_encode failed\n");
	}
	if (i_nal == 0)
		return 0;

	err = 0;
	for (i=0; i<i_nal && !err; i++) {
		const uint8_t hdr = nal[i].i_ref_idc<<5 | nal[i].i_type<<0;
		int offset = 0;

#if X264_BUILD >= 76
		const uint8_t *p = nal[i].p_payload;

		/* Find the NAL Escape code [00 00 01] */
		if (nal[i].i_payload > 4 && p[0] == 0x00 && p[1] == 0x00) {
			if (p[2] == 0x00 && p[3] == 0x01)
				offset = 4 + 1;
			else if (p[2] == 0x01)
				offset = 3 + 1;
		}
#endif

		/* skip Supplemental Enhancement Information (SEI) */
		if (nal[i].i_type == H264_NAL_SEI)
			continue;

		err = h264_nal_send(true, true, (i+1)==i_nal, hdr,
				    nal[i].p_payload + offset,
				    nal[i].i_payload - offset,
				    st->encprm.pktsize, st->pkth, st->arg);
	}

	return err;
}
#endif


int encode(struct videnc_state *st, bool update, const struct vidframe *frame)
{
	int i, err, ret;
	int pix_fmt;

	if (!st || !frame)
		return EINVAL;

	switch (frame->fmt) {

	case VID_FMT_YUV420P:
		pix_fmt = AV_PIX_FMT_YUV420P;
		break;

	case VID_FMT_NV12:
		pix_fmt = AV_PIX_FMT_NV12;
		break;

	default:
		warning("avcodec: pixel format not supported (%s)\n",
			vidfmt_name(frame->fmt));
		return ENOTSUP;
	}

	if (!st->ctx || !vidsz_cmp(&st->encsize, &frame->size)) {

		err = open_encoder(st, &st->encprm, &frame->size, pix_fmt);
		if (err) {
			warning("avcodec: open_encoder: %m\n", err);
			return err;
		}
	}

	for (i=0; i<4; i++) {
		st->pict->data[i]     = frame->data[i];
		st->pict->linesize[i] = frame->linesize[i];
	}
	st->pict->pts = st->pts++;
	if (update) {
		debug("avcodec: encoder picture update\n");
		st->pict->key_frame = 1;
#ifdef FF_I_TYPE
		st->pict->pict_type = FF_I_TYPE;  /* Infra Frame */
#else
		st->pict->pict_type = AV_PICTURE_TYPE_I;
#endif
	}
	else {
		st->pict->key_frame = 0;
		st->pict->pict_type = 0;
	}

	mbuf_rewind(st->mb);

#if LIBAVCODEC_VERSION_INT >= ((57<<16)+(37<<8)+100)
	do {
		AVPacket *pkt;

		ret = avcodec_send_frame(st->ctx, st->pict);
		if (ret < 0)
			return EBADMSG;

		pkt = av_packet_alloc();
		if (!pkt)
			return ENOMEM;

		ret = avcodec_receive_packet(st->ctx, pkt);
		if (ret < 0) {
			av_packet_free(&pkt);
			return 0;
		}

		err = mbuf_write_mem(st->mb, pkt->data, pkt->size);
		st->mb->pos = 0;

		av_packet_free(&pkt);

		if (err)
			return err;
	} while (0);
#elif LIBAVCODEC_VERSION_INT >= ((54<<16)+(1<<8)+0)
	do {
		AVPacket avpkt;
		int got_packet;

		av_init_packet(&avpkt);

		avpkt.data = st->mb->buf;
		avpkt.size = (int)st->mb->size;

		ret = avcodec_encode_video2(st->ctx, &avpkt,
					    st->pict, &got_packet);
		if (ret < 0)
			return EBADMSG;
		if (!got_packet)
			return 0;

		mbuf_set_end(st->mb, avpkt.size);

	} while (0);
#else
	ret = avcodec_encode_video(st->ctx, st->mb->buf,
				   (int)st->mb->size, st->pict);
	if (ret < 0 )
		return EBADMSG;

	/* todo: figure out proper buffer size */
	if (ret > (int)st->sz_max) {
		debug("avcodec: grow encode buffer %u --> %d\n",
		      st->sz_max, ret);
		st->sz_max = ret;
	}

	mbuf_set_end(st->mb, ret);
#endif

	switch (st->codec_id) {

	case AV_CODEC_ID_H263:
		err = h263_packetize(st, st->mb, st->pkth, st->arg);
		break;

	case AV_CODEC_ID_H264:
		err = h264_packetize(st->mb->buf, st->mb->end,
				     st->encprm.pktsize,
				     st->pkth, st->arg);
		break;

	case AV_CODEC_ID_MPEG4:
		err = general_packetize(st->mb, st->encprm.pktsize,
					st->pkth, st->arg);
		break;

	default:
		err = EPROTO;
		break;
	}

	return err;
}
