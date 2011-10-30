#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys

if len( sys.argv) <= 1:
   print('Error: No command specified.')
   sys.exit(1)

from PyQt4 import QtCore, QtGui

class Dialog( QtGui.QWidget):
   def __init__( self):
      QtGui.QWidget.__init__( self)

      self.setWindowTitle( sys.argv[0])

      layout = QtGui.QVBoxLayout( self)
      self.cmdField = QtGui.QLineEdit( self)
      self.cmdField.setReadOnly( True)
      layout.addWidget( self.cmdField)
      self.outputField = QtGui.QTextEdit( self)
      self.outputField.setReadOnly( True)
      layout.addWidget( self.outputField)

      command = QtCore.QString.fromUtf8( sys.argv[1])
      cmdtoshow = command
      arguments = []
      for arg in sys.argv[2:]:
         arguments.append( QtCore.QString.fromUtf8( arg))
         cmdtoshow += ' ' + arg

      self.cmdField.setText( cmdtoshow)

      self.process = QtCore.QProcess( self)
      self.process.setProcessChannelMode( QtCore.QProcess.MergedChannels)
      QtCore.QObject.connect( self.process, QtCore.SIGNAL('finished( int)'), self.processfinished)
      QtCore.QObject.connect( self.process, QtCore.SIGNAL('readyRead()'), self.processoutput)
      self.process.start( command, arguments)

   def closeEvent( self, event):
      self.process.terminate()
      if sys.platform.find('win') == 0:
         self.process.kill()
      self.outputField.setText('Stopped.')
      self.process.waitForFinished()

   def processfinished( self, exitCode):
      print('Exit code = %d' % exitCode)
      if exitCode == 0: self.outputField.insertPlainText('Command finished successfully.')

   def processoutput( self):
      output = self.process.readAll()
      print('%s' % output),
      self.outputField.insertPlainText( QtCore.QString( output))
      self.outputField.moveCursor( QtGui.QTextCursor.End)

app = QtGui.QApplication( sys.argv)
dialog = Dialog()
dialog.show()
app.exec_()
