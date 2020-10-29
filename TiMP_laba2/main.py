# -*- coding: utf-8 -*-

import time
import sys
import os
from os.path import expanduser

TIME_LIMITED = 0 # 1 - time limited version, 0 - usages limited version
TIME_LIMIT = 240 # time limit
USES_LIMIT = 4 # limit of usages

reached = 0
if TIME_LIMITED:
  flimit = open(os.path.join(expanduser("~"), '.trial_limits/install_time'))
  install_time = float(flimit.read())
  now_time = time.time()
  if (now_time - install_time) > TIME_LIMIT:
    print("Your trial version is over. You can purchase full version on our website.")
    sys.exit()
  print("Time left to use this program: " + str(TIME_LIMIT - now_time + install_time))
  flimit.close()
else:
  flimit = open(os.path.join(expanduser("~"), '.trial_limits/times_used'))
  times_used = float(flimit.read())
  if times_used >= USES_LIMIT:
    print("Your trial version is over. You can purchase full version on our website.")
    sys.exit()
  print("Usages left for this program: ", str(USES_LIMIT - times_used - 1))
  flimit.close()
  flimit = open(os.path.join(expanduser("~"), '.trial_limits/times_used'), 'w')
  flimit.write(str(times_used + 1))
  flimit.close()

f = open('hist', 'r+')
hist = [line for line in f]
name = input("Enter your name: ")
try:
  hist.index(name + '\n')
except ValueError:
  f.write(name + '\n')
else:
  print("You have used this program before.")
