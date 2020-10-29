import os
from os.path import expanduser

os.chdir(expanduser("~"))
for dirpath, dirnames, filenames in os.walk('trial'):
  for filename in filenames:
    os.remove(os.path.join(dirpath, filename))
  for dirname in dirnames:
    os.removedirs(os.path.join(dirpath, dirname))
    os.rmdir('trial')