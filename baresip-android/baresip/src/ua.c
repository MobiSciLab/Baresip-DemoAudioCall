/**
 * @file src/ua.c  User-Agent
 *
 * Copyright (C) 2010 Creytiv.com
 */
#include <string.h>
#include <re.h>
#include <baresip.h>
#include "core.h"
#include <ctype.h>


/** Magic number */
#define MAGIC 0x0a0a0a0a
#include "magic.h"


/** Defines a SIP User Agent object */
struct ua {
	MAGIC_DECL                   /**< Magic number for struct ua         */
	struct ua **uap;             /**< Pointer to application's ua        */
	struct le le;                /**< Linked list element                */
	struct account *acc;         /**< Account Parameters                 */
	struct list regl;            /**< List of Register clients           */
	struct list calls;           /**< List of active calls (struct call) */
	struct pl extensionv[8];     /**< Vector of SIP extensions           */
	size_t    extensionc;        /**< Number of SIP extensions           */
	char *cuser;                 /**< SIP Contact username               */
	char *pub_gruu;              /**< SIP Public GRUU                    */
	int af;                      /**< Preferred Address Family           */
	int af_media;                /**< Preferred Address Family for media */
	enum presence_status my_status; /**< Presence Status                 */
};

struct ua_eh {
	struct le le;
	ua_event_h *h;
	void *arg;
};

static struct {
	struct config_sip *cfg;        /**< SIP configuration               */
	struct list ual;               /**< List of User-Agents (struct ua) */
	struct list ehl;               /**< Event handlers (struct ua_eh)   */
	struct sip *sip;               /**< SIP Stack                       */
	struct sip_lsnr *lsnr;         /**< SIP Listener                    */
	struct sipsess_sock *sock;     /**< SIP Session socket              */
	struct sipevent_sock *evsock;  /**< SIP Event socket                */
	struct ua *ua_cur;             /**< Current User-Agent              */
	bool use_udp;                  /**< Use UDP transport               */
	bool use_tcp;                  /**< Use TCP transport               */
	bool use_tls;                  /**< Use TLS transport               */
	bool prefer_ipv6;              /**< Force IPv6 transport            */
	sip_msg_h *subh;               /**< Subscribe handler               */
	ua_exit_h *exith;              /**< UA Exit handler                 */
	void *arg;                     /**< UA Exit handler argument        */
	char *eprm;                    /**< Extra UA parameters             */
#ifdef USE_TLS
	struct tls *tls;               /**< TLS Context                     */
#endif
} uag = {
	NULL,
	LIST_INIT,
	LIST_INIT,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	true,
	true,
	true,
	false,
	NULL,
	NULL,
	NULL,
	NULL,
#ifdef USE_TLS
	NULL,
#endif
};


/* prototypes */
static int  ua_call_alloc(struct call **callp, struct ua *ua,
			  enum vidmode vidmode, const struct sip_msg *msg,
			  struct call *xcall, const char *local_uri);


/* This function is called when all SIP transactions are done */
static void exit_handler(void *arg)
{
	(void)arg;

	ua_event(NULL, UA_EVENT_EXIT, NULL, NULL);

	debug("ua: sip-stack exit\n");

	if (uag.exith)
		uag.exith(uag.arg);
}


void ua_printf(const struct ua *ua, const char *fmt, ...)
{
	va_list ap;

	if (!ua)
		return;

	va_start(ap, fmt);
	info("%r@%r: %v", &ua->acc->luri.user, &ua->acc->luri.host, fmt, &ap);
	va_end(ap);
}


void ua_event(struct ua *ua, enum ua_event ev, struct call *call,
	      const char *fmt, ...)
{
	struct le *le;
	char buf[256];
	va_list ap;

	va_start(ap, fmt);
	(void)re_vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	/* send event to all clients */
	le = uag.ehl.head;
	while (le) {
		struct ua_eh *eh = le->data;
		le = le->next;

		eh->h(ua, ev, call, buf, eh->arg);
	}
}


/**
 * Start registration of a User-Agent
 *
 * @param ua User-Agent
 *
 * @return 0 if success, otherwise errorcode
 */
int ua_register(struct ua *ua)
{
	struct account *acc;
	struct le *le;
	struct uri uri;
	char *reg_uri = NULL;
	char params[256] = "";
	unsigned i;
	int err;

	if (!ua)
		return EINVAL;

	acc = ua->acc;
	uri = ua->acc->luri;
	uri.user = uri.password = pl_null;

	err = re_sdprintf(&reg_uri, "%H", uri_encode, &uri);
	if (err)
		goto out;

	if (uag.cfg && str_isset(uag.cfg->uuid)) {
		if (re_snprintf(params, sizeof(params),
				";+sip.instance=\"<urn:uuid:%s>\"",
				uag.cfg->uuid) < 0) {
			err = ENOMEM;
			goto out;
		}
	}

	if (acc->regq) {
		if (re_snprintf(&params[strlen(params)],
				sizeof(params) - strlen(params),
				";q=%s", acc->regq) < 0) {
			err = ENOMEM;
			goto out;
		}
	}

	if (acc->mnat && acc->mnat->ftag) {
		if (re_snprintf(&params[strlen(params)],
				sizeof(params) - strlen(params),
				";%s", acc->mnat->ftag) < 0) {
			err = ENOMEM;
			goto out;
		}
	}

	ua_event(ua, UA_EVENT_REGISTERING, NULL, NULL);

	for (le = ua->regl.head, i=0; le; le = le->next, i++) {
		struct reg *reg = le->data;

		err = reg_register(reg, reg_uri, params,
				   acc->regint, acc->outboundv[i]);
		if (err) {
			warning("ua: SIP register failed: %m\n", err);
			goto out;
		}
	}

 out:
	mem_deref(reg_uri);

	return err;
}


/**
 * Unregister all Register clients of a User-Agent
 *
 * @param ua User-Agent
 */
void ua_unregister(struct ua *ua)
{
	struct le *le;

	if (!ua)
		return;

	if (!list_isempty(&ua->regl))
		ua_event(ua, UA_EVENT_UNREGISTERING, NULL, NULL);

	for (le = ua->regl.head; le; le = le->next) {
		struct reg *reg = le->data;

		reg_unregister(reg);
	}
}


bool ua_isregistered(const struct ua *ua)
{
	struct le *le;

	if (!ua)
		return false;

	for (le = ua->regl.head; le; le = le->next) {

		const struct reg *reg = le->data;

		/* it is enough if one of the registrations work */
		if (reg_isok(reg))
			return true;
	}

	return false;
}


static struct call *ua_find_call_onhold(const struct ua *ua)
{
	struct le *le;

	if (!ua)
		return NULL;

	for (le = ua->calls.tail; le; le = le->prev) {

		struct call *call = le->data;

		if (call_is_onhold(call))
			return call;
	}

	return NULL;
}


static void resume_call(struct ua *ua)
{
	struct call *call;

	call = ua_find_call_onhold(ua);
	if (call) {
		ua_printf(ua, "resuming previous call with '%s'\n",
			  call_peeruri(call));
		call_hold(call, false);
	}
}


static void call_event_handler(struct call *call, enum call_event ev,
			       const char *str, void *arg)
{
	struct ua *ua = arg;
	const char *peeruri;
	struct call *call2 = NULL;
	int err;

	MAGIC_CHECK(ua);

	peeruri = call_peeruri(call);

	switch (ev) {

	case CALL_EVENT_INCOMING:

		if (contact_block_access(baresip_contacts(),
					 peeruri)) {

			info("ua: blocked access: \"%s\"\n", peeruri);

			ua_event(ua, UA_EVENT_CALL_CLOSED, call, str);
			mem_deref(call);
			break;
		}

		switch (ua->acc->answermode) {

		case ANSWERMODE_EARLY:
			(void)call_progress(call);
			break;

		case ANSWERMODE_AUTO:
			(void)call_answer(call, 200);
			break;

		case ANSWERMODE_MANUAL:
		default:
			ua_event(ua, UA_EVENT_CALL_INCOMING, call, peeruri);
			break;
		}
		break;

	case CALL_EVENT_RINGING:
		ua_event(ua, UA_EVENT_CALL_RINGING, call, peeruri);
		break;

	case CALL_EVENT_PROGRESS:
		ua_printf(ua, "Call in-progress: %s\n", peeruri);
		ua_event(ua, UA_EVENT_CALL_PROGRESS, call, peeruri);
		break;

	case CALL_EVENT_ESTABLISHED:
		ua_printf(ua, "Call established: %s\n", peeruri);
		ua_event(ua, UA_EVENT_CALL_ESTABLISHED, call, peeruri);
		break;

	case CALL_EVENT_CLOSED:
		ua_event(ua, UA_EVENT_CALL_CLOSED, call, str);
		mem_deref(call);

		resume_call(ua);
		break;

	case CALL_EVENT_TRANSFER:

		/*
		 * Create a new call to transfer target.
		 *
		 * NOTE: we will automatically connect a new call to the
		 *       transfer target
		 */

		ua_printf(ua, "transferring call to %s\n", str);

		err = ua_call_alloc(&call2, ua, VIDMODE_ON, NULL, call,
				    call_localuri(call));
		if (!err) {
			struct pl pl;

			pl_set_str(&pl, str);

			err = call_connect(call2, &pl);
			if (err) {
				warning("ua: transfer: connect error: %m\n",
					err);
			}
		}

		if (err) {
			(void)call_notify_sipfrag(call, 500, "Call Error");
			mem_deref(call2);
		}
		break;

	case CALL_EVENT_TRANSFER_FAILED:
		ua_event(ua, UA_EVENT_CALL_TRANSFER_FAILED, call, str);
		break;
	}
}


static void call_dtmf_handler(struct call *call, char key, void *arg)
{
	struct ua *ua = arg;
	char key_str[2];

	MAGIC_CHECK(ua);

	if (key != '\0') {

		key_str[0] = key;
		key_str[1] = '\0';

		ua_event(ua, UA_EVENT_CALL_DTMF_START, call, key_str);
	}
	else {
		ua_event(ua, UA_EVENT_CALL_DTMF_END, call, NULL);
	}
}


static int ua_call_alloc(struct call **callp, struct ua *ua,
			 enum vidmode vidmode, const struct sip_msg *msg,
			 struct call *xcall, const char *local_uri)
{
	const struct network *net = baresip_network();
	struct call_prm cprm;
	int af = AF_UNSPEC;
	int err;

	if (*callp) {
		warning("ua: call_alloc: call is already allocated\n");
		return EALREADY;
	}

	/* 1. if AF_MEDIA is set, we prefer it
	 * 2. otherwise fall back to SIP AF
	 */
	if (ua->af_media) {
		af = ua->af_media;
	}
	else if (ua->af) {
		af = ua->af;
	}

	memset(&cprm, 0, sizeof(cprm));

	sa_cpy(&cprm.laddr, net_laddr_af(net, af));
	cprm.vidmode = vidmode;
	cprm.af      = af;

	err = call_alloc(callp, conf_config(), &ua->calls,
			 ua->acc->dispname,
			 local_uri ? local_uri : ua->acc->aor,
			 ua->acc, ua, &cprm,
			 msg, xcall,
			 net_dnsc(net),
			 call_event_handler, ua);
	if (err)
		return err;

	call_set_handlers(*callp, NULL, call_dtmf_handler, ua);

	return 0;
}


static void handle_options(struct ua *ua, const struct sip_msg *msg)
{
	struct sip_contact contact;
	struct call *call = NULL;
	struct mbuf *desc = NULL;
	int err;

	debug("ua: incoming OPTIONS message from %r (%J)\n",
	      &msg->from.auri, &msg->src);

	err = ua_call_alloc(&call, ua, VIDMODE_ON, NULL, NULL, NULL);
	if (err) {
		(void)sip_treply(NULL, uag.sip, msg, 500, "Call Error");
		return;
	}

	err = call_sdp_get(call, &desc, true);
	if (err)
		goto out;

	sip_contact_set(&contact, ua_cuser(ua), &msg->dst, msg->tp);

	err = sip_treplyf(NULL, NULL, uag.sip,
			  msg, true, 200, "OK",
			  "Allow: %s\r\n"
			  "%H"
			  "%H"
			  "Content-Type: application/sdp\r\n"
			  "Content-Length: %zu\r\n"
			  "\r\n"
			  "%b",
			  uag_allowed_methods(),
			  ua_print_supported, ua,
			  sip_contact_print, &contact,
			  mbuf_get_left(desc),
			  mbuf_buf(desc),
			  mbuf_get_left(desc));
	if (err) {
		warning("ua: options: sip_treplyf: %m\n", err);
	}

 out:
	mem_deref(desc);
	mem_deref(call);
}


static void ua_destructor(void *arg)
{
	struct ua *ua = arg;

	if (ua->uap) {
		*ua->uap = NULL;
		ua->uap = NULL;
	}

	list_unlink(&ua->le);

	if (!list_isempty(&ua->regl))
		ua_event(ua, UA_EVENT_UNREGISTERING, NULL, NULL);

	list_flush(&ua->calls);
	list_flush(&ua->regl);
	mem_deref(ua->cuser);
	mem_deref(ua->pub_gruu);
	mem_deref(ua->acc);

	if (list_isempty(&uag.ual)) {
		sip_close(uag.sip, false);
	}
}


static bool request_handler(const struct sip_msg *msg, void *arg)
{
	struct ua *ua;

	(void)arg;

	if (pl_strcmp(&msg->met, "OPTIONS"))
		return false;

	ua = uag_find(&msg->uri.user);
	if (!ua) {
		(void)sip_treply(NULL, uag_sip(), msg, 404, "Not Found");
		return true;
	}

	handle_options(ua, msg);

	return true;
}


static void add_extension(struct ua *ua, const char *extension)
{
	struct pl e;

	if (ua->extensionc >= ARRAY_SIZE(ua->extensionv)) {
		warning("ua: maximum %u number of SIP extensions\n");
		return;
	}

	pl_set_str(&e, extension);

	ua->extensionv[ua->extensionc++] = e;
}


/**
 * Allocate a SIP User-Agent
 *
 * @param uap   Pointer to allocated User-Agent object
 * @param aor   SIP Address-of-Record (AOR)
 *
 * @return 0 if success, otherwise errorcode
 */
int ua_alloc(struct ua **uap, const char *aor)
{
	struct ua *ua;
	char *buf = NULL;
	int err;

	if (!aor)
		return EINVAL;

	ua = mem_zalloc(sizeof(*ua), ua_destructor);
	if (!ua)
		return ENOMEM;

	MAGIC_INIT(ua);

	list_init(&ua->calls);

#if HAVE_INET6
	ua->af   = uag.prefer_ipv6 ? AF_INET6 : AF_INET;
#else
	ua->af   = AF_INET;
#endif

	/* Decode SIP address */
	if (uag.eprm) {
		err = re_sdprintf(&buf, "%s;%s", aor, uag.eprm);
		if (err)
			goto out;
		aor = buf;
	}

	err = account_alloc(&ua->acc, aor);
	if (err)
		goto out;

	/* generate a unique contact-user, this is needed to route
	   incoming requests when using multiple useragents */
	err = re_sdprintf(&ua->cuser, "%r-%p", &ua->acc->luri.user, ua);
	if (err)
		goto out;

	if (ua->acc->sipnat) {
		ua_printf(ua, "Using sipnat: `%s'\n", ua->acc->sipnat);
	}

	if (ua->acc->mnat) {
		ua_printf(ua, "Using medianat `%s'\n",
			  ua->acc->mnat->id);

		if (0 == str_casecmp(ua->acc->mnat->id, "ice"))
			add_extension(ua, "ice");
	}

	if (ua->acc->menc) {
		ua_printf(ua, "Using media encryption `%s'\n",
			  ua->acc->menc->id);
	}

	/* Register clients */
	if (uag.cfg && str_isset(uag.cfg->uuid))
	        add_extension(ua, "gruu");

	if (0 == str_casecmp(ua->acc->sipnat, "outbound")) {

		size_t i;

		add_extension(ua, "path");
		add_extension(ua, "outbound");

		if (!str_isset(uag.cfg->uuid)) {

			warning("ua: outbound requires valid UUID!\n");
			err = ENOSYS;
			goto out;
		}

		for (i=0; i<ARRAY_SIZE(ua->acc->outboundv); i++) {

			if (ua->acc->outboundv[i] && ua->acc->regint) {
				err = reg_add(&ua->regl, ua, (int)i+1);
				if (err)
					break;
			}
		}
	}
	else if (ua->acc->regint) {
		err = reg_add(&ua->regl, ua, 0);
	}
	if (err)
		goto out;

	list_append(&uag.ual, &ua->le, ua);

	if (ua->acc->regint) {
		err = ua_register(ua);
	}

	if (!uag_current())
		uag_current_set(ua);

 out:
	mem_deref(buf);
	if (err)
		mem_deref(ua);
	else if (uap) {
		*uap = ua;

		ua->uap = uap;
	}

	return err;
}


static int uri_complete(struct ua *ua, struct mbuf *buf, const char *uri)
{
	size_t len;
	int err = 0;

	/* Skip initial whitespace */
	while (isspace(*uri))
		++uri;

	len = str_len(uri);

	/* Append sip: scheme if missing */
	if (0 != re_regex(uri, len, "sip:"))
		err |= mbuf_printf(buf, "sip:");

	err |= mbuf_write_str(buf, uri);

	/* Append domain if missing */
	if (0 != re_regex(uri, len, "[^@]+@[^]+", NULL, NULL)) {
#if HAVE_INET6
		if (AF_INET6 == ua->acc->luri.af)
			err |= mbuf_printf(buf, "@[%r]",
					   &ua->acc->luri.host);
		else
#endif
			err |= mbuf_printf(buf, "@%r",
					   &ua->acc->luri.host);

		/* Also append port if specified and not 5060 */
		switch (ua->acc->luri.port) {

		case 0:
		case SIP_PORT:
			break;

		default:
			err |= mbuf_printf(buf, ":%u", ua->acc->luri.port);
			break;
		}
	}

	return err;
}


/**
 * Connect an outgoing call to a given SIP uri
 *
 * @param ua        User-Agent
 * @param callp     Optional pointer to allocated call object
 * @param from_uri  Optional From uri, or NULL for default AOR
 * @param uri       SIP uri to connect to
 * @param params    Optional URI parameters
 * @param vmode     Video mode
 *
 * @return 0 if success, otherwise errorcode
 */
int ua_connect(struct ua *ua, struct call **callp,
	       const char *from_uri, const char *uri,
	       const char *params, enum vidmode vmode)
{
	struct call *call = NULL;
	struct mbuf *dialbuf;
	struct pl pl;
	int err = 0;

	if (!ua || !str_isset(uri))
		return EINVAL;

	dialbuf = mbuf_alloc(64);
	if (!dialbuf)
		return ENOMEM;

	if (params)
		err |= mbuf_printf(dialbuf, "<");

	err |= uri_complete(ua, dialbuf, uri);

	if (params) {
		err |= mbuf_printf(dialbuf, ";%s", params);
	}

	/* Append any optional URI parameters */
	err |= mbuf_write_pl(dialbuf, &ua->acc->luri.params);

	if (params)
		err |= mbuf_printf(dialbuf, ">");

	if (err)
		goto out;

	err = ua_call_alloc(&call, ua, vmode, NULL, NULL, from_uri);
	if (err)
		goto out;

	pl.p = (char *)dialbuf->buf;
	pl.l = dialbuf->end;

	err = call_connect(call, &pl);

	if (err)
		mem_deref(call);
	else if (callp)
		*callp = call;

 out:
	mem_deref(dialbuf);

	return err;
}


/**
 * Hangup the current call
 *
 * @param ua     User-Agent
 * @param call   Call to hangup, or NULL for current call
 * @param scode  Optional status code
 * @param reason Optional reason
 */
void ua_hangup(struct ua *ua, struct call *call,
	       uint16_t scode, const char *reason)
{
	if (!ua)
		return;

	if (!call) {
		call = ua_call(ua);
		if (!call)
			return;
	}

	(void)call_hangup(call, scode, reason);

	ua_event(ua, UA_EVENT_CALL_CLOSED, call, reason);

	//mem_deref(call);

	resume_call(ua);
}


/**
 * Answer an incoming call
 *
 * @param ua   User-Agent
 * @param call Call to answer, or NULL for current call
 *
 * @return 0 if success, otherwise errorcode
 */
int ua_answer(struct ua *ua, struct call *call)
{
	if (!ua)
		return EINVAL;

	if (!call) {
		call = ua_call(ua);
		if (!call)
			return ENOENT;
	}

	return call_answer(call, 200);
}


/**
 * Put the current call on hold and answer the incoming call
 *
 * @param ua   User-Agent
 * @param call Call to answer, or NULL for current call
 *
 * @return 0 if success, otherwise errorcode
 */
int ua_hold_answer(struct ua *ua, struct call *call)
{
	struct call *pcall;
	int err;

	if (!ua)
		return EINVAL;

	if (!call) {
		call = ua_call(ua);
		if (!call)
			return ENOENT;
	}

	/* put previous call on-hold */
	pcall = ua_prev_call(ua);
	if (pcall) {
		ua_printf(ua, "putting call with '%s' on hold\n",
		     call_peeruri(pcall));

		err = call_hold(pcall, true);
		if (err)
			return err;
	}

	return ua_answer(ua, call);
}


int ua_print_status(struct re_printf *pf, const struct ua *ua)
{
	struct le *le;
	int err;

	if (!ua)
		return 0;

	err = re_hprintf(pf, "%-42s", ua->acc->aor);

	for (le = ua->regl.head; le; le = le->next)
		err |= reg_status(pf, le->data);

	err |= re_hprintf(pf, "\n");

	return err;
}


/**
 * Send SIP OPTIONS message to a peer
 *
 * @param ua      User-Agent object
 * @param uri     Peer SIP Address
 * @param resph   Response handler
 * @param arg     Handler argument
 *
 * @return 0 if success, otherwise errorcode
 */
int ua_options_send(struct ua *ua, const char *uri,
		    options_resp_h *resph, void *arg)
{
	struct mbuf *dialbuf;
	int err = 0;

	if (!ua || !str_isset(uri))
		return EINVAL;

	dialbuf = mbuf_alloc(64);
	if (!dialbuf)
		return ENOMEM;

	err = uri_complete(ua, dialbuf, uri);
	if (err)
		goto out;

	dialbuf->buf[dialbuf->end] = '\0';

	err = sip_req_send(ua, "OPTIONS", (char *)dialbuf->buf, resph, arg,
			   "Accept: application/sdp\r\n"
			   "Content-Length: 0\r\n"
			   "\r\n");
	if (err) {
		warning("ua: send options: (%m)\n", err);
	}

 out:
	mem_deref(dialbuf);

	return err;
}


/**
 * Get the AOR of a User-Agent
 *
 * @param ua User-Agent object
 *
 * @return AOR
 */
const char *ua_aor(const struct ua *ua)
{
	return ua ? ua->acc->aor : NULL;
}


/**
 * Get presence status of a User-Agent
 *
 * @param ua User-Agent object
 *
 * @return presence status
 */
enum presence_status ua_presence_status(const struct ua *ua)
{
	return ua ? ua->my_status : PRESENCE_UNKNOWN;
}


/**
 * Set presence status of a User-Agent
 *
 * @param ua     User-Agent object
 * @param status Presence status
 */
void ua_presence_status_set(struct ua *ua, const enum presence_status status)
{
	if (!ua)
		return;

	ua->my_status = status;
}


/**
 * Get the outbound SIP proxy of a User-Agent
 *
 * @param ua User-Agent object
 *
 * @return Outbound SIP proxy uri
 */
const char *ua_outbound(const struct ua *ua)
{
	/* NOTE: we pick the first outbound server, should be rotated? */
	return ua ? ua->acc->outboundv[0] : NULL;
}


/**
 * Get the current call object of a User-Agent
 *
 * @param ua User-Agent object
 *
 * @return Current call, NULL if no active calls
 *
 *
 * Current call strategy:
 *
 * We can only have 1 current call. The current call is the one that was
 * added last (end of the list).
 */
struct call *ua_call(const struct ua *ua)
{
	if (!ua)
		return NULL;

	return list_ledata(list_tail(&ua->calls));
}


struct call *ua_prev_call(const struct ua *ua)
{
	struct le *le;
	int prev = 0;

	if (!ua)
		return NULL;

	for (le = ua->calls.tail; le; le = le->prev) {
		if ( prev == 1) {
			struct call *call = le->data;
			return call;
		}
		if ( prev == 0)
			prev = 1;
	}

	return NULL;
}


int ua_debug(struct re_printf *pf, const struct ua *ua)
{
	struct le *le;
	int err;

	if (!ua)
		return 0;

	err  = re_hprintf(pf, "--- %s ---\n", ua->acc->aor);
	err |= re_hprintf(pf, " nrefs:     %u\n", mem_nrefs(ua));
	err |= re_hprintf(pf, " cuser:     %s\n", ua->cuser);
	err |= re_hprintf(pf, " pub-gruu:  %s\n", ua->pub_gruu);
	err |= re_hprintf(pf, " af:        %s\n", net_af2name(ua->af));
	err |= re_hprintf(pf, " %H", ua_print_supported, ua);

	err |= account_debug(pf, ua->acc);

	for (le = ua->regl.head; le; le = le->next)
		err |= reg_debug(pf, le->data);

	return err;
}


/* One instance */


static int add_transp_af(const struct sa *laddr)
{
	struct sa local;
	int err = 0;

	if (str_isset(uag.cfg->local)) {
		err = sa_decode(&local, uag.cfg->local,
				str_len(uag.cfg->local));
		if (err) {
			err = sa_set_str(&local, uag.cfg->local, 0);
			if (err) {
				warning("ua: decode failed: '%s'\n",
					uag.cfg->local);
				return err;
			}
		}

		if (!sa_isset(&local, SA_ADDR)) {
			uint16_t port = sa_port(&local);
			(void)sa_set_sa(&local, &laddr->u.sa);
			sa_set_port(&local, port);
		}

		if (sa_af(laddr) != sa_af(&local))
			return 0;
	}
	else {
		sa_cpy(&local, laddr);
		sa_set_port(&local, 0);
	}

	if (uag.use_udp)
		err |= sip_transp_add(uag.sip, SIP_TRANSP_UDP, &local);
	if (uag.use_tcp)
		err |= sip_transp_add(uag.sip, SIP_TRANSP_TCP, &local);
	if (err) {
		warning("ua: SIP Transport failed: %m\n", err);
		return err;
	}

#ifdef USE_TLS
	if (uag.use_tls) {
		/* Build our SSL context*/
		if (!uag.tls) {
			const char *cert = NULL;

			if (str_isset(uag.cfg->cert)) {
				cert = uag.cfg->cert;
				info("SIP Certificate: %s\n", cert);
			}

			err = tls_alloc(&uag.tls, TLS_METHOD_SSLV23,
					cert, NULL);
			if (err) {
				warning("ua: tls_alloc() failed: %m\n", err);
				return err;
			}
		}

		if (sa_isset(&local, SA_PORT))
			sa_set_port(&local, sa_port(&local) + 1);

		err = sip_transp_add(uag.sip, SIP_TRANSP_TLS, &local, uag.tls);
		if (err) {
			warning("ua: SIP/TLS transport failed: %m\n", err);
			return err;
		}
	}
#endif

	return err;
}


static int ua_add_transp(struct network *net)
{
	int err = 0;

	if (!uag.prefer_ipv6) {
		if (sa_isset(net_laddr_af(net, AF_INET), SA_ADDR))
			err |= add_transp_af(net_laddr_af(net, AF_INET));
	}

#if HAVE_INET6
	if (sa_isset(net_laddr_af(net, AF_INET6), SA_ADDR))
		err |= add_transp_af(net_laddr_af(net, AF_INET6));
#endif

	return err;
}


static bool require_handler(const struct sip_hdr *hdr,
			    const struct sip_msg *msg, void *arg)
{
	struct ua *ua = arg;
	bool supported = false;
	size_t i;
	(void)msg;

	for (i=0; i<ua->extensionc; i++) {

		if (!pl_casecmp(&hdr->val, &ua->extensionv[i])) {
			supported = true;
			break;
		}
	}

	return !supported;
}


/* Handle incoming calls */
static void sipsess_conn_handler(const struct sip_msg *msg, void *arg)
{
	struct config *config = conf_config();
	const struct sip_hdr *hdr;
	struct ua *ua;
	struct call *call = NULL;
	char to_uri[256];
	int err;

	(void)arg;

	ua = uag_find(&msg->uri.user);
	if (!ua) {
		warning("ua: %r: UA not found: %r\n",
			&msg->from.auri, &msg->uri.user);
		(void)sip_treply(NULL, uag_sip(), msg, 404, "Not Found");
		return;
	}

	/* handle multiple calls */
	if (config->call.max_calls &&
	    list_count(&ua->calls) + 1 > config->call.max_calls) {

		info("ua: rejected call from %r (maximum %d calls)\n",
		     &msg->from.auri, config->call.max_calls);
		(void)sip_treply(NULL, uag.sip, msg, 486, "Max Calls");
		return;
	}

	/* Handle Require: header, check for any required extensions */
	hdr = sip_msg_hdr_apply(msg, true, SIP_HDR_REQUIRE,
				require_handler, ua);
	if (hdr) {
		info("ua: call from %r rejected with 420"
			     " -- option-tag '%r' not supported\n",
			     &msg->from.auri, &hdr->val);

		(void)sip_treplyf(NULL, NULL, uag.sip, msg, false,
				  420, "Bad Extension",
				  "Unsupported: %r\r\n"
				  "Content-Length: 0\r\n\r\n",
				  &hdr->val);
		return;
	}

	(void)pl_strcpy(&msg->to.auri, to_uri, sizeof(to_uri));

	err = ua_call_alloc(&call, ua, VIDMODE_ON, msg, NULL, to_uri);
	if (err) {
		warning("ua: call_alloc: %m\n", err);
		goto error;
	}

	err = call_accept(call, uag.sock, msg);
	if (err)
		goto error;

	return;

 error:
	mem_deref(call);
	(void)sip_treply(NULL, uag.sip, msg, 500, "Call Error");
}


static void net_change_handler(void *arg)
{
	(void)arg;

	info("IP-address changed: %j\n",
	     net_laddr_af(baresip_network(), AF_INET));

	(void)uag_reset_transp(true, true);
}


static int cmd_quit(struct re_printf *pf, void *unused)
{
	int err;

	(void)unused;

	err = re_hprintf(pf, "Quit\n");

	ua_stop_all(false);

	return err;
}


static const struct cmd cmdv[] = {
	{"quit", 'q', 0, "Quit",                     cmd_quit             },
};


static bool sub_handler(const struct sip_msg *msg, void *arg)
{
	struct ua *ua;

	(void)arg;

	ua = uag_find(&msg->uri.user);
	if (!ua) {
		warning("subscribe: no UA found for %r\n", &msg->uri.user);
		(void)sip_treply(NULL, uag_sip(), msg, 404, "Not Found");
		return true;
	}

	if (uag.subh)
		uag.subh(msg, ua);

	return true;
}


/**
 * Initialise the User-Agents
 *
 * @param software    SIP User-Agent string
 * @param udp         Enable UDP transport
 * @param tcp         Enable TCP transport
 * @param tls         Enable TLS transport
 * @param prefer_ipv6 Prefer IPv6 flag
 *
 * @return 0 if success, otherwise errorcode
 */
int ua_init(const char *software, bool udp, bool tcp, bool tls,
	    bool prefer_ipv6)
{
	struct config *cfg = conf_config();
	struct network *net = baresip_network();
	uint32_t bsize;
	int err;

	if (!net) {
		warning("ua: no network\n");
		return EINVAL;
	}

	uag.cfg = &cfg->sip;
	bsize = cfg->sip.trans_bsize;

	uag.use_udp = udp;
	uag.use_tcp = tcp;
	uag.use_tls = tls;
	uag.prefer_ipv6 = prefer_ipv6;

	list_init(&uag.ual);

	err = sip_alloc(&uag.sip, net_dnsc(net), bsize, bsize, bsize,
			software, exit_handler, NULL);
	if (err) {
		warning("ua: sip stack failed: %m\n", err);
		goto out;
	}

	err = ua_add_transp(net);
	if (err)
		goto out;

	err = sip_listen(&uag.lsnr, uag.sip, true, request_handler, NULL);
	if (err)
		goto out;

	err = sipsess_listen(&uag.sock, uag.sip, bsize,
			     sipsess_conn_handler, NULL);
	if (err)
		goto out;

	err = sipevent_listen(&uag.evsock, uag.sip, bsize, bsize,
			      sub_handler, NULL);
	if (err)
		goto out;

	err = cmd_register(baresip_commands(), cmdv, ARRAY_SIZE(cmdv));
	if (err)
		goto out;

	net_change(net, 60, net_change_handler, NULL);

 out:
	if (err) {
		warning("ua: init failed (%m)\n", err);
		ua_close();
	}
	return err;
}


/**
 * Close all active User-Agents
 */
void ua_close(void)
{
	cmd_unregister(baresip_commands(), cmdv);
	ui_reset();

	uag.evsock   = mem_deref(uag.evsock);
	uag.sock     = mem_deref(uag.sock);
	uag.lsnr     = mem_deref(uag.lsnr);
	uag.sip      = mem_deref(uag.sip);
	uag.eprm     = mem_deref(uag.eprm);

#ifdef USE_TLS
	uag.tls = mem_deref(uag.tls);
#endif

	list_flush(&uag.ual);
	list_flush(&uag.ehl);

	/* note: must be done before mod_close() */
	module_app_unload();
}


/**
 * Stop all User-Agents
 *
 * @param forced True to force, otherwise false
 */
void ua_stop_all(bool forced)
{
	struct le *le;
	bool ext_ref = false;

	info("ua: stop all (forced=%d)\n", forced);

	/* check if someone else has grabbed a ref to ua */
	le = uag.ual.head;
	while (le) {

		struct ua *ua = le->data;
		le = le->next;

		if (mem_nrefs(ua) > 1) {

			list_unlink(&ua->le);
			list_flush(&ua->calls);
			mem_deref(ua);

			ext_ref = true;
		}

		ua_event(ua, UA_EVENT_SHUTDOWN, NULL, NULL);
	}

	if (ext_ref) {
		info("ua: ext_ref -> cannot unload mods\n");
		return;
	}
	else {
		module_app_unload();
	}

	if (!list_isempty(&uag.ual)) {
		const uint32_t n = list_count(&uag.ual);
		info("Stopping %u useragent%s.. %s\n",
		     n, n==1 ? "" : "s", forced ? "(Forced)" : "");
	}

	if (forced)
		sipsess_close_all(uag.sock);
	else
		list_flush(&uag.ual);

	sip_close(uag.sip, forced);
}


/**
 * Set the global UA exit handler. The exit handler will be called
 * asyncronously when the SIP stack has exited.
 *
 * @param exith Exit handler
 * @param arg   Handler argument
 */
void uag_set_exit_handler(ua_exit_h *exith, void *arg)
{
	uag.exith = exith;
	uag.arg = arg;
}


/**
 * Reset the SIP transports for all User-Agents
 *
 * @param reg      True to reset registration
 * @param reinvite True to update active calls
 *
 * @return 0 if success, otherwise errorcode
 */
int uag_reset_transp(bool reg, bool reinvite)
{
	struct network *net = baresip_network();
	struct le *le;
	int err;

	/* Update SIP transports */
	sip_transp_flush(uag.sip);

	(void)net_check(net);
	err = ua_add_transp(net);
	if (err)
		return err;

	/* Re-REGISTER all User-Agents */
	for (le = uag.ual.head; le; le = le->next) {
		struct ua *ua = le->data;

		if (reg && ua->acc->regint) {
			err |= ua_register(ua);
		}

		/* update all active calls */
		if (reinvite) {
			struct le *lec;

			for (lec = ua->calls.head; lec; lec = lec->next) {
				struct call *call = lec->data;
				const struct sa *laddr;

				laddr = net_laddr_af(net, call_af(call));

				err |= call_reset_transp(call, laddr);
			}
		}
	}

	return err;
}


/**
 * Print the SIP Status for all User-Agents
 *
 * @param pf     Print handler for debug output
 * @param unused Unused parameter
 *
 * @return 0 if success, otherwise errorcode
 */
int ua_print_sip_status(struct re_printf *pf, void *unused)
{
	(void)unused;
	return sip_debug(pf, uag.sip);
}


/**
 * Print all calls for a given User-Agent
 *
 * @param pf     Print handler for debug output
 * @param ua     User-Agent
 *
 * @return 0 if success, otherwise errorcode
 */
int ua_print_calls(struct re_printf *pf, const struct ua *ua)
{
	uint32_t n, count=0;
	uint32_t linenum;
	int err = 0;

	if (!ua) {
		err |= re_hprintf(pf, "\n--- No active calls ---\n");
		return err;
	}

	n = list_count(&ua->calls);

	err |= re_hprintf(pf, "\n--- List of active calls (%u): ---\n",
			  n);

	for (linenum=CALL_LINENUM_MIN; linenum<CALL_LINENUM_MAX; linenum++) {

		const struct call *call;

		call = call_find_linenum(&ua->calls, linenum);
		if (call) {
			++count;

			err |= re_hprintf(pf, "  %c %H\n",
					  call == ua_call(ua) ? '>' : ' ',
					  call_info, call);
		}

		if (count >= n)
			break;
	}

	err |= re_hprintf(pf, "\n");

	return err;
}


/**
 * Get the global SIP Stack
 *
 * @return SIP Stack
 */
struct sip *uag_sip(void)
{
	return uag.sip;
}


/**
 * Get the global SIP Session socket
 *
 * @return SIP Session socket
 */
struct sipsess_sock *uag_sipsess_sock(void)
{
	return uag.sock;
}


/**
 * Get the global SIP Event socket
 *
 * @return SIP Event socket
 */
struct sipevent_sock *uag_sipevent_sock(void)
{
	return uag.evsock;
}


struct tls *uag_tls(void)
{
#ifdef USE_TLS
	return uag.tls;
#else
	return NULL;
#endif
}


/**
 * Get the name of the User-Agent event
 *
 * @param ev User-Agent event
 *
 * @return Name of the event
 */
const char *uag_event_str(enum ua_event ev)
{
	switch (ev) {

	case UA_EVENT_REGISTERING:          return "REGISTERING";
	case UA_EVENT_REGISTER_OK:          return "REGISTER_OK";
	case UA_EVENT_REGISTER_FAIL:        return "REGISTER_FAIL";
	case UA_EVENT_UNREGISTERING:        return "UNREGISTERING";
	case UA_EVENT_SHUTDOWN:             return "SHUTDOWN";
	case UA_EVENT_EXIT:                 return "EXIT";
	case UA_EVENT_CALL_INCOMING:        return "CALL_INCOMING";
	case UA_EVENT_CALL_RINGING:         return "CALL_RINGING";
	case UA_EVENT_CALL_PROGRESS:        return "CALL_PROGRESS";
	case UA_EVENT_CALL_ESTABLISHED:     return "CALL_ESTABLISHED";
	case UA_EVENT_CALL_CLOSED:          return "CALL_CLOSED";
	case UA_EVENT_CALL_TRANSFER_FAILED: return "TRANSFER_FAILED";
	case UA_EVENT_CALL_DTMF_START:      return "CALL_DTMF_START";
	case UA_EVENT_CALL_DTMF_END:        return "CALL_DTMF_END";
	default: return "?";
	}
}


/**
 * Get the current SIP socket file descriptor for a User-Agent
 *
 * @param ua User-Agent
 *
 * @return File descriptor, or -1 if not available
 */
int ua_sipfd(const struct ua *ua)
{
	struct le *le;

	if (!ua)
		return -1;

	for (le = ua->regl.head; le; le = le->next) {

		struct reg *reg = le->data;
		int fd;

		fd = reg_sipfd(reg);
		if (fd != -1)
			return fd;
	}

	return -1;
}


/**
 * Find the correct UA from the contact user
 *
 * @param cuser Contact username
 *
 * @return Matching UA if found, NULL if not found
 */
struct ua *uag_find(const struct pl *cuser)
{
	struct le *le;

	for (le = uag.ual.head; le; le = le->next) {
		struct ua *ua = le->data;

		if (0 == pl_strcasecmp(cuser, ua->cuser))
			return ua;
	}

	/* Try also matching by AOR, for better interop */
	for (le = uag.ual.head; le; le = le->next) {
		struct ua *ua = le->data;

		if (0 == pl_casecmp(cuser, &ua->acc->luri.user))
			return ua;
	}

	return NULL;
}


/**
 * Find a User-Agent (UA) from an Address-of-Record (AOR)
 *
 * @param aor Address-of-Record string
 *
 * @return User-Agent (UA) if found, otherwise NULL
 */
struct ua *uag_find_aor(const char *aor)
{
	struct le *le;

	for (le = uag.ual.head; le; le = le->next) {
		struct ua *ua = le->data;

		if (str_isset(aor) && str_cmp(ua->acc->aor, aor))
			continue;

		return ua;
	}

	return NULL;
}


/**
 * Find a User-Agent (UA) which has certain address parameter and/or value
 *
 * @param name  SIP Address parameter name
 * @param value SIP Address parameter value (optional)
 *
 * @return User-Agent (UA) if found, otherwise NULL
 */
struct ua *uag_find_param(const char *name, const char *value)
{
	struct le *le;

	for (le = uag.ual.head; le; le = le->next) {
		struct ua *ua = le->data;
		struct sip_addr *laddr = account_laddr(ua->acc);
		struct pl val;

		if (value) {

			if (0 == msg_param_decode(&laddr->params, name, &val)
			    &&
			    0 == pl_strcasecmp(&val, value)) {
				return ua;
			}
		}
		else {
			if (0 == msg_param_exists(&laddr->params, name, &val))
				return ua;
		}
	}

	return NULL;
}


/**
 * Get the contact user/uri of a User-Agent (UA)
 *
 * If the Public GRUU is set, it will be returned.
 * Otherwise the local contact-user (cuser) will be returned.
 *
 * @param ua User-Agent
 *
 * @return Contact user
 */
const char *ua_cuser(const struct ua *ua)
{
	if (!ua)
		return NULL;

	if (str_isset(ua->pub_gruu))
		return ua->pub_gruu;

	return ua->cuser;
}


const char *ua_local_cuser(const struct ua *ua)
{
	return ua ? ua->cuser : NULL;
}


/**
 * Get Account of a User-Agent
 *
 * @param ua User-Agent
 *
 * @return Pointer to UA's account
 */
struct account *ua_account(const struct ua *ua)
{
	return ua ? ua->acc : NULL;
}


/**
 * Set Public GRUU of a User-Agent (UA)
 *
 * @param ua   User-Agent
 * @param pval Public GRUU
 */
void ua_pub_gruu_set(struct ua *ua, const struct pl *pval)
{
	if (!ua)
		return;

	ua->pub_gruu = mem_deref(ua->pub_gruu);
	(void)pl_strdup(&ua->pub_gruu, pval);
}


struct list *uag_list(void)
{
	return &uag.ual;
}


/**
 * Return list of methods supported by the UA
 *
 * @return String of supported methods
 */
const char *uag_allowed_methods(void)
{
	return "INVITE,ACK,BYE,CANCEL,OPTIONS,REFER,"
		"NOTIFY,SUBSCRIBE,INFO,MESSAGE";
}


int ua_print_supported(struct re_printf *pf, const struct ua *ua)
{
	size_t i;
	int err;

	err = re_hprintf(pf, "Supported:");

	for (i=0; i<ua->extensionc; i++) {
		err |= re_hprintf(pf, "%s%r",
				  i==0 ? " " : ",", &ua->extensionv[i]);
	}

	err |= re_hprintf(pf, "\r\n");

	return err;
}


struct list *ua_calls(const struct ua *ua)
{
	return ua ? (struct list *)&ua->calls : NULL;
}


static void eh_destructor(void *arg)
{
	struct ua_eh *eh = arg;
	list_unlink(&eh->le);
}


int uag_event_register(ua_event_h *h, void *arg)
{
	struct ua_eh *eh;

	if (!h)
		return EINVAL;

	uag_event_unregister(h);

	eh = mem_zalloc(sizeof(*eh), eh_destructor);
	if (!eh)
		return ENOMEM;

	eh->h = h;
	eh->arg = arg;

	list_append(&uag.ehl, &eh->le, eh);

	return 0;
}


void uag_event_unregister(ua_event_h *h)
{
	struct le *le;

	for (le = uag.ehl.head; le; le = le->next) {

		struct ua_eh *eh = le->data;

		if (eh->h == h) {
			mem_deref(eh);
			break;
		}
	}
}


void uag_set_sub_handler(sip_msg_h *subh)
{
	uag.subh = subh;
}


void uag_current_set(struct ua *ua)
{
	uag.ua_cur = ua;
}


struct ua *uag_current(void)
{
	if (list_isempty(uag_list()))
		return NULL;

	return uag.ua_cur;
}


void ua_set_media_af(struct ua *ua, int af_media)
{
	if (!ua)
		return;

	ua->af_media = af_media;
}


int uag_set_extra_params(const char *eprm)
{
	uag.eprm = mem_deref(uag.eprm);

	if (eprm)
		return str_dup(&uag.eprm, eprm);

	return 0;
}
