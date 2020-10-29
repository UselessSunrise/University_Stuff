# -*- coding: utf-8 -*-

import tarfile
import os
from os.path import expanduser
import time

tar = tarfile.open("files.tar")
os.chdir(expanduser("~"))
tar.extractall("./trial")
tar.close()

if not os.path.isdir('.trial_limits'):
  os.mkdir('.trial_limits')
  install_time = open('.trial_limits/install_time', 'w')
  install_time.write(str(time.time()))
  install_time.close()
  times_used = open('.trial_limits/times_used', 'w')
  times_used.write('0')
  times_used.close()
else:
  print("Previous installation detected.")
