# Bits in subencoding type
HEXTILE_RAW = 1
HEXTILE_BG = 2
HEXTILE_FG = 4
HEXTILE_ANYSUB = 8
HEXTILE_COLOUR = 16

def nybbles1(byte):
    x = ((byte >> 4) & 15) + 1
    y = (byte & 15) + 1
    return x, y

class Client:
    def rectangle_Hextile(self, xpos,ypos, width,height):
	rows = (width+15)/16
	cols = (height+15)/16
	bg = fg = None
	for row in range(rows):
	    y0 = ypos + 16*row
	    y1 = min(y0+16, ypos+height)
	    for col in range(cols):
		x0 = xpos + 16*col
		x1 = min(x0+16, xpos+width)
		subtype = ord(self.read(1))
		if subtype & HEXTILE_RAW:
		    for y in range(y0, y1):
			row = self.read((x1-x0) * self.pixbytes)
			# FIXME paint
		else:
		    if subtype & HEXTILE_BG:
			bg = self.read_pixel()
		    if subtype & HEXTILE_FG:
			fg = self.read_pixel()
		    self.fill(x0,y0, x1-x0,y1-y0, bg)
		    if subtype & HEXTILE_ANYSUB:
			nsub = ord(self.read(1))
			if subtype & HEXTILE_COLOUR:
			    for i in range(nsub):
				colour = self.read_pixel()
				xy, wh = struct.unpack('>BB', self.read(2))
				x,y = nybbles1(xy)
				w,h = nybbles1(wh)
				self.fill(x0+x,y0+y, w,h, colour)
			else:
			    for i in range(nsub):
				xy, wh = struct.unpack('>BB', self.read(2))
				x,y = nybbles1(xy)
				w,h = nybbles1(wh)
				self.fill(x0+x,y0+y, w,h, fg)
