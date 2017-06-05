/**
 * @file gst1/gst.c  Gstreamer 1.0 playbin pipeline
 *
 * Copyright (C) 2010 - 2015 Creytiv.com
 */
#include <stdlib.h>
#include <string.h>
#define __USE_POSIX199309
#include <time.h>
#include <pthread.h>
#include <gst/gst.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>


/**
 * @defgroup gst1 gst1
 *
 * Audio source module using gstreamer 1.0 as input
 *
 * The module 'gst1' is using the Gstreamer framework to play external
 * media and provide this as an internal audio source.
 *
 * Example config:
 \verbatim
  audio_source        gst,http://relay.slayradio.org:8000/
 \endverbatim
 */


/**
 * Defines the Gstreamer state
 *
 * <pre>
 *                ptime=variable             ptime=20ms
 *  .-----------. N kHz          .---------. N kHz
 *  |           | 1-2 channels   |         | 1-2 channels
 *  | Gstreamer |--------------->|Packetize|-------------> [read handler]
 *  |           |                |         |
 *  '-----------'                '---------'
 *
 * </pre>
 */
struct ausrc_st {
	const struct ausrc *as;     /**< Inheritance             */

	pthread_t tid;              /**< Thread ID               */
	bool run;                   /**< Running flag            */
	ausrc_read_h *rh;           /**< Read handler            */
	ausrc_error_h *errh;        /**< Error handler           */
	void *arg;                  /**< Handler argument        */
	struct ausrc_prm prm;       /**< Read parameters         */
	struct aubuf *aubuf;        /**< Packet buffer           */
	size_t psize;               /**< Packet size in bytes    */
	size_t sampc;

	/* Gstreamer */
	char *uri;
	GstElement *pipeline, *bin, *source, *capsfilt, *sink;
	GMainLoop *loop;
};


typedef struct _GstFakeSink GstFakeSink;
static char gst_uri[256] = "http://relay1.slayradio.org:8000/";
static struct ausrc *ausrc;


static void *thread(void *arg)
{
	struct ausrc_st *st = arg;

	/* Now set to playing and iterate. */
	gst_element_set_state(st->pipeline, GST_STATE_PLAYING);

	while (st->run) {
		g_main_loop_run(st->loop);
	}

	return NULL;
}


static gboolean bus_watch_handler(GstBus *bus, GstMessage *msg, gpointer data)
{
	struct ausrc_st *st = data;
	GMainLoop *loop = st->loop;
	GstTagList *tag_list;
	gchar *title;
	GError *err;
	gchar *d;

	(void)bus;

	switch (GST_MESSAGE_TYPE(msg)) {

	case GST_MESSAGE_EOS:
		/* XXX decrementing repeat count? */

		/* Re-start stream */
		if (st->run) {
			gst_element_set_state(st->pipeline, GST_STATE_NULL);
			gst_element_set_state(st->pipeline, GST_STATE_PLAYING);
		}
		else {
			g_main_loop_quit(loop);
		}
		break;

	case GST_MESSAGE_ERROR:
		gst_message_parse_error(msg, &err, &d);

		warning("gst: Error: %d(%m) message=\"%s\"\n", err->code,
			err->code, err->message);
		warning("gst: Debug: %s\n", d);

		g_free(d);

		/* Call error handler */
		if (st->errh)
			st->errh(err->code, err->message, st->arg);

		g_error_free(err);

		st->run = false;
		g_main_loop_quit(loop);
		break;

	case GST_MESSAGE_TAG:
		gst_message_parse_tag(msg, &tag_list);

		if (gst_tag_list_get_string(tag_list, GST_TAG_TITLE, &title)) {
			info("gst: title: %s\n", title);
			g_free(title);
		}
		break;

	default:
		break;
	}

	return TRUE;
}


static void format_check(struct ausrc_st *st, GstStructure *s)
{
	int rate, channels, width;
	gboolean sign;

	if (!st || !s)
		return;

	gst_structure_get_int(s, "rate", &rate);
	gst_structure_get_int(s, "channels", &channels);
	gst_structure_get_int(s, "width", &width);
	gst_structure_get_boolean(s, "signed", &sign);

	if ((int)st->prm.srate != rate) {
		warning("gst: expected %u Hz (got %u Hz)\n", st->prm.srate,
			rate);
	}
	if (st->prm.ch != channels) {
		warning("gst: expected %d channels (got %d)\n",
			st->prm.ch, channels);
	}
	if (16 != width) {
		warning("gst: expected 16-bit width (got %d)\n", width);
	}
	if (!sign) {
		warning("gst: expected signed 16-bit format\n");
	}
}


static void play_packet(struct ausrc_st *st)
{
	int16_t buf[st->sampc];

	/* timed read from audio-buffer */
	if (aubuf_get_samp(st->aubuf, st->prm.ptime, buf, st->sampc))
		return;

	/* call read handler */
	if (st->rh)
		st->rh(buf, st->sampc, st->arg);
}


/* Expected format: 16-bit signed PCM */
static void packet_handler(struct ausrc_st *st, GstBuffer *buffer)
{
	GstMapInfo info;
	int err;

	if (!st->run)
		return;

	/* NOTE: When streaming from files, the buffer will be filled up
	 *       pretty quickly..
	 */

	if (!gst_buffer_map(buffer, &info, GST_MAP_READ)) {
		warning("gst: gst_buffer_map failed\n");
		return;
	}

	err = aubuf_write(st->aubuf, info.data, info.size);
	if (err) {
		warning("gst: aubuf_write: %m\n", err);
	}

	gst_buffer_unmap(buffer, &info);

	/* Empty buffer now */
	while (st->run) {
		const struct timespec delay = {0, st->prm.ptime*1000000/2};

		play_packet(st);

		if (aubuf_cur_size(st->aubuf) < st->psize)
			break;

		(void)nanosleep(&delay, NULL);
	}
}


static void handoff_handler(GstFakeSink *fakesink, GstBuffer *buffer,
			    GstPad *pad, gpointer user_data)
{
	struct ausrc_st *st = user_data;
	GstCaps *caps;
	(void)fakesink;

	caps = gst_pad_get_current_caps(pad);

	format_check(st, gst_caps_get_structure(caps, 0));

	packet_handler(st, buffer);
}


static void set_caps(struct ausrc_st *st)
{
	GstCaps *caps;

	/* Set the capabilities we want */
	caps = gst_caps_new_simple("audio/x-raw",
				   "rate",     G_TYPE_INT,    st->prm.srate,
				   "channels", G_TYPE_INT,    st->prm.ch,
				   "width",    G_TYPE_INT,    16,
				   "signed",   G_TYPE_BOOLEAN,true,
				   NULL);

	g_object_set(G_OBJECT(st->capsfilt), "caps", caps, NULL);
}


/**
 * Set up the Gstreamer pipeline. The playbin element is used to decode
 * all kinds of different formats. The capsfilter is used to deliver the
 * audio in a fixed format (X Hz, 1-2 channels, 16 bit signed)
 *
 * The pipeline looks like this:
 *
 * <pre>
 *  .--------------.    .------------------------------------------.
 *  |    playbin   |    |mybin    .------------.   .------------.  |
 *  |----.    .----|    |-----.   | capsfilter |   |  fakesink  |  |
 *  |sink|    |src |--->|ghost|   |----.   .---|   |----.   .---|  |    handoff
 *  |----'    '----|    |pad  |-->|sink|   |src|-->|sink|   |src|--+--> handler
 *  |              |    |-----'   '------------'   '------------'  |
 *  '--------------'    '------------------------------------------'
 * </pre>
 *
 * @param st Audio source state
 *
 * @return 0 if success, otherwise errorcode
 */
static int gst_setup(struct ausrc_st *st)
{
	GstBus *bus;
	GstPad *pad;

	st->loop = g_main_loop_new(NULL, FALSE);

	st->pipeline = gst_pipeline_new("pipeline");
	if (!st->pipeline) {
		warning("gst: failed to create pipeline element\n");
		return ENOMEM;
	}

	/********************* Player BIN **************************/

	st->source = gst_element_factory_make("playbin", "source");
	if (!st->source) {
		warning("gst: failed to create playbin source element\n");
		return ENOMEM;
	}

	/********************* My BIN **************************/

	st->bin = gst_bin_new("mybin");

	st->capsfilt = gst_element_factory_make("capsfilter", NULL);
	if (!st->capsfilt) {
		warning("gst: failed to create capsfilter element\n");
		return ENOMEM;
	}

	set_caps(st);

	st->sink = gst_element_factory_make("fakesink", "sink");
	if (!st->sink) {
		warning("gst: failed to create sink element\n");
		return ENOMEM;
	}

	gst_bin_add_many(GST_BIN(st->bin), st->capsfilt, st->sink, NULL);
	gst_element_link_many(st->capsfilt, st->sink, NULL);

	/* add ghostpad */
	pad = gst_element_get_static_pad(st->capsfilt, "sink");
	gst_element_add_pad(st->bin, gst_ghost_pad_new("sink", pad));
	gst_object_unref(GST_OBJECT(pad));

	/* put all elements in a bin */
	gst_bin_add_many(GST_BIN(st->pipeline), st->source, NULL);

	/* Override audio-sink handoff handler */
	g_object_set(G_OBJECT(st->sink), "signal-handoffs", TRUE, NULL);
	g_signal_connect(st->sink, "handoff", G_CALLBACK(handoff_handler), st);

	g_object_set(G_OBJECT(st->source), "audio-sink", st->bin, NULL);

	/********************* Misc **************************/

	/* Bus watch */
	bus = gst_pipeline_get_bus(GST_PIPELINE(st->pipeline));
	gst_bus_add_watch(bus, bus_watch_handler, st);
	gst_object_unref(bus);

	/* Set URI */
	g_object_set(G_OBJECT(st->source), "uri", st->uri, NULL);

	return 0;
}


static void gst_destructor(void *arg)
{
	struct ausrc_st *st = arg;

	if (st->run) {
		st->run = false;
		g_main_loop_quit(st->loop);
		pthread_join(st->tid, NULL);
	}

	gst_element_set_state(st->pipeline, GST_STATE_NULL);
	gst_object_unref(GST_OBJECT(st->pipeline));

	mem_deref(st->uri);
	mem_deref(st->aubuf);
}


static int gst_alloc(struct ausrc_st **stp, const struct ausrc *as,
		     struct media_ctx **ctx,
		     struct ausrc_prm *prm, const char *device,
		     ausrc_read_h *rh, ausrc_error_h *errh, void *arg)
{
	struct ausrc_st *st;
	int err;

	(void)ctx;

	if (!device)
		device = gst_uri;

	if (!prm)
		return EINVAL;

	st = mem_zalloc(sizeof(*st), gst_destructor);
	if (!st)
		return ENOMEM;

	st->as   = as;
	st->rh   = rh;
	st->errh = errh;
	st->arg  = arg;

	err = str_dup(&st->uri, device);
	if (err)
		goto out;

	st->prm   = *prm;

	st->sampc = prm->srate * prm->ch * prm->ptime / 1000;
	st->psize = 2 * st->sampc;

	err = aubuf_alloc(&st->aubuf, st->psize, 0);
	if (err)
		goto out;

	err = gst_setup(st);
	if (err)
		goto out;

	st->run  = true;
	err = pthread_create(&st->tid, NULL, thread, st);
	if (err) {
		st->run = false;
		goto out;
	}

 out:
	if (err)
		mem_deref(st);
	else
		*stp = st;

	return err;
}


static int mod_gst_init(void)
{
	gchar *s;

	gst_init(0, NULL);

	s = gst_version_string();

	info("gst: init: %s\n", s);

	g_free(s);

	return ausrc_register(&ausrc, baresip_ausrcl(), "gst", gst_alloc);
}


static int mod_gst_close(void)
{
	gst_deinit();
	ausrc = mem_deref(ausrc);
	return 0;
}


EXPORT_SYM const struct mod_export DECL_EXPORTS(gst1) = {
	"gst1",
	"sound",
	mod_gst_init,
	mod_gst_close
};
