#!/usr/bin/env python
#=======================================================================
#	Doodles with PIL & Tk
#=======================================================================
import Tkinter
from Tkinter import *
import PIL.Image
import ImageTk

class Display(Frame):
    '''A Tk widget which displays a PIL Image'''
    def __init__(self, *args, **kw):
	Frame.__init__(self, *args, **kw)
	self.photo = None
	self.label = Label(self)
	self.label.pack()

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

#a = App()
a = Tk()
d = Display(a)
d.pack()

#slate = PIL.Image.new('RGB', (200,150))
#sprite = PIL.Image.new('RGB', (50,50), color=(255,128,0))
#d.display(slate)

def splodge():
    slate.paste(sprite, (20,20,70,70))
    d.show(slate)

def splurge():
    slate.paste((50,200,20), (10,30,110,40))
    d.show(slate)

b = Buffer((200,150))
d.show(b.image)

#a.after(1000, splodge)
#a.after(2000, splurge)
red = (255,0,0)
green = (0,255,0)

a.after(1000, b.fill_rect, red, (0,0),(50,50), d)
a.after(2000, b.fill_rect, green, (20,20),(70,50), d)
a.after(3000, b.copy_rect, (30,0), (130,10), (20,80), d)

a.mainloop()
