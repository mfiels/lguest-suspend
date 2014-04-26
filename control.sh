#! /bin/bash
echo $1 | socat - UNIX-CONNECT:./.lguest_control
