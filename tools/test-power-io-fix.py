#!/usr/bin/env python3
"""Static regression checks for XSSH v9.3's port and scheduling choices."""
from pathlib import Path
import re
s=(Path(__file__).resolve().parent.parent/'src/main.c').read_text()
assert 'HalWriteSMBusValue(0x20,0x02,FALSE,0x80)' in s
power=s.split('static void confirm_power(App *a,int yes){',1)[1].split('static void render(App *a)',1)[0]
assert 'HalReturnToFirmware(1)' in power
assert 'HalReturnToFirmware(mode)' not in power
assert 'if(shutdown)' in power
assert 'TCP_NODELAY' in s and 'setsockopt(a->sock,IPPROTO_TCP,TCP_NODELAY' in s
pump=s.split('static void ssh_pump(App *a) {',1)[1].split('static void nav(',1)[0]
assert pump.index('ssh_flush_outgoing(a);') < pump.index('libssh2_channel_read(')
assert 'SSH_READ_BURST_MAX' in pump and 'SSH_READ_TIME_BUDGET_MS' in pump
assert 'a->term.dirty || !a->cache_valid' in s
print('PASS: SMC power off and explicit reboot are distinct')
print('PASS: SSH input prioritization, Nagle disable, bounded reads and idle render')
