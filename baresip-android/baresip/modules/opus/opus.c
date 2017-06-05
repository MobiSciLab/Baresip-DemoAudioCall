/**
 * @file opus.c Opus Audio Codec
 *
 * Copyright (C) 2010 Creytiv.com
 */

#include <re.h>
#include <baresip.h>
#include <opus/opus.h>
#include "opus.h"


/**
 * @defgroup opus opus
 *
 * The OPUS audio codec
 *
 * Supported version: libopus 1.0.0 or later
 *
 * Configuration options:
 *
 \verbatim
  opus_bitrate    128000     # Average bitrate in [bps]
  opus_cbr        {yes,no}   # Constant Bitrate (inverse of VBR)
  opus_inbandfec  {yes,no}   # Enable inband Forward Error Correction (FEC)
  opus_dtx        {yes,no}   # Enable Discontinuous Transmission (DTX)
 \endverbatim
 *
 * References:
 *
 *    RFC 6716  Definition of the Opus Audio Codec
 *    RFC 7587  RTP Payload Format for the Opus Speech and Audio Codec
 *
 *    http://opus-codec.org/downloads/
 */


static bool opus_mirror;
static char fmtp[256] = "stereo=1;sprop-stereo=1";
static char fmtp_mirror[256];


static int opus_fmtp_enc(struct mbuf *mb, const struct sdp_format *fmt,
			 bool offer, void *arg)
{
	bool mirror;

	(void)arg;
	(void)offer;

	if (!mb || !fmt)
		return 0;

	mirror = !offer && str_isset(fmtp_mirror);

	return mbuf_printf(mb, "a=fmtp:%s %s\r\n",
			   fmt->id, mirror ? fmtp_mirror : fmtp);
}


static struct aucodec opus = {
	.name      = "opus",
	.srate     = 48000,
	.crate     = 48000,
	.ch        = 2,
	.fmtp      = NULL,
	.encupdh   = opus_encode_update,
	.ench      = opus_encode_frm,
	.decupdh   = opus_decode_update,
	.dech      = opus_decode_frm,
	.plch      = opus_decode_pkloss,
	.fmtp_ench = opus_fmtp_enc,
};


void opus_mirror_params(const char *x)
{
	if (!opus_mirror)
		return;

	info("opus: mirror parameters: \"%s\"\n", x);

	str_ncpy(fmtp_mirror, x, sizeof(fmtp_mirror));
}


static int module_init(void)
{
	struct conf *conf = conf_cur();
	uint32_t value;
	char *p = fmtp + str_len(fmtp);
	bool b;
	int n = 0;

	if (0 == conf_get_u32(conf, "opus_bitrate", &value)) {

		n = re_snprintf(p, sizeof(fmtp) - str_len(p),
				";maxaveragebitrate=%d", value);
		if (n <= 0)
			return ENOMEM;

		p += n;
	}

	if (0 == conf_get_bool(conf, "opus_cbr", &b)) {

		n = re_snprintf(p, sizeof(fmtp) - str_len(p),
				";cbr=%d", b);
		if (n <= 0)
			return ENOMEM;

		p += n;
	}

	if (0 == conf_get_bool(conf, "opus_inbandfec", &b)) {

		n = re_snprintf(p, sizeof(fmtp) - str_len(p),
				";useinbandfec=%d", b);
		if (n <= 0)
			return ENOMEM;

		p += n;
	}

	if (0 == conf_get_bool(conf, "opus_dtx", &b)) {

		n = re_snprintf(p, sizeof(fmtp) - str_len(p),
				";usedtx=%d", b);
		if (n <= 0)
			return ENOMEM;

		p += n;
	}

	(void)conf_get_bool(conf, "opus_mirror", &opus_mirror);

	debug("opus: fmtp=\"%s\"\n", fmtp);

	aucodec_register(baresip_aucodecl(), &opus);

	return 0;
}


static int module_close(void)
{
	aucodec_unregister(&opus);

	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(opus) = {
	"opus",
	"audio codec",
	module_init,
	module_close,
};
