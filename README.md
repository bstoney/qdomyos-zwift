# qdomyos-zwift
Zwift bridge for Domyos treadmills

<a href="https://www.buymeacoffee.com/cagnulein" target="_blank"><img src="https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png" alt="Buy Me A Coffee" style="height: 41px !important;width: 174px !important;box-shadow: 0px 3px 2px 0px rgba(190, 190, 190, 0.5) !important;-webkit-box-shadow: 0px 3px 2px 0px rgba(190, 190, 190, 0.5) !important;" ></a>

![UI](docs/ui.png)

### Features

1. Domyos compatible
2. Zwift compatible
3. Create, load and save train programs
4. Measure distance, elevation gain and watts
5. Gpx import (with difficulty slider)

![First Success](docs/first_success.jpg)

### Installation from source

$ sudo apt upgrade && sudo apt update # this is very important on raspberry pi: you need the bluetooth firmware updated!

$ sudo apt install git libqt5bluetooth5 libqt5widgets5 libqt5positioning5 libqt5xml5 qtconnectivity5-dev

$ git clone https://github.com/cagnulein/qdomyos-zwift.git

$ cd src

$ qmake

$ make -j4

$ sudo ./qdomyos-zwift

### Tested on

Raspberry PI 0W and Domyos Intense Run

### No gui version

run as

$ sudo ./qdomyos-zwift -no-gui

### Reference

https://github.com/ProH4Ck/treadmill-bridge

https://www.livestrong.com/article/422012-what-is-10-degrees-in-incline-on-a-treadmill/

### Blog

https://robertoviola.cloud
