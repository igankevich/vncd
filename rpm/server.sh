#!/bin/sh

set -e
if ! test ${HOME+x}
then
	echo "HOME is not set"
	exit 1
fi
if ! test ${VNCD_UID+x}
then
	echo "VNCD_UID is not set"
	exit 1
fi
if ! test ${VNCD_PORT+x}
then
	echo "VNCD_PORT is not set"
	exit 1
fi

# log everything to home directory
log_directory=$HOME/.local/log
mkdir -p "$log_directory"
exec > "$log_directory/turbovnc.server.log" 2>&1

# generate keys
h=$(hostname)
key=$HOME/.config/VirtualGL/xauth-server-key
#key=/etc/opt/VirtualGL/vgl_xauth_key
xauth add "$h/unix:$VNCD_UID" . $(mcookie)
xauth merge $key

# X server
exec /opt/TurboVNC/bin/Xvnc \
	:$VNCD_UID \
	-securitytypes none \
	-pamsession \
	-rfbport "$VNCD_PORT" \
	-fp catalogue:/etc/X11/fontpath.d \
	-once \
	-nevershared \
	-noreverse \
	-localhost
