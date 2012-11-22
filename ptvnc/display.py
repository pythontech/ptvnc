#=======================================================================
#	Base class for display
#=======================================================================
class VncDisplay(object):
    '''Base class for VNC display.
    Stub methods, apart from paint_row which has a default implemenbation.
    '''
    def set_size(self, width, depth):
	pass

    def fill_rect(self, xpos,ypos, width,height, pixel):
	pass

    def paint_row(self, xpos,ypos, width, pixels):
	'''Noddy implementation by filling lots of 1x1 rectangles'''
	for dx in range(width):
	    self.fill_rect(xpos+dx,ypos, 1,1, pixels[dx])

    def update_done(self):
	pass
