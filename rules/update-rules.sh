#!/bin/bash

sudo cp 99-virtual-joystick.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger