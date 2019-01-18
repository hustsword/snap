#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (c) IBM
# All rights reserved.

# Authors: Gou Peng Fei
# Date:
#
import string
import random

f = open("packet.rand.txt", "w")
len = 1024

for i in range(50000):
    str = ''.join(random.choice(string.ascii_uppercase + string.ascii_lowercase +
                                string.digits) for _ in range(len))
    f.write(str+"\n")

