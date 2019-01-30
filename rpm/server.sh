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
h=$(hostname)
key=$HOME/.config/VirtualGL/xauth-server-key
#key=/etc/opt/VirtualGL/vgl_xauth_key
xauth add "$h/unix:$VNCD_UID" . $(mcookie)
xauth merge $key
exec /opt/TurboVNC/bin/Xvnc \
	:$VNCD_UID \
	-geometry 1240x900 \
	-depth 24 \
	-rfbwait 60000 \
	-securitytypes none \
	-rfbport "$VNCD_PORT" \
	-fp catalogue:/etc/X11/fontpath.d \
	-deferupdate 1 \
	-once \
	-reset \
	-terminate \
	-idletimeout 30
