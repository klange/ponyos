#!/usr/bin/python3
"""
'About ToaruOS' applet
"""
import os
import sys

import yutani
import yutani_mainloop

from about_applet import AboutAppletWindow

def version():
    """Get a release from uname without a git short sha."""
    release = os.uname().release
    if '-' in release:
        return release[:release.index('-')]
    return release

_default_text = f"<b>PonyOS {version()}</b>\n© 2011-2018 K Lange, et al.\n\n<color 0x0000FF>http://ponyos.org/</color>"


if __name__ == '__main__':
    yutani.Yutani()
    d = yutani.Decor()

    def quit():
        sys.exit(0)

    if len(sys.argv) > 4:
        window = AboutAppletWindow(d,sys.argv[1],sys.argv[3],sys.argv[4],sys.argv[2],on_close=quit)
    else:
        window = AboutAppletWindow(d,"About PonyOS",'/usr/share/logo_small.png',_default_text,on_close=quit)

    yutani_mainloop.mainloop()

