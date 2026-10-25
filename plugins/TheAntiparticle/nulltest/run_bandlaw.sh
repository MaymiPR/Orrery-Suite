#!/bin/sh
# every band and amount node, single and dual detector (the better one is kept); high band also with a peak detector
PY="${PY:-python}"     # PY=/path/to/python to pick one
for band in ${BANDS:-mid low high}; do
  for node in ${NODES:-4 5 6 7 8 3 2 1}; do
    "$PY" fit_bandlaw.py $band $node
    "$PY" fit_bandlaw.py $band $node --dual
    if [ $band = high ]; then "$PY" fit_bandlaw.py $band $node --peak; "$PY" fit_bandlaw.py $band $node --peak --dual; fi
  done
done
