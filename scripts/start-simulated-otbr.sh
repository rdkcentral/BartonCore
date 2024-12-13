#!/bin/env bash

sudo service dbus start
sudo otbr-agent -I wpan0 -B eth0 -d 7 -v "spinel+hdlc+forkpty://`which ot-rcp`?forkpty-arg=1"
