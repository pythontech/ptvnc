#!/usr/bin/env python
#=======================================================================
#	Doodles with PIL & Tk
#=======================================================================
import Tkinter
from Tkinter import *
import PIL.Image
import ImageTk
from ptvnc.client import VncClient
from ptvnc.display import VncDisplay

class Display(Frame, VncDisplay):
    '''A Tk widget which displays a PIL Image'''
    def __init__(self, master, size, *args, **kw):
	Frame.__init__(self, master, *args, **kw)
	self.photo = None
	self.label = Label(self)
	self.label.pack()
	self.image = PIL.Image.new('RGB', size)

    def fill_rect(self, x,y, w,h, colour):
	rgb = ((colour & 0x001f) << 3,
	       (colour & 0x07e0) >> 3,
	       (colour & 0xf800) >> 8)
	self.image.paste(rgb, (x, y, x+w, y+h))

    def update_done(self):
	self.show(self.image)

    def show(self, image):
	self.photo = ImageTk.PhotoImage(image)
	self.label.configure(image=self.photo)

class Buffer:
    '''Image buffer'''
    def __init__(self, size):
	self.size = size
	self.image = PIL.Image.new('RGB', size)

    def fill_rect(self, colour, (x,y), (w,h), display=None):
	self.image.paste(colour, (x,y,x+w,y+h))
	if display:
	    display.show(self.image)

    def copy_rect(self, (x,y), dest, (w,h), display=None):
	sbox = (x, y, x+w, y+h)
	piece = self.image.crop(sbox)
	self.image.paste(piece, dest)
	if display:
	    display.show(self.image)

class App(Tk):
    def __init__(self):
	Tk.__init__(self)
	#self.image = PhotoImage(file='/home/chah/www/ccfe.gif')
	slate = PIL.Image.new('RGB', (200,150))
	sprite = PIL.Image.new('RGB', (50,50), color=(255,128,0))
	slate.paste(sprite, (20,20,70,70))
	self.photo = ImageTk.PhotoImage(image=slate)
	self.lab = Label(self, image=self.photo, width=200, height=150)
	self.lab.pack()

if __name__=='__main__':
    import logging
    logging.basicConfig(level=logging.DEBUG)
    app = Tk()
    disp = Display(app, (800,600))
    disp.pack()
    cl = VncClient('jtv.jet.efda.org',5925,format='rgb565',display=disp)
    cl.run()
    app.mainloop()

if 0:
    #a = App()
    a = Tk()
    d = Display(a)
    d.pack()

    # slate = PIL.Image.new('RGB', (200,150))
    # sprite = PIL.Image.new('RGB', (50,50), color=(255,128,0))
    # d.display(slate)

    def splodge():
	slate.paste(sprite, (20,20,70,70))
	d.show(slate)

    def splurge():
	slate.paste((50,200,20), (10,30,110,40))
	d.show(slate)

    b = Buffer((200,150))
    d.show(b.image)

    # a.after(1000, splodge)
    # a.after(2000, splurge)
    red = (255,0,0)
    green = (0,255,0)

    a.after(1000, b.fill_rect, red, (0,0),(50,50), d)
    a.after(2000, b.fill_rect, green, (20,20),(70,50), d)
    a.after(3000, b.copy_rect, (30,0), (130,10), (20,80), d)

    a.mainloop()
