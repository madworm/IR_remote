
Most of this code and IR code data gravitates around a little remote
control and matching demodulating receiver, which are currently
available at Adafruit.

https://www.adafruit.com/products/157 (IR receiver)
https://www.adafruit.com/products/389 (IR remote, nice and small)

As it turns out, the TSOP38238 is much better suited for the IR remote
from Adafruit. Range and detection reliability is greatly improved.

The remote uses the extended NEC format, which is very well explained 
on this site:

http://www.sbprojects.com/knowledge/ir/nec.php

