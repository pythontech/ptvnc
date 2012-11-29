/*=======================================================================
 *	Constants for VNC
 *=======================================================================*/

enum {
  ENCODING_Raw = 0,
  ENCODING_CopyRect = 1,
  ENCODING_RRE = 2,
  ENCODING_Hextile = 5,
  ENCODING_ZRLE = 16,
  ENCODING_Cursor = -239,
  ENCODING_DesktopSize = -223,
};

enum {
  CLIENT_MSG_SetPixelFormat = 0,
  CLIENT_MSG_SetEncodings = 2,
  CLIENT_MSG_FramebufferUpdateRequest = 3,
  CLIENT_MSG_KeyEvent = 4,
  CLIENT_MSG_PointerEvent = 5,
  CLIENT_MSG_ClientCutText = 6,
};

enum {
  SERVER_MSG_FramebufferUpdate = 0,
  SERVER_MSG_SetColourMapEntries = 1,
  SERVER_MSG_Bell = 2,
  SERVER_MSG_ServerCutText = 3,
};

enum {
  SECURITY_Invalid = 0,
  SECURITY_NONE = 1,
  SECURITY_VNC_Authentication = 2,
  SECURITY_RA2 = 5,
  SECURITY_RA2ne = 5,
  SECURITY_Tight = 16,
  SECURITY_Ultra = 17,
  SECURITY_TLS = 18,
  SECURITY_VeNCrypt = 19,
  SECURITY_GTK_VNC_SASL = 20,
  SECURITY_MD5_hash = 21,
  SECURITY_Colin_Dean_xvp = 22,
};
