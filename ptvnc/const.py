#=======================================================================
#       Constants
#=======================================================================

class Enum:
    def __init__(self, **kw):
        self._names = {}
        for n,v in kw.items():
            setattr(self, n, v)
            self._names[v] = n

    def get_name(self, value):
        return self._names.get(value) or '*unknown(%s)*' % value

encoding = Enum(
    Raw = 0,
    CopyRect = 1,
    RRE = 2,
    Hextile = 5,
    ZRLE = 16,
    Cursor = -239,
    DesktopSize = -223,
    )

client_msg = Enum(
    SetPixelFormat = 0,
    SetEncodings = 2,
    FramebufferUpdateRequest = 3,
    KeyEvent = 4,
    PointerEvent = 5,
    ClientCutText = 6,
    )

server_msg = Enum(
    FrameBufferUpdate = 0,
    SetColourMapEntries = 1,
    Bell = 2,
    ServerCutText = 3,
    )

security = Enum(
    Invalid = 0,
    NONE = 1,
    VNC_Authentication = 2,
    RA2 = 5,
    RA2ne = 5,
    Tight = 16,
    Ultra = 17,
    TLS = 18,
    VeNCrypt = 19,
    GTK_VNC_SASL = 20,
    MD5_hash = 21,
    Colin_Dean_xvp = 22,
    )
