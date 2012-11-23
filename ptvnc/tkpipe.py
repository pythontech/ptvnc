from Tkinter import *
import threading
import os
import time

class App:
    def __init__(self, tk):
	self.tk = tk
	self.label = Label(self.tk, text='--start--')
	self.label.pack()

    def tcl_end(self, fd):
	'''Build the Tcl end of the pipe'''
	fdname = '/dev/fd/%d' % fd
	self.pipe = self.tk.call('open', fdname, 'r')
	print 'pipe = %s' % self.pipe
	self.tk.call('fconfigure', self.pipe,
		     '-encoding', 'binary',
		     '-blocking', 0)
	tcl_cb = self.tk.register(self.pipe_readable)
	self.tk.call('fileevent', self.pipe, 'readable', tcl_cb)

    def pipe_readable(self):
	print 'pipe_readable'
	eof = int(self.tk.call('eof', self.pipe))
	print 'eof %s' % eof
	if eof:
	    self.tk.call('close', self.pipe)
	else:
	    bytes = self.tk.call('read', self.pipe, 42)
	    print 'from pipe: %r' % bytes
	    self.label.configure(text=repr(bytes))

class Generator(threading.Thread):
    def __init__(self, fd):
	threading.Thread.__init__(self)
	self.daemon = True
	self.fd = fd

    def run(self):
	try:
	    file = os.fdopen(self.fd, 'w')
	    for i in range(10):
		file.write('%d' % i)
		file.flush()
		time.sleep(1)
	except Exception, e:
	    print 'Generator: %s' % e
	    import traceback
	    traceback.print_exc()

class TkPipe(object):
    '''
    Allow notifying an event to the Tk mainloop.
    '''
    def __init__(self, tk, handler):
	self.tk = tk
	self.handler = handler
	# Create a Tcl file channel.
	# Its name should be "file$n" where $n is the fd
	self.read_chan = tk.call('open', os.devnull, 'r')
	assert(str(self.read_chan).startswith('file'))
	fd = int(str(self.read_chan)[4:])
	# Close the fd under Tcl's feet and replace with the read end of a pipe
	os.close(fd)
	rd, wr = os.pipe()
	# If fd was the first available descriptor when chan was opened,
	# rd should get the same number; but just in case...
	assert(rd == fd)
	# We could fix it up thus (unless wr==fd ...)
	#  if rd != fd:
	#      os.dup2(rd, fd)
	#      os.close(rd)
	# Now make a Python file around the write-end of the pipe,
	# with no buffering
	self.write_file = os.fdopen(wr, 'w', 0)
	# Now set up the Tcl handler
	tk.call('fconfigure', self.read_chan,
		'-encoding', 'binary',
		'-blocking', 0)
	self.tcl_handler = tk.register(self._pipe_readable)
	tk.call('fileevent', self.read_chan, 'readable', self.tcl_handler)
	#tk.call('fileevent', self.read_chan, 'readable', "puts foo")

    def _pipe_readable(self):
	'''Called in Tcl thread when pipe is readable'''
	try:
	    # Check if the pipe has closed
	    eof = int(self.tk.call('eof', self.read_chan))
	    if eof:
		self.tk.call('close', self.read_chan)
	    else:
		# Drain possibly multiple event bytes from the pipe
		data = self.tk.call('read', self.read_chan)
		#print 'read %r from pipe' % data
		# Call the handler
		self.handler(data)
	except Exception, e:
	    print e

    def send(self, data):
	# Write a byte to the pipe.
	# This should set the read end readable and wake up the Tcl event loop
	#os.write(self.write_fd, '\0')
	self.write_file.write(data)

    def set(self):
	self.send('\0')

if __name__=='__main__':
    tk = Tk()
    lab = Label(tk, text='-start-')
    lab.pack()
    text = ''
    def update(data):
	print 'update %s' % text
	lab.configure(text=text)
    pipe = TkPipe(tk, update)
    def gen():
	global text
	try:
	    for i in range(10):
		text = str(i)
		print 'sig.set'
		pipe.set()
		if i%3==2:
		    pipe.send(str(i))
		time.sleep(1)
	except Exception, e:
	    print e
    t = threading.Thread(target=gen)
    t.start()
    tk.after(1500, lambda: lab.configure(text='1.5'))
    tk.mainloop()
if 0:
    tk = Tk()
    app = App(tk)
    rd, wr = os.pipe()
    print 'pipe r=%s w=%s' % (rd,wr)
    app.tcl_end(rd)
    gen = Generator(wr)
    gen.start()
    tk.mainloop()
