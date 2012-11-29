/*=======================================================================
 *	VNC client
 *=======================================================================*/
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "vncconst.h"

typedef enum {
  VNCFORMAT_NONE,		/* No preference, use server setting */
  VNCFORMAT_RGB565,
} VncFormat;

typedef struct _VncClient VncClient;

struct _VncClient {
  /* config */
  const char *host;
  guint16 port;
  gboolean shared;
  VncFormat format;
  const char *password;
  /* FIXME region */
  /* state */
  int sock;
  int version;			/* E.g. 3.8 as 3008 */
  guint32 security;
  /* image details */
  guint16 width, height;
  guint8 bpp, depth, be, tru;
  guint16 max[3];
  guint8 shift[3];
  gsize pixbytes;
  char *name;
};

#define VERSION(hi,lo) (((hi) << 8) | (lo))
#define VERSION_HI(v) (((v) >> 8) & 0xff)
#define VERSION_LO(v) ((v) & 0xff)

static gboolean _handshake(VncClient *self, GError **error);
static gboolean _security(VncClient *self, GError **error);
static gboolean _initialisation(VncClient *self, GError **error);
static gboolean _send_SetPixelFormat(VncClient *self,
				     guint bpp, guint depth, gboolean be, gboolean tru,
				     guint rmax, guint gmax, guint bmax,
				     guint rsh, guint gsh, guint bsh,
				     GError **error);
static gboolean _send_FrameBufferUpdateRequest(VncClient *self,
					       guint xpos, guint ypos, guint width, guint height,
					       gboolean incremental,
					       GError **error);
static void _calc_pixel(VncClient *self);
static gboolean _recv_request(VncClient *self, GError **error);
static gboolean _recv_FramebufferUpdate(VncClient *self, GError **error);
static gboolean _read(VncClient *self, void *data, size_t size, GError **error);
static gboolean _write(VncClient *self, const void *data, size_t size, GError **error);

static GQuark 
_vncclient_error_quark(void) {
  return g_quark_from_static_string("vncclient-error");
}

#define VNCCLIENT_ERROR _vncclient_error_quark()
enum {
  VNCCLIENT_E_BAD_HOST = 1,
  VNCCLIENT_E_SOCKET,
  VNCCLIENT_E_CONNECT,
  VNCCLIENT_E_BAD_PROTOCOL,
  VNCCLIENT_E_TOO_OLD_VERSION,
  VNCCLIENT_E_HANDSHAKE,
  VNCCLIENT_E_NO_SUPPORTED_SECURITY,
  VNCCLIENT_E_SECURITY_FAILED,
  VNCCLIENT_E_SECURITY_UNKNOWN,
  VNCCLIENT_E_SOCK_READ,
  VNCCLIENT_E_BAD_MESSAGE,
  VNCCLIENT_E_BAD_ENCODING,
};

#define ERROR(t,s) g_set_error(error, VNCCLIENT_ERROR, VNCCLIENT_E_##t, "%s",s)
#define ERROR1(t,f,a1) g_set_error(error, VNCCLIENT_ERROR, VNCCLIENT_E_##t, f,a1)
#define ERROR2(t,f,a1,a2) g_set_error(error, VNCCLIENT_ERROR, VNCCLIENT_E_##t, f,a1,a2)
#define ERROR3(t,f,a1,a2,a3) g_set_error(error, VNCCLIENT_ERROR, VNCCLIENT_E_##t, f,a1,a2,a3)

VncClient *
vncclient_create(const char *host, guint16 port, gboolean shared, VncFormat format, GError **error)
{
  VncClient *self = g_new0(VncClient, 1);
  self->host = g_strdup(host);
  self->port = port;
  self->shared = shared;
  self->format = format;
  return self;
}

gboolean
vncclient_run(VncClient *self, GError **error)
{
  struct sockaddr_in inaddr;
  struct hostent *he;
  he = gethostbyname(self->host);
  if (he == NULL) {
    ERROR1(BAD_HOST, "Unknown hostname \"%s\"", self->host);
    return FALSE;
  }
  inaddr.sin_family = AF_INET;
  inaddr.sin_port = htons(self->port);
  memcpy(&inaddr.sin_addr, he->h_addr_list[0], sizeof(inaddr.sin_addr));

  self->sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (self->sock < 0) {
    ERROR1(SOCKET, "Could not open TCP socket: errno=%d", errno);
    return FALSE;
  }
  if (connect(self->sock, (const struct sockaddr *)&inaddr, sizeof(inaddr)) < 0) {
    ERROR3(CONNECT, "Cannot connect to %s:%d: errno=%d", self->host, self->port, errno);
    close(self->sock);
    return FALSE;
  }
  if (! _handshake(self, error)) return FALSE;
  if (! _security(self, error)) return FALSE;
  if (! _initialisation(self, error)) return FALSE;
#if NOTYET
  if (self->format != VNCFORMAT_NONE) {
    if (! _set_format(self, error)) return FALSE;
  }
#endif
  /* FIXME */
  return TRUE;
}

#define READ(p,n) do {if (! _read(self, p, n, error)) return FALSE;} while (0)
#define READU8(pb) do {guint8 _v; READ(&_v, 1); *(pb)=_v;} while (0)
#define READU16(ph) do {guint16 _v; READ(&_v, 2); *(ph)=ntohs(_v);} while (0)
#define READU32(pi) do {guint32 _v; READ(&_v, 4); *(pi)=ntohl(_v);} while (0)
#define READI32(pi) do {gint32 _v; READ(&_v, 4); *(pi)=ntohl(_v);} while (0)
#define READSTRING(ps) do {guint32 _len; READU32(&_len); *(ps) = g_malloc(_len+1); if (! _read(self, *(ps), _len, error)) {g_free(*ps); return FALSE;}; (*(ps))[_len] = '\0';} while (0)
#define WRITE(p,n) do {if (! _write(self, p, n, error)) return FALSE;} while (0)
#define WRITEU8(b) do {guint8 _v = (b); WRITE(&_v, 1);} while (0)
#define WRITEU16(b) do {guint16 _v = htons(b); WRITE(&_v, 2);} while (0)
#define WRITEU32(b) do {guint32 _v = htonl(b); WRITE(&_v, 4);} while (0)
#define FLUSH()

#define atoi3(s) ((((s)[0]-'0')*10 + (s)[1]-'0')*10 + (s)[2]-'0')

/* Client has connected.  Expect ProtocolVersion from server */
static gboolean
_handshake(VncClient *self, GError **error)
{
  unsigned char msg[12+1];
  guint vhi, vlo;

  READ(msg, 12);
  if (! (strncmp(msg, "RFB ", 4) == 0 &&
	 isdigit(msg[4]) && isdigit(msg[5]) && isdigit(msg[6]) &&
	 msg[7] == '.' &&
	 isdigit(msg[8]) && isdigit(msg[9]) && isdigit(msg[10]) &&
	 msg[11] == 10)) {
    ERROR(BAD_PROTOCOL, "Bad ProtocolVersion message");
    return FALSE;
  }
  vhi = atoi3(&msg[4]);
  vlo = atoi3(&msg[8]);
  if (VERSION(vhi,vlo) < VERSION(3,3)) {
    ERROR2(TOO_OLD_VERSION, "Server supports version %d.%d only", vhi, vlo);
    return FALSE;
  } else if (VERSION(vhi,vlo) >= VERSION(3,8)) {
    self->version = VERSION(3,8);
  } else if (VERSION(vhi,vlo) >= VERSION(3,7)) {
    self->version = VERSION(3,7);
  } else {
    self->version = VERSION(3,3);
  }
  sprintf(msg, "RFB %03d.%03d\012", VERSION_HI(self->version), VERSION_LO(self->version));
  WRITE(msg, 12);
  return TRUE;
}

/* Negotiate security */
static gboolean
_security(VncClient *self, GError **error)
{
  if (self->version >= VERSION(3,7)) {
    guint8 nsec;
    guint8 *secs = NULL;
    guint i;
    READU8(&nsec);
    g_debug("nsec=%d", nsec);
    if (nsec == 0) {
      /* Error */
      char *reason = NULL;
      READSTRING(&reason);
      ERROR1(HANDSHAKE, "Server error in handshake: %s", reason);
      g_free(reason);
      return FALSE;
    }
    secs = g_new0(guint8, nsec);
    READ(secs, nsec);
    for (i=0; i < nsec; i++) {
      if (secs[i] == SECURITY_NONE || secs[i] == SECURITY_VNC_Authentication) {
	break;
      }
    }
    if (i == nsec) {
      /* None found which is acceptable */
      ERROR(NO_SUPPORTED_SECURITY, "No compatible security type");
      g_free(secs);
      return FALSE;
    }
    self->security = secs[i];
    WRITEU8(self->security);
    switch (self->security) {
    case SECURITY_VNC_Authentication:
      g_debug("VNC Auth");
      guint8 challenge[16], response[16];
      READ(challenge, 16);
      /* FIXME_DES(challenge, self->password, response); */
      WRITE(response, 16);
      break;
    case SECURITY_NONE:
      break;
    }
    g_free(secs);
    WRITEU8(self->security);

  } else {
    /* 3.3 */
    READU32(&self->security);
  }
  g_debug("security = %u", self->security);

  if (self->version >= VERSION(3,8)) {
    /* SecurityResult */
    guint32 sec_result;
    READU32(&sec_result);
    if (sec_result == 0) {
      /* OK */
    } else if (sec_result == 1) {
      /* Failed */
      char *reason = NULL;
      if (self->version >= VERSION(3,8)) {
	READSTRING(&reason);
      }
      ERROR1(SECURITY_FAILED, "Server says security handshake failed: %s",
	     reason ? reason : "unknown reason");
      g_free(reason);
      return FALSE;
    } else {
      ERROR1(SECURITY_UNKNOWN, "Unexpected SecurityResult %u", sec_result);
      return FALSE;
    }
  }
  return TRUE;
}

static gboolean
_initialisation(VncClient *self, GError **error)
{
  struct {
    guint16 width, height;
    guint8 bpp, depth, be, tru;
    guint16 rmax, gmax, bmax;
    guint8 rsh, gsh, bsh;
    guint8 _pad[3];
  } head;
  /* ClientInit */
  WRITEU8(self->shared);
  /* ServerInit */
  READ(&head, sizeof(head));
  self->width = ntohs(head.width);
  self->height = ntohs(head.height);
  self->bpp = head.bpp;
  self->depth = head.depth;
  self->be = head.be;
  self->tru = head.tru;
  self->max[0] = head.rmax;
  self->max[1] = head.gmax;
  self->max[2] = head.bmax;
  self->shift[0] = head.rsh;
  self->shift[1] = head.gsh;
  self->shift[2] = head.bsh;
  g_debug("bpp=%d depth=%d be=%d tru=%d max=%d,%d,%d shift=%d,%d,%d",
	 self->bpp, self->depth, self->be, self->tru,
	 self->max[0], self->max[1], self->max[2],
	 self->shift[0], self->shift[1], self->shift[2]);
  READSTRING(&self->name);
  g_debug("name = %s", self->name);
  return TRUE;
}

static gboolean
_send_SetPixelFormat(VncClient *self,
		     guint bpp, guint depth, gboolean be, gboolean tru,
		     guint rmax, guint gmax, guint bmax,
		     guint rsh, guint gsh, guint bsh,
		     GError **error)
{
  self->bpp = bpp;
  self->depth = depth;
  self->be = be;
  self->tru = tru;
  self->max[0] = rmax;
  self->max[1] = gmax;
  self->max[2] = bmax;
  self->shift[0] = rsh;
  self->shift[1] = gsh;
  self->shift[2] = bsh;
  struct {
    guint8 mtype;
    guint8 _pad[3];
    guint8 bpp;
    guint8 depth;
    guint8 be;
    guint8 tru;
    guint16 rmax, gmax, bmax;
    guint8 rsh, gsh, bsh;
    guint8 _pad2[3];
  } msg;
  msg.mtype = CLIENT_MSG_SetPixelFormat;
  msg.bpp = bpp;
  msg.depth = depth;
  msg.be = be;
  msg.tru = tru;
  msg.rmax = htons(rmax);
  msg.gmax = htons(gmax);
  msg.bmax = htons(bmax);
  msg.rsh = rsh;
  msg.gsh = gsh;
  msg.bsh = bsh;
  WRITE(&msg, sizeof(msg));
  _calc_pixel(self);
  return TRUE;
}

static gboolean
_send_FrameBufferUpdateRequest(VncClient *self,
			       guint xpos, guint ypos, guint width, guint height,
			       gboolean incremental,
			       GError **error)
{
  struct {
    guint8 msgtype, incremental;
    guint16 xpos, ypos;
    guint16 width, height;
  } msg;
  msg.msgtype = CLIENT_MSG_FramebufferUpdateRequest;
  msg.incremental = incremental;
  msg.xpos = htons(xpos);
  msg.ypos = htons(ypos);
  msg.width = htons(width);
  msg.height = htons(height);
  WRITE(&msg, sizeof(msg));
  FLUSH();
  return TRUE;
}

gboolean
vncclient_request_all(VncClient *self, GError **error)
{
  return _send_FrameBufferUpdateRequest(self, 0, 0, self->width, self->height, FALSE, error);
}

static void
_calc_pixel(VncClient *self)
{
  self->pixbytes = self->bpp / 8;
  /* FIXME pixend, pixfmt */
}

static gboolean
_recv_request(VncClient *self, GError **error)
{
  guint mtype;
  READU8(&mtype);
  switch (mtype) {
  case SERVER_MSG_FramebufferUpdate:
    if (! _recv_FramebufferUpdate(self, error)) return FALSE;
    break;
  default:
    ERROR1(BAD_MESSAGE, "Unexpected message %d from server", mtype);
    break;
  }
  return TRUE;
}

static gboolean
_recv_FramebufferUpdate(VncClient *self, GError **error)
{
  guint8 pad;
  guint16 nrect, i;
  READ(&pad, 1);
  READU16(&nrect);
  g_debug(" nrect = %d", nrect);
  for (i=0; i < nrect; i++) {
    guint16 xpos, ypos;
    guint16 width, height;
    gint32 enc;
    READU16(&xpos);
    READU16(&ypos);
    READU16(&width);
    READU16(&height);
    READI32(&enc);
    g_debug(" [%d] pos=(%d,%d) size=(%d,%d) enc=%d",
	    i, xpos,ypos, width,height, enc);
    switch (enc) {
#if NOTYET
    case ENCODING_Raw:
      if (! _update_RAW(self, xpos,ypos, width,height, error)) return FALSE;
      break;
    case ENCODING_CopyRect:
      if (! _update_COPYRECT(self, xpos,ypos, width,height, error)) return FALSE;
      break;
    case ENCODING_RRE:
      if (! _update_RRE(self, xpos,ypos, width,height, error)) return FALSE;
      break;
#endif
    default:
      ERROR1(BAD_ENCODING, "Unexpected rectangle encoding %d", enc);
      return FALSE;
    }
  }
  return TRUE;
}

static gboolean
_read(VncClient *self, void *data, size_t size, GError **error)
{
  size_t nread;
  nread = read(self->sock, data, size);
  if (nread < size) {
    ERROR1(SOCK_READ, "Socket read error %d", errno);
    return FALSE;
  }
  return TRUE;
}

static gboolean
_write(VncClient *self, const void *data, size_t size, GError **error)
{
  size_t nwritten;
  nwritten = write(self->sock, data, size);
  if (nwritten < size) {
    ERROR1(SOCK_READ, "Socket write error %d", errno);
    return FALSE;
  }
  return TRUE;
}

#include <stdlib.h>
int main(int argc, char **argv)
{
  const char *host = argv[1];
  int port = 5900;
  VncClient *client;
  GError *errorv = NULL, **error = &errorv;

  if (argc > 2) {
    port = 5900 + atoi(argv[2]);
  }
  client = vncclient_create(host, port, TRUE, VNCFORMAT_RGB565, error);
  if (! client) goto fail;
  if (! vncclient_run(client, error)) goto fail;

  return 0;
 fail:
  fprintf(stderr, "%s: %s\n", argv[0], errorv->message);
  return 1;
}
