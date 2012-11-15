#=======================================================================
#       Test the client implementation
#=======================================================================
import logging
import optparse
from ptvnc.client import VncClient

op = optparse.OptionParser(usage='%prog [options] host [dspno]')
op.add_option('-d','--debug',
              action='store_true', default=False,
              help='Enable debug tracing')
op.add_option('-s','--shared',
              action='store_true', default=False,
              help='Allow shared display')
op.add_option('-f','--format',
              help='Set pixel format e.g. rgb222')
op.add_option('-r','--region',
              help='Set region to update')
opts, args = op.parse_args()
if not 1 <= len(args) <= 2:
    op.error('Wrong number of arguments')
host = args[0]
if len(args) > 1:
    port = 5900 + int(args[1])
else:
    port = 5900
if opts.region is not None:
    opts.region = map(int, opts.region.split('x'))
level = (logging.DEBUG if opts.debug else
         logging.INFO)
logging.basicConfig(level=level)
vnc = VncClient(host, port, shared=opts.shared,
                format=opts.format, region=opts.region)
vnc.run()
