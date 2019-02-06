#!/bin/sh

unset VNCD_ARGS
unset VNCD_SERVER
unset VNCD_SESSION

# log everything to home directory
log_directory=$HOME/.local/log
mkdir -p "$log_directory"
exec > "$log_directory/turbovnc.session.log" 2>&1

# clipboard
/opt/TurboVNC/bin/tvncconfig &

# display manager
exec vglrun -fps 60 dwm
