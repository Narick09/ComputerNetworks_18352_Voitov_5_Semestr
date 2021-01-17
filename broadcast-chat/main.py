import socket
from threading import Thread
import queue


def receiver(socket, message):
    while True:
        # socket.recvfrom (bufsize) which returns us data and the address of the socket from which this data was received
        message, addr = socket.recvfrom(1024)
        print('from:', addr, ': ', message.decode())


def sender(socket, mes):
    while True:
        str = input()
        socket.sendto(str.encode(), ('255.255.255.255', 11719))


# create listening socket

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# indicates that packets will be broadcast
s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
# allows multiple applications to "listen" to the socket
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
# to listen, we connect to the address '0.0.0.0' - this means that all interfaces are listened to, 11719 - port_num
s.bind(('0.0.0.0', 11719))

# create flood broadcast network

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
mes = 'Hello!'

thread_receive = Thread(target=receiver, args=(s, mes))
thread_send = Thread(target=sender, args=(sock, mes))
thread_send.start()
thread_receive.start()
