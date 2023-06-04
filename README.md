# AdvancedYoutubeCounter
A more advanced Youtube counter that has iot configuration interface for ESP32 uC


YOUTUBE COUNTER / DUAL CLOCK / Web Config

TLDR;

once the code is loaded it will enter the setup mode, you will need to configure your computer to connect to MyESP32AP, password123
once connected to the esp, you can put in your web browser http://192.168.4.1 which will bring up the configuration page. enter your home AP name and Password along with the other details and click save. As long as everything is ok, it should start showing the time and youtube subscriber accounts


More detail;
This code is designed to operate on an ESP32 based microcontroller. Various reasonings were taken into consideration when picking the microcontroller but it basically boils down to total memory and speed. Since were dealing with RGB matrix displays a fair amount of ram is dedicated to the display. Althought this software can be configured to run many HUB75 displays it is not without limits as the more pixles the more ram will be used. 

This software also lends itself to plug and play operation to someone who is not microcontroller savvy. Because there is a basic web interface that on default, will allow someone to connect to the microcontroller in AP mode (default setup mode) to enter the setup information such as the end users AP name and its password. 

Once the microcontroller is connected to the users AP,  it will temporarily show the address that it can be reconfigured or reset back to factory mode again through a web interface. There is also the use of IO pin 33 that when pulled to groud for less than 10 seconds will reboot the esp32, if held for more than 10 seconds will also put the unit back into default setup mode. The pages will flip from subscriber count to clock mode every 30 seconds and pull subscriber data from youtube every 5 minutes.

there are two animate functions in the program that display two animated sprites, these sprites must be vertical and can be changed to suit the user or removed entirely to create more text space on the youtube subscriber display page. animate is displayed on the right hand side and animate1 is displayed on the right hand side of the display. the coordinates can be changed to suit the matrix types being used.  

converting animated sprite images was done at https://lvgl.io/tools/imageconverter using cf_true_color and C array. It will generate a file with multiple methods of encoding the image and colors. this program uses /*Pixel format: Fix 0xFF: 8 bit, Red: 8 bit, Green: 8 bit, Blue: 8 bit*/
RGB format so only that part will need be pasted in the image.c or ytimage.c files. You will need to update the animate functions and defines to reflect the X and Y resolution as well as how many frames are to be displayed. 

Creators YT channel.

https://www.youtube.com/channel/UCu9VPhckeKef3dNAcJGBXwQ

