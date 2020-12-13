#!/usr/bin/python3

from scapy.all import sniff
import threading
from matplotlib import pyplot
from matplotlib import animation
import sys

### Reading arguments
if len(sys.argv) > 2:
    print('Usage:', sys.argv[0], '[filter]')
    exit()
if len(sys.argv) == 2:
    filt = sys.argv[1]
else:
    print('Enter valid pcap filter:')
    filt = input()
###

### Capturing packets
time_points = [0]
data = [0]


def handlePacket(packet):
    global data
    data[-1] += len(bytes(packet))


capture_thread = threading.Thread(target=sniff, kwargs={'prn': handlePacket, 'filter': filt})
capture_thread.start()
print('Started capturing...')
###

### Drawing the plot
time_interval = 0.5  # seconds
time_width = 15  # seconds


def animate(i):
    while len(data) * time_interval > time_width:
        data.pop(0)
        time_points.pop(0)

    plot.clear()
    plot.set_title('Network activity')
    plot.set_xlabel('Seconds, s')
    plot.set_ylabel('Data, bytes')
    plot.plot(time_points, data)

    data.append(0)
    time_points.append(time_points[-1] + time_interval)


figure = pyplot.figure()
figure.canvas.set_window_title('Network activity')
plot = figure.add_subplot(111)
a = animation.FuncAnimation(figure, animate, interval=time_interval * 1000)
pyplot.show()
###
