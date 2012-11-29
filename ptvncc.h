/*=======================================================================
 *	VNC Client
 *=======================================================================*/
#ifndef INC_ptvncc_h
#define INC_ptvncc_h

#include <glib.h>

/*--- Opaque data type */
typedef struct _VncClient VncClient;

/*--- Supported pixel formats */
typedef enum {
  VNCFORMAT_UNKNOWN = -1,
  VNCFORMAT_NONE,		/* No preference, use server setting */
  VNCFORMAT_RGB565,
} VncFormat;

/*--- Abstract interface for painting display */
typedef gboolean VncDisplayOpen(void *, const char */*name*/,
				guint /*width*/, guint /*height*/,
				VncFormat /*format*/,
				GError **);
typedef gboolean VncDisplayPaintRow(void *, guint /*x*/,guint /*y*/,
				    gsize /*count*/, const void */*pixels*/,
				    GError **);
typedef gboolean VncDisplayFillRect(void *, guint /*x*/,guint /*y*/,
				    guint /*width*/,guint /*height*/,
				    guint /*pixel*/,
				    GError **);
typedef gboolean VncDisplayCopyRect(void *, guint /*x*/,guint /*y*/,
				    guint /*width*/,guint /*height*/,
				    guint /*srcx*/,guint /*srcy*/,
				    GError **);
typedef gboolean VncDisplayUpdateDone(void *, GError **);

typedef struct {
  gsize size;
  VncDisplayOpen *open;
  VncDisplayPaintRow *paint_row;
  VncDisplayFillRect *fill_rect;
  VncDisplayCopyRect *copy_rect;
  VncDisplayUpdateDone *update_done;
} VncDisplayMethods;

typedef struct {
  const VncDisplayMethods *methods;
  void *priv;				/* Provate data */
} IVncDisplay;

extern VncClient *vncclient_create(const char */*host*/, guint16 /*port*/,
				   gboolean /*shared*/, VncFormat,
				   GError **);
extern void vncclient_set_display(VncClient *, const IVncDisplay *);
extern gboolean vncclient_run(VncClient *, GError **);
extern gboolean vncclient_request_all(VncClient *, GError **);

#endif /* INC_ptvncc_h */
