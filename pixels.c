/*-----------------------------------------------------------------------
 *	Define SYMN(sym) to append 8,16,32
 *-----------------------------------------------------------------------*/
#define GUINTN SYMN(guint)

static gboolean
SYMN(_update_RAW)(VncClient *self,
		  guint xpos,guint ypos, guint width, guint height,
		  GError **error)
{
  int i;
  gsize rowbytes = width * self->pixbytes;
  /*--- FIXME assumes 16-bit */
  GUINTN *row = g_new(GUINTN, width);
  for (i=0; i < height; i++) {
    READ(row, rowbytes);
    if (width <= 1) {
      g_debug("  [%d]: %04x", i, row[0]);
    } else if (width <= 2) {
      g_debug("  [%d]: %04x %04x", i, row[0],row[1]);
    } else if (width <= 3) {
      g_debug("  [%d]: %04x %04x %04x", i, row[0],row[1],row[2]);
    } else {
      g_debug("  [%d]: %04x %04x %04x %04x%s", i, row[0],row[1],row[2],row[3],
	      width > 4 ? "...":"");
    }
    if (CAN(self->display, paint_row)) {
      if (! CALL5(self->display, paint_row, xpos, ypos+i, width, row, error)) {
	return FALSE;
      }
    }
  }
  g_free(row);
  return TRUE;
}

static gboolean
SYMN(_update_RRE)(VncClient *self,
		  guint xpos,guint ypos, guint width, guint height,
		  GError **error)
{
  guint32 nsub;
  GUINTN fg, bg;
  guint16 x,y,w,h;
  guint i;
  READU32(&nsub);
  READ(&bg, sizeof(bg));
  g_debug("  nsub=%u bg=%04x", nsub, bg);
  if (! _fill_rect(self, xpos, ypos, width, height, bg, error)) return FALSE;
  for (i=0; i < nsub; i++) {
    READ(&fg, sizeof(fg));
    READU16(&x);
    READU16(&y);
    READU16(&w);
    READU16(&h);
    g_debug("  {%d}: pos=(%d,%d) size=(%d,%d) fg=%04x",
	    i, x,y, w,h, fg);
    if (! _fill_rect(self, xpos+x, ypos+y, w, h, fg, error)) return FALSE;
  }
  return TRUE;
}

#define HEXTILE_RAW	1
#define HEXTILE_BG	2
#define HEXTILE_FG	4
#define HEXTILE_ANYSUB	8
#define HEXTILE_COLOUR	16

static gboolean
SYMN(_update_Hextile)(VncClient *self,
		      guint xpos,guint ypos, guint width, guint height,
		      GError **error)
{
  guint cols = (width+15)/16;
  guint rows = (height+15)/16;
  GUINTN fg = 0, bg = 0;
  guint row, col;
  for (row=0; row < rows; row++) {
    guint y0 = ypos + 16*row;
    guint y1 = MIN(y0+16, ypos+height);
    for (col=0; col < cols; col++) {
      guint x0 = xpos + 16*col;
      guint x1 = MIN(x0+16, xpos+width);
      guint8 subtype;
      READU8(&subtype);
      g_debug("  {%u,%u} subtype=%02x", x0, y0, subtype);
      if (subtype & HEXTILE_RAW) {
	GUINTN pixels[16];
	guint y;
	for (y=y0; y < y1; y++) {
	  READ(pixels, (x1-x0)*self->pixbytes);
	  if (CAN(self->display, paint_row)) {
	    if (! CALL5(self->display, paint_row, x0,y, x1-x0, pixels, error)) {
	      return FALSE;
	    }
	  }
	}
      } else {
	if (subtype & HEXTILE_BG) {
	  SYMN(READU)(&bg);
	}
	if (subtype & HEXTILE_FG) {
	  SYMN(READU)(&fg);
	}
	if (! _fill_rect(self, x0,y0, x1-x0,y1-y0, bg, error)) return FALSE;
	if (subtype & HEXTILE_ANYSUB) {
	  guint8 nsub;
	  READU8(&nsub);
	  g_debug("   nsub=%u", nsub);
	  if (subtype & HEXTILE_COLOUR) {
	    guint i;
	    for (i=0; i < nsub; i++) {
	      GUINTN colour;
	      guint8 xy, wh;
	      guint x, y, w, h;
	      SYMN(READU)(&colour);
	      READU8(&xy);
	      READU8(&wh);
	      x = xy >> 4;  y = xy & 15;
	      w = (wh >> 4) + 1;  h = (wh & 15) + 1;
	      if (! _fill_rect(self, x0+x,y0+y, w,h, colour, error)) return FALSE;
	    }
	  } else {
	    guint i;
	    for (i=0; i < nsub; i++) {
	      guint8 xy, wh;
	      guint x, y, w, h;
	      READU8(&xy);
	      READU8(&wh);
	      x = xy >> 4;  y = xy & 15;
	      w = (wh >> 4) + 1;  h = (wh & 15) + 1;
	      if (! _fill_rect(self, x0+x,y0+y, w,h, bg, error)) return FALSE;
	    }
	  }
	}
      }
    }
  }
  return TRUE;
}

#undef GUINTN
