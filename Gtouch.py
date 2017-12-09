# Single Color Grayscale Blob Tracking Example
#
# This example shows off single color grayscale tracking using the OpenMV Cam.

import sensor, image, time
from pyb import UART, Pin
uart = UART(3, 115200)
pin6 = Pin('P6', Pin.OUT_PP)    #used as gnd power
pin6.low()
#uart.init(115200, bits=8, parity=None, stop=1)
#uart.init(115200, bits=8, parity=None, stop=1, *, timeout=1000, flow=0, timeout_char=0, read_buf_len=64)

# Color Tracking Thresholds (Grayscale Min, Grayscale Max)
# The below grayscale threshold is set to only find extremely bright white areas.
thresholds = (70, 255)  #可以放宽,毕竟是有滤光片,放宽后数据更多有助于后期处理
#roi = (x, y, w, h)
sensor.reset()
sensor.set_pixformat(sensor.GRAYSCALE)
sensor.set_framesize(sensor.VGA)
sensor.skip_frames(time = 2000)
sensor.set_auto_gain(False, value=100)
sensor.set_auto_exposure(False, value=3600)
#sensor.set_auto_exposure(False,1)
#sensor.set_auto_gain(False) # must be turned off for color tracking
sensor.set_auto_whitebal(False) # must be turned off for color tracking
clock = time.clock()
rect=[8,109,623,269]
ROI=[8,80,827,320]
# Only blobs that with more pixels than "pixel_threshold" and more area than "area_threshold" are
# returned by "find_blobs" below. Change "pixels_threshold" and "area_threshold" if you change the
# camera resolution. "merge=True" merges all overlapping blobs in the image.

while(True):
    clock.tick()
#    uart.write("newcycle\n")
    img = sensor.snapshot()
    newdata = 0
    for blob in img.find_blobs([thresholds],roi = (ROI),x_stride = 3,y_stride =20,invert = False, pixels_threshold=100, area_threshold=100, merge=True):
        img.draw_rectangle(blob.rect())
        img.draw_cross(blob.cx(), blob.cy())
        uart.write("X:%d"%blob.cx())
        uart.write("Y:%d\r\n"%blob.cy())
        print(blob.cx())
        print(blob.cy())
    img.draw_rectangle(rect)

#    uart.write("counts:%d\n"%counts)
    print(clock.fps())
    #if (newdata == 1):
    uart.write("EndCycle\r\n")

#uart.write("X:%dY:%d\n"%blob.cx()%blob.cy(
