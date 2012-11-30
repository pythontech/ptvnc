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

#undef GUINTN
