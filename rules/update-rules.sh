#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

cp $SCRIPT_DIR/99-virtual-joystick.rules /etc/udev/rules.d/
udevadm control --reload-rules
udevadm trigger
