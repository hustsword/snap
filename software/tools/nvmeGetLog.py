#!/usr/bin/python
#----------------------------------------------------------------------------
#----------------------------------------------------------------------------
#--
#-- Copyright 2016,2017 International Business Machines
#--
#-- Licensed under the Apache License, Version 2.0 (the "License");
#-- you may not use this file except in compliance with the License.
#-- You may obtain a copy of the License at
#--
#--     http://www.apache.org/licenses/LICENSE-2.0
#--
#-- Unless required by applicable law or agreed to in writing, software
#-- distributed under the License is distributed on an "AS IS" BASIS,
#-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#-- See the License for the specific language governing permissions AND
#-- limitations under the License.
#--
#----------------------------------------------------------------------------
#----------------------------------------------------------------------------


from __future__ import print_function
import os
import sys
import subprocess
import time
import inspect
# import argparse
#trace = True
from optparse import OptionParser

trace = False

SSD0      = 0
SSD1      = 1
SSD0_USED = False
SSD1_USED = False
CARD      = "-C0"
CARD_TXT  = "Card 0"

class  AFU_MMIO:

    PROG_REG =0x98   # scratch register to store  init progress

    @staticmethod
    def init():
        tmp  = os.path.dirname(os.path.abspath(inspect.stack()[0][1]))
        AFU_MMIO.snap_peek = tmp + '/snap_peek'
        AFU_MMIO.snap_poke = tmp + '/snap_poke'
        parser = OptionParser()
        parser.add_option('-c', '--card', type = str, default = '0', help ="card:       select 0 or 1 (default 0)" )
        parser.add_option('-d', '--drive', type = str, default = '0', help ="NVMe drive: select 0,1,b  (default 0)" )

        (args,dummy) = parser.parse_args()
        #parser = argparse.ArgumentParser(description='test')
        #parser.add_argument('-C', type = str, default = '0' )
        #parser.add_argument('-D', type = str, default = '1' )
        global SSD0_USED
        global SSD1_USED
        #args = parser.parse_args()
        if (args.drive == '0') : SSD0_USED = True
        if (args.drive == '1') : SSD1_USED = True
        if (args.drive == 'b') :
            SSD0_USED = True
            SSD1_USED = True
        global CARD
        global CARD_TXT
        if (args.card == '1'):
            CARD     = "-C1"
            CARD_TXT = "Card 1"


    @staticmethod
    def write(addr, data):
        if trace :
            print ('w', end ='')
            sys.stdout.flush()
        p = subprocess.Popen ([AFU_MMIO.snap_poke, "-w32", CARD, str(addr), str(data)],stdout=subprocess.PIPE,)
        p.wait()
        return


    @staticmethod
    def write64(addr, data):
        if trace :
            print ('w', end ='')
            sys.stdout.flush()
        p = subprocess.Popen ([AFU_MMIO.snap_poke, CARD, str(addr), str(data)],stdout=subprocess.PIPE,)
        p.wait()
        return


    @staticmethod
    def read(addr):
        if trace:
            print ('r',end ='')
            sys.stdout.flush()
        p = subprocess.Popen ([AFU_MMIO.snap_peek, CARD, "-w32", str(addr),],stdout=subprocess.PIPE,)
        p.wait()

        txt = p.communicate()[0]
        txt = txt.split(']',1)
        txt = txt[1].split()
	#print ('txt=', txt)

        return int(txt[0],16)


    @staticmethod
    def read64(addr):
        if trace:
            print ('r',end ='')
            sys.stdout.flush()
        p = subprocess.Popen ([AFU_MMIO.snap_peek, CARD, str(addr),],stdout=subprocess.PIPE,)
        p.wait()

        txt = p.communicate()[0]
        txt = txt.split(']',1)
        txt = txt[1].split()
        return int(txt[0],16)


    @staticmethod
    def nvme_write(addr, data):
        if (addr >= 0x30000) :
            AFU_MMIO.write(0x30000, addr)
            AFU_MMIO.write(0x30004, data)
        else :
            AFU_MMIO.write(0x20000 + addr, data)


    @staticmethod
    def nvme_read(addr):
        if (addr >= 0x30000) :
            AFU_MMIO.write(0x30000, addr)
            return AFU_MMIO.read (0x30004)
        else:
            return AFU_MMIO.read (0x20000 + addr)


    @staticmethod
    def dump_buffer(drive, words):
        AFU_MMIO.nvme_write(0x88, 0x6f0)
        counter = 0
        while (words > 0):
            data = AFU_MMIO.nvme_read(0x100)
            print('buffer data word %d : %8x' % (counter, data))
            words   -= 1
	    counter += 1


    @staticmethod
    def nvme_fill_buffer(array):
        AFU_MMIO.nvme_write(0x80, 0x7)      # auto increment, Enable NVME Host, Clear Error status
        for data in array:
            AFU_MMIO.nvme_write(0x100,data)


class NVME_Drive:


    @staticmethod
    def get_AQ_PTR(drive):
       
        index = AFU_MMIO.nvme_read(0x94)
        if (drive == SSD0 ):
            tmp = (index & 0xf) * 0x40
        else:
            tmp = ((index & 0xf0000) >> 16) * 0x40 + 0x3780 
        print ('AQ Pointer for current Command is set to %4x' % tmp)  
        return tmp

    @staticmethod
    def read(drive, addr):
        if (drive == 1 ):
            addr += 0x2000
        AFU_MMIO.write(0x2008c, addr)
        #print (' write on 2008c: %4x' % addr)
        return AFU_MMIO.read (0x20104)


    @staticmethod
    def write(drive, addr, data):
        if (drive == 1 ):
            addr += 0x2000
        AFU_MMIO.write(0x2008c, addr)
        AFU_MMIO.write(0x20104, data)


    @staticmethod
    def wait_for_ready(drive):
        status = 0
        print ('Waiting for SSD%i drive to be ready' % drive)
        while (status == 0):
            status = NVME_Drive.read(drive,0x0000001c)
            print ('NVMe drive %x ready : %x ' % (drive, status))


    @staticmethod
    def wait_for_complete(drive):
        AFU_MMIO.nvme_write(0x80, 5) # clear error bit
        while (True):
            status = AFU_MMIO.nvme_read(0x84)
            if ((status & 0x2) > 0 ):
                print ("Error waiting for Admin Command to complete %x " % status)

                return status
            if (drive == SSD0):
                if ((status & 0x4) > 0 ): return status
            if (drive == SSD1):
                if ((status & 0x8) > 0 ): return status


    @staticmethod
    def create_IO_Queues(drive):
        print ('create IO CQ SSD%i' % drive)

        offset = NVME_Drive.get_AQ_PTR(drive)
        AFU_MMIO.nvme_write(0x88, offset)  # set TX buffer address
        if (drive == SSD0):
            array = [    0x5,0,0,0,0,0,0x38000000,0,0,0,0xd90001,1,0,0,0,0]
        else:
            array = [0x20005,0,0,0,0,0,0x48000000,0,0,0,0xd90001,1,0,0,0,0]
        AFU_MMIO.nvme_fill_buffer(array)
        # notify drive
        cmd = 0x02
        if (drive == 1) : cmd = 0x22
        AFU_MMIO.nvme_write(0x14, cmd)
        print ('waiting for Command to complete')
        status = NVME_Drive.wait_for_complete(drive)
        print ('completion code %x' % status)

        print ('create IO SQ SSD%i' % drive)
        offset = NVME_Drive.get_AQ_PTR(drive)
        AFU_MMIO.nvme_write(0x88,offset)  # set TX  buffer address
        if (drive == SSD0):
            array = [    0x1,0,0,0,0,0,0x10000000,0,0,0,0xd90001,0x10005,0,0,0,0]
        else:
            array = [0x20001,0,0,0,0,0,0x20000000,0,0,0,0xd90001,0x10005,0,0,0,0]
        AFU_MMIO.nvme_fill_buffer(array)
        # notify drive
        cmd = 0x02
        if (drive == SSD1) : cmd = 0x22
        AFU_MMIO.nvme_write(0x14, cmd)
        print ('waiting for Command to complete')
        status = NVME_Drive.wait_for_complete(drive)
        print ('completion code %x' % status)
        AFU_MMIO.nvme_write(AFU_MMIO.PROG_REG, AFU_MMIO.nvme_read(AFU_MMIO.PROG_REG) | 2 << (drive * 2))


    @staticmethod
    def send_identify(drive):
        print ('sending Identify command SSD%i' % drive)
        offset = NVME_Drive.get_AQ_PTR(drive)
        AFU_MMIO.nvme_write(0x88, offset)  # set TX buffer address
        if (drive == SSD0):
            array = [0x6,    0,0,0,0,0,0x50000000,0,0,0,1,0,0,0,0,0]
        else:
            array = [0x20006,0,0,0,0,0,0x50000000,0,0,0,1,0,0,0,0,0]
        AFU_MMIO.nvme_fill_buffer(array)

        # notify drive
        cmd = 0x02
        if (drive == SSD1) : cmd = 0x22
        print (" cmd = %x " % cmd)
        AFU_MMIO.nvme_write(0x14, cmd )
        print ('waiting for Identify to complete')
        status = NVME_Drive.wait_for_complete(drive)
        print ('completion code %x' % status)
        print ('Identify command completed')
	return


    @staticmethod
    def send_identify2(drive):
        print ('sending Namespace Identify command SSD%u' % drive)
        offset = NVME_Drive.get_AQ_PTR(drive)
        AFU_MMIO.nvme_write(0x88, offset)  # set buffer address
        if (drive == SSD0):
            array = [    0x6,1,0,0,0,0,0x50000000,0,0,0,0,0,0,0,0,0]
        else:
            array = [0x20006,1,0,0,0,0,0x50000000,0,0,0,0,0,0,0,0,0]
        AFU_MMIO.nvme_fill_buffer(array)
        # notify drive
        cmd = 0x02
        if (drive == SSD1) : cmd = 0x22

        AFU_MMIO.nvme_write(0x14, cmd)
        print ('waiting for Namespace Identify command to complete')
        status = NVME_Drive.wait_for_complete(drive)
        print ('completion code %x' % status)
        print ('Namespace Identify command completed')


    @staticmethod
    def get_log(drive):
        print ('get log SSD%i ' % drive)
        AFU_MMIO.nvme_write(0x80, 0x7)      # auto increment
        offset = NVME_Drive.get_AQ_PTR(drive)
        print ('setting buffer address to %8x' % offset)
        AFU_MMIO.nvme_write(0x88, offset)  # set buffer address
        if (drive == SSD0):
            array =     [0x2,0,0,0,0,0,0x50000000,0,0,0,1,2,0,0,0,0]
        else:
            array = [0x20002,0,0,0,0,0,0x50000000,0,0,0,0x00ff0001,0,0,0,0,0]
        AFU_MMIO.nvme_fill_buffer(array)
        # notify drive
        cmd = 0x02
        if (drive == SSD1) : cmd = 0x22
        AFU_MMIO.nvme_write(0x14, cmd)
        status = 0
        print ('waiting for command to complete')
        status = NVME_Drive.wait_for_complete(drive)
        print ('completion code %x' % status)
        print ('get feature command completed')


    @staticmethod
    def set_Features(drive):
        print ('set SSD%i Features' % drive)
        AFU_MMIO.nvme_write(0x80, 0x7)      # auto increment
        offset = NVME_Drive.get_AQ_PTR(drive)
        AFU_MMIO.nvme_write(0x88, offset)  # set buffer address
        if (drive == SSD0):
            array =     [0x9,0,0,0,0,0,0,0,0,0,1,2,0,0,0,0]
        else:
            array = [0x20009,0,0,0,0,0,0,0,0,0,1,2,0,0,0,0]
        AFU_MMIO.nvme_fill_buffer(array)
        # notify drive
        cmd = 0x02
        if (drive == SSD1) : cmd = 0x22
        AFU_MMIO.nvme_write(0x14, cmd)
        status = 0
        print ('waiting for command to complete')
        status = NVME_Drive.wait_for_complete(drive)
        print ('completion code %x' % status)
        print ('set feature command completed')


    @staticmethod
    def get_Features(drive):
        print ('get SSD%i Features' % drive)
        AFU_MMIO.nvme_write(0x80, 0x3)      # auto increment
        offset = NVME_Drive.get_AQ_PTR(drive)
        AFU_MMIO.nvme_write(0x88, offset)  # set buffer address
        if (drive == SSD0):
            array = [    0xa,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0]
        else:
            array = [0x2000a,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0]

        AFU_MMIO.nvme_fill_buffer(array)
        # notify drive
        cmd = 0x02
        if (drive == SSD1) : cmd = 0x22
        AFU_MMIO.nvme_write(0x14, cmd)
        print ('waiting for command to complete')
        status = NVME_Drive.wait_for_complete(drive)
        print ('completion code %x' % status)
        print ('get feature command completed')


    @staticmethod
    def dump_buffer(words):
        AFU_MMIO.nvme_write(0x88, 0x1bc0)
        AFU_MMIO.nvme_write(0x80, 7)
        counter = 0
        while (words > 0):
            data = AFU_MMIO.nvme_read(0x100)
            print('buffer data word %d : %8x' % (counter, data))
            words   -= 1
	    counter += 1


    @staticmethod
    def dump_buffer_addr(addr,words):
        AFU_MMIO.nvme_write(0x88, addr)
        AFU_MMIO.nvme_write(0x80, 7)
        counter = 0
        while (words > 0):
            data = AFU_MMIO.nvme_read(0x100)
            print('buffer data word %d : %8x' % (counter, data))
            words   -= 1
	    counter += 1


    @staticmethod
    def admin_ready(drive):
        data = AFU_MMIO.nvme_read(0)
        while (True):
            if (drive == SSD1):
                if ((data & 0x4) == 0): return
                print ('waiting ..')


    @staticmethod
    def RW_data(mem_low, mem_high, lba_low, lba_high, num, cmd):
        AFU_MMIO.nvme_write(0x00, mem_low)
        AFU_MMIO.nvme_write(0x04, mem_high)
        AFU_MMIO.nvme_write(0x08, lba_low)
        AFU_MMIO.nvme_write(0x0c, lba_high)
        AFU_MMIO.nvme_write(0x10, num)
        AFU_MMIO.nvme_write(0x14, cmd)



ADMIN_Q_ENTRIES = 4;
AFU_MMIO.init()

if (SSD0_USED) : print("will get log for SSD0" + " on " + CARD_TXT)
if (SSD1_USED) : print("will get log for SSD1" + " on " + CARD_TXT)

#exit(0)



if (SSD0_USED):
    NVME_Drive.get_log(0);
    NVME_Drive.dump_buffer(0x40)
#    addr = AFU_MMIO.nvme_read(0x88)
#    print ('Buffer Address: %8x' % addr)
#    offset = NVME_Drive.get_AQ_PTR(0)
#    print ('setting buffer address to %8x' % offset)
#    NVME_Drive.dump_buffer_addr(offset,0x20)
#    NVME_Drive.dump_buffer_addr(0x0,0x20)

if (SSD1_USED):
    NVME_Drive.get_log(1);
    NVME_Drive.dump_buffer(0x200)
