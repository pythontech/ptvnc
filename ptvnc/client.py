#=======================================================================
#	VNC
#=======================================================================
import logging
import socket
import struct
import re

from ptvnc.const import client_msg, server_msg, encoding, security
from ptvnc.pyDes import des

_log = logging.getLogger('vnc')

class VncError(Exception): pass

def bitrev(b):
    rev = 0
    for i in range(8):
        if b & (1 << i):
            rev |= 0x80 >> i
    return rev

class VncDES(des):
    def setKey(self, key):
        revkey = ''.join([chr(bitrev(ord(c))) for c in key])
        des.setKey(self, revkey)

def ignore(*args, **kw):
    '''Do-nothing function'''
    pass

class VncClient(object):
    def __init__(self, host, port=5900, shared=False, format=None, region=None, password=None, display=None):
	self.host = host
	self.port = port
	self.shared = shared
	if format not in (None,'rgb222','rgb565'):
	    raise ValueError, 'Invalid format %r' % format
	self.format = format
	self.region = region
        self.password = password
	if display is None:
	    import ptvnc.display
	    self.display = ptvnc.display.VncDisplay()
	else:
	    self.display = display

    def run(self):
	self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	self.sock.connect((self.host, self.port))
	self.handshake()
	self.security()
	self.initialisation()
	# Client may do SetPixelFormat here...
	if self.format == 'rgb222':
	    self.set_rgb222()
	elif self.format == 'rgb565':
	    self.set_rgb565()
	self.SetEncodings([encoding.RRE, encoding.Raw])
	self.poll()
	if self.region is not None:
	    self.FrameBufferUpdateRequest(0,0, *self.region)
	else:
	    self.request_all()
	for i in range(3):
	    self.poll()
	self.drain()

    def handshake(self):
	'''Client has connected.  Expect ProtocolVersion from server'''
	msg = self.read(12)
	if not re.match(r'RFB \d\d\d\.\d\d\d\n', msg):
	    raise VncError, 'Bad ProtolVersion message: %r' % msg
	version = int(msg[4:7]), int(msg[8:11])
	_log.info('version %r', version)
	if version < (3,3):
	    raise VncError, 'Server supports version %d.%d only' % version
	elif version >= (3,8):
	    self.version = (3,8)
	elif version >= (3,7):
	    self.version = (3,7)
	else:
	    self.version = (3,3)
	self.write('RFB %03d.%03d\n' % self.version)

    def security(self):
	'''Negotiate security'''
	if self.version >= (3,7):
	    nsec, = struct.unpack('B', self.sock.recv(1))
	    _log.debug('nsec=%d', nsec)
	    if nsec == 0:
		# Error
		reason = self.read_string()
		raise VncError('Server error in handshake: %r' % reason)
	    secs = [ord(c) for c in self.read(nsec)]
	    _log.debug('secs = %r',
		       ', '.join(['%d=%s' % (n, security.get_name(n))
				  for n in secs]))
            if security.VNC_Authentication in secs:
		self.security = security.VNC_Authentication
		self.write(chr(self.security))
                _log.info('VNC Auth')
                challenge = self.read(16)
                _log.debug('challenge %r', challenge)
                pw = self.password.ljust(8,'\0')[:8]
                response = VncDES(pw).encrypt(challenge)
                _log.debug('response %r', response)
                self.write(response)
            elif security.NONE:
		self.security = security.NONE
		self.write(chr(self.security))
            else:
		raise VncError, 'No supported security type available'
	    
	else:
	    # 3.3
	    self.security = self.read_u32()
	_log.debug('security = %d', self.security)
	if self.version >= (3,8):
	    # SecurityResult
	    sec_result = self.read_u32()
	    if sec_result == 0:
		pass
	    elif sec_result == 1:
		# Failed
		error = 'Server says security handshake failed'
		if self.version >= (3,8):
		    error += ': ' + self.read_string()
		    raise VncError, error
		else:
		    raise VncError, 'Unexpected SecurityResult %d' % sec_result

    def initialisation(self):
	# ClientInit
	self.write(chr(bool(self.shared)))
	# ServerInit
	head = self.read(20)
	self.width, self.height = struct.unpack('>H H', head[:4])
	_log.info('framebuffer %d x %d', self.width, self.height)
	self.bpp, self.depth, self.be, self.tru, rmax, gmax, bmax, rsh, gsh, bsh = \
		  struct.unpack('>B B B B H H H B B B 3x', head[4:])
	self.rgbmax = (rmax, gmax, bmax)
	self.shift = (rsh, gsh, bsh)
	self.calc_pixel()
	_log.info('bpp=%d depth=%d, tru=%d max=%r shift=%r',
		  self.bpp, self.depth, self.tru, self.rgbmax, self.shift)
	self.name = self.read_string()
	_log.info('name = %r', self.name)

    def SetPixelFormat(self, bpp,depth,be,tru,rgbmax,shift):
	_log.info('setting %dbpp', bpp)
	self.bpp = bpp
	self.depth = depth
	self.be = be
	self.tru = tru
	self.rgbmax = rgbmax
	self.shift = shift
	msg = struct.pack('B 3x B B B B HHH BBB 3x',
			  client_msg.SetPixelFormat,
			  self.bpp, self.depth, self.be, self.tru,
			  self.rgbmax[0], self.rgbmax[1], self.rgbmax[2],
			  self.shift[0], self.shift[1], self.shift[2])
	self.write(msg)
	self.calc_pixel()

    def set_rgb222(self):
	'''Request 2 bits per pixel as per vncviewer'''
	self.SetPixelFormat(bpp=8, depth=6, be=1, tru=1,
			    rgbmax=(3,3,3), shift=(0,2,4))

    def set_rgb565(self):
	'''Request 5,6,5 bits per pixel'''
	self.SetPixelFormat(bpp=16, depth=16, be=1, tru=1,
			    rgbmax=(31,63,31), shift=(0,5,11))

    def SetEncodings(self, encs):
	msg = struct.pack('>B x H %dI' % len(encs),
			  client_msg.SetEncodings, len(encs),
			  *encs)
	self.write(msg)

    def FrameBufferUpdateRequest(self, xpos,ypos, width,height, incremental=False):
	_log.debug('requesting %dx%d @ %d,%d', width,height, xpos,ypos)
	msg = struct.pack('>B B H H H H',
			  3,		# FramebufferUpdateRequest
			  incremental,
			  xpos, ypos,		# x- and y-position
			  width, height)
	self.write(msg)

    def request_all(self):
	'''Request update of entire screen'''
	self.FrameBufferUpdateRequest(0,0, self.width, self.height)

    def poll(self):
	'''Check for messages from server'''
	self.sock.settimeout(0.1)
	try:
	    ctype = self.read(1)
	except socket.timeout:
	    return
	finally:
	    self.sock.settimeout(None)
	mtype = ord(ctype)
	_log.debug('server message type %d=%s',
		   mtype, server_msg.get_name(mtype))
	if mtype == server_msg.FrameBufferUpdate:
	    self.FrameBufferUpdate()
	elif mtype == server_msg.SetColourMapEntries:
	    self.SetColourMapEntries()
	else:
	    raise VncError, 'Unhandled server message %d' % mtype

    def drain(self):
	count = 0
	data = []
	self.sock.settimeout(2)
	while True:
	    try:
		b = self.read(1)
	    except socket.timeout:
		break
	    count += 1
	    data.append(b)
	    #print '%02x' % ord(b),
	    if count % 8 == 0:
		#print
		pass
	self.sock.settimeout(None)
	if count:
	    print 'Drained %d bytes' % count
	    if all([c=='\0' for c in data]):
		print 'All zero'
	    open('drained','w').write(''.join(data))

    def SetColourMapEntries(self):
	'''Handle SetColourMapEntries from server.
	'''
	first, ncol = struct.unpack('>x H H', self.read(5))
	for i in range(ncol):
	    r,g,b = struct.unpack('>HHH', self.read(6))
	    _log.debug(' colour[%d] = %d,%d,%d' % (first+i, r,g,b))

    def FrameBufferUpdate(self):
	'''Handle FrameBufferUpdate from server.
	On entry, the message type (0) has been read from the socket.
	'''
	# FrameBufferUpdate
	nrect, = struct.unpack('>x H', self.read(3))
	_log.debug(' nrect = %d', nrect)
	for i in range(nrect):
	    crect = self.read(12)
	    xpos, ypos, width, height, enc \
		  = struct.unpack('>H H H H i', crect)
	    _log.debug(' [%d] pos=%r size=%r enc=%d=%s',
		       i, (xpos,ypos), (width,height),
		       enc, encoding.get_name(enc))
	    if enc == encoding.Raw:
		if xpos + width > self.width:
		    raise VncError, 'Invalid xpos %d width %d' % (xpos, width)
		if ypos + height > self.height:
		    raise VncError, 'Invalid ypos %d height %d' % (xpos, height)
		self.rectangle_Raw(xpos,ypos,width,height)
	    elif enc == encoding.RRE:
		self.rectangle_RRE(xpos,ypos,width,height)
	    else:
		_log.error('Unhandled encoding %d', enc)
	self.display.update_done()

    def rectangle_Raw(self, xpos,ypos, width,height):
	'''Handle a rectangle update in Raw encoding'''
	rowbytes = width * self.pixbytes
	_log.debug('  %d bytes', height * rowbytes)
	for dy in range(height):
	    row = self.read_pixels(width)
	    # FIXME render into buffer
	    _log.debug('  {%d}: %s', ypos+dy,
		       ' '.join(['%02x' % p for p in row[:20]]))
	    self.display.paint_row(xpos,ypos+dy, width, row)

    def rectangle_RRE(self, xpos,ypos, width,height):
	'''Handle an update rectangle in RRE encoding'''
	nsub = self.read_u32()
	bg = self.read_pixel()
	_log.debug('  nsub=%d bg=%#x', nsub, bg)
	self.display.fill_rect(xpos,ypos, width,height, bg)
	for i in range(nsub):
	    fg = self.read_pixel()
	    x,y,w,h = struct.unpack('>HHHH', self.read(8))
	    _log.debug('  {%d}: pos=%r size=%r fg=%#x',
		       i, (x,y), (w,h), fg)
	    self.display.fill_rect(xpos+x,ypos+y, w,h, fg)

    def calc_pixel(self):
	'''Cache info for reading pixel off the wire'''
	self.pixbytes = self.bpp / 8
	self.pixend = '>' if self.be else '<'
	self.pixfmt = {1:'B', 2:'H', 4:'I'}[self.pixbytes]

    def read_pixel(self):
	'''Read a single pixel value off the wire'''
	pix, = struct.unpack(self.pixend+self.pixfmt, self.read(self.pixbytes))
	return pix

    def read_pixels(self, count):
	'''Read a number of pixels off the wire'''
	format = '%s%d%s' % (self.pixend, count, self.pixfmt)
	return struct.unpack(format, self.read(count * self.pixbytes))

    def read_string(self):
	'''Read a string preceded by unsigned 32-bit length'''
	slen = self.read_u32()
	return self.read(slen)

    def read_u32(self):
	'''Read an unsigned 32-bit int'''
	c4 = self.read(4)
	i4, = struct.unpack('>I', c4)
	return i4

    def read(self, nbytes):
	'''Read a number of byted'''
	return self.sock.recv(nbytes)

    def write(self, msg):
	'''Write a number of bytes'''
	self.sock.send(msg)
