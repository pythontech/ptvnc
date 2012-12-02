static gboolean _readf(void *self, const char *fmt, ...)
{
  unsigned char c;
  va_list ap;
  va_start(ap, fmt);
  while ((c = *fmt++) != '\0') {
    if (c == ' ') {
    } else if (c == 'B') {
      guint8 b;
      _read(self, &b, 1);
      *va_arg(ap, guint8 *) = b;
    } else if (c == 'H') {
      guint16 h;
      _read(self, &h, 2);
      *va_arg(ap, guint16 *) = htons(h);
    } else if (c == 'I') {
      guint32 i;
      _read(self, &i, 4);
      *va_arg(ap, guint32 *) = htonl(l);
    } else if (c == 'i') {
      gint32 i;
      _read(self, &i, 4);
      *va_arg(ap, gint32 *) = htonl(l);
    } else {
      ERROR1("Unknown format char %c", c);
    }
  }
  va_end(ap);
  return TRUE;
}

gboolean _writef(void *self, const char *fmt, ...)
{
  unsigned char c;
  va_list ap;
  va_start(ap, fmt);
  while ((c = *fmt++) != '\0') {
    if (c == ' ') {
    } else if (c == 'B') {
      guint8 b = *va_arg(ap, const guint8 *);
      _write(self, &b, 1);
    } else if (c == 'H') {
      guint16 h = *va_arg(ap, guint16 *);
      guint16 nh = htons(h);
      _write(self, &nh, 2);
    } else if (c == 'I') {
      guint32 i = *va_arg(ap, guint32 *);
      guint32 i = htonl(i);
      _write(self, &ni, 4);
    } else if (c == 'i') {
      gint32 i = *va_arg(ap, gint32 *);
      gint32 ni = htonl(i);
      _write(self, &ni, 4);
      
    } else {
      ERROR1("Unknown format char %c", c);
    }
  }
  va_end(ap);
}
