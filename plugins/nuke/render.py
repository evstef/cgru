import os
import optparse
import shutil
import signal
import sys
import tempfile
import re

import nuke

from afpathmap import PathMap

# Interrupt function to delete temp directory:
def interrupt( signum, frame):
   shutil.rmtree( tmpdir)
   exit('\nInterrupt received...')

# Set interrupt function:
signal.signal( signal.SIGTERM, interrupt)
signal.signal( signal.SIGABRT, interrupt)
if sys.platform.find('win') != 0: signal.signal( signal.SIGQUIT, interrupt)
signal.signal( signal.SIGINT,  interrupt)

# Error function to print message and delete temp directory:
def errorExit( msg, deletetemp):
   print msg
   if deletetemp:
      print 'Removing temp directory:'
      print tmpdir
      shutil.rmtree( tmpdir)
   exit(1)

# Parse argumets:
parser = optparse.OptionParser( usage="usage: %prog [options] (like nuke --help)", version="%prog 1.0")
parser.add_option('-x', '--xscene', dest='xscene', type='string', default='', help='Path to scene to execute')
parser.add_option('-X', '--xnode',  dest='xnode',  type='string', default='', help='The name of node to execute')
parser.add_option('-F', '--frange', dest='frange', type='string', default='', help='Frame range to render (Nuke syntax)')
(options, args) = parser.parse_args()
xscene = options.xscene
xnode  = options.xnode
srange = options.frange

# Check arguments:
if xscene == '': errorExit('Scene to execute is not specified.', False)
if xnode == '': errorExit('Node to execute is not specified.', False)
if srange == '': errorExit('Frame range is not specified.', False)

# Check frame range:
frange = re.findall( r'[0-9]{1,}', srange)
if frange is None: errorExit('No numbers in frame range founded', False)
if len(frange) == 2: frange.append('1')
if len(frange) != 3: errorExit('Invalid frame range specified, type A-BxC - [from]-[to]x[by]', False)
try:
   ffirst = int(frange[0])
   flast  = int(frange[1])
   fby    = int(frange[2])
except:
   errorExit( str(sys.exc_info()[1]) + '\nInvalid frame range syntax, type A-BxC - [from]-[to]x[by]', False)

# Check for negative numbers:
pos = srange.find(frange[0])
if pos > 0:
   if srange[pos-1] == '-': ffirst = -ffirst
srange = srange[pos+len(frange[0]):]
pos = srange.find(frange[1])
if pos > 1:
   if srange[pos-2:pos] == '--': flast = -flast

# Check first and last frame values:
if flast < ffirst: errorExit('First frame (%(ffirst)d) must be grater or equal last frame (%(flast)d)' % vars(), False)
if fby < 1: errorExit('By frame (%(fby)d) must be grater or equal 1' % vars(), False)

# Check scene file for existance:
if not os.path.isfile( xscene): errorExit('File "%s" not founded.' % xscene, False)

# Get Afanasy root directory:
afroot = os.getenv('AF_ROOT')
if afroot is None: errorExit('AF_ROOT is not defined.', True)

# Create and check temp directory:
tmpdir = tempfile.mkdtemp('.afrender.nuke')
if os.path.exists( tmpdir): print 'Temp directory = "%s"' % tmpdir
else: errorExit('Error creating temp directory.', False)

# Transfer scene pathes
pm = PathMap( afroot, UnixSeparators = True, Verbose = True)
if pm.initialized:
   pmscene = os.path.basename(xscene)
   pmscene = os.path.join( tmpdir, pmscene)
   pm.toServerFile( xscene, pmscene, Verbose = False)
   xscene = pmscene

# Try to open scene:
try: nuke.scriptOpen( xscene)
except: errorExit('Scene open error:\n' + str(sys.exc_info()[1]), True)

# Try to process write node:
writenode = nuke.toNode( xnode)
if writenode is None: errorExit('Node "%s" not founded.' % xnode, True)
if writenode.Class() != 'Write': errorExit('Node "%s" is not "Write".' % xnode, True)
try:
   fileknob = writenode['file']
   filepath   = fileknob.value()
   # Nuke paths has only unix slashes, even on MS Windows platform
   if sys.platform.find('win') == 0:
      filepath = filepath.replace('/','\\')
   imagesdir  = os.path.dirname(  filepath)
   imagesname = os.path.basename( filepath)
   tmppath    = os.path.join( tmpdir, imagesname)
   # Nuke paths has only unix slashes, even on MS Windows platform
   if sys.platform.find('win') == 0:
      tmppath = tmppath.replace('\\','/')
   fileknob.setValue( tmppath)
except:
   errorExit('Write node file operations error:\n' + str(sys.exc_info()[1]), True)

# Render frames cycle:
exitcode = 0
frame = ffirst
while frame <= flast:
   print 'Rendering frame %d:' % frame
   sys.stdout.flush()

   # Try to execute write node:
   try:
      nuke.execute( xnode, frame, frame)
   except:
      print 'Node execution error:'
      print str(sys.exc_info()[1])
      exitcode = 1
   print
   sys.stdout.flush()

   # Copy image files from temp directory:
   allitems = os.listdir( tmpdir)
   for item in allitems:
      if item.rfind('.tmp') == len(item)-4: continue
      if item.rfind('.nk') == len(item)-3: continue
      src  = os.path.join( tmpdir,    item)
      dest = os.path.join( imagesdir, item)
      if os.path.isfile( dest):
         try:
            print 'Deleting old "%s"' % dest
            os.remove( dest)
         except:
            print str(sys.exc_info()[1])
            print 'Unable to remove destination file:'
            print dest
            exitcode = 1
      try:      
         print 'Moving "%s"' % dest
         shutil.move( src, imagesdir)
      except:
         print str(sys.exc_info()[1])
         print 'Unable to move file:'
         print src
         print dest
         exitcode = 1

   sys.stdout.flush()
   if exitcode != 0: break
   frame += fby

# Remove temp directory:
shutil.rmtree( tmpdir)

# Exit:
exit( exitcode)
