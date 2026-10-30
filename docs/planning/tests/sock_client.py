#!/usr/bin/env python3
#
# Connects to the emulator's console socket, waits for "hello, world" from
# echo.bin, sends "ping 123" and checks it comes back.
#
#   sock_client.py HOST PORT

import socket
import sys
import time

host, port = sys.argv[1], int(sys.argv[2])
for _ in range(200):
	try:
		s = socket.create_connection((host, port))
		break
	except OSError:
		time.sleep(0.05)
else:
	sys.exit('could not connect')

s.settimeout(0.5)
buf = b''


def drain(secs):
	global buf
	end = time.time() + secs
	while time.time() < end:
		try:
			d = s.recv(4096)
			if not d:
				break
			buf += d
		except socket.timeout:
			pass


drain(8)
hello = b'hello, world' in buf
s.sendall(b'ping 123')
drain(2)
echoed = b'ping 123' in buf.split(b'hello, world', 1)[-1]
print('got hello:', hello)
print('got echo:', echoed)
sys.exit(0 if hello and echoed else 1)
