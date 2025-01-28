#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

sudo cp $SCRIPT_DIR/99-virtual-joystick.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
