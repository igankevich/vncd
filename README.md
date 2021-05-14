# Overview

VNCD is a daemon that automatically spawns X servers and sessions for VNC
clients.
1. Each user is assigned a unique port.
2. When VNC client connects to this port a server script, specified by
   `VNCD_SERVER` environment variable, is executed
   User id, group id and local port are passed via environment variables
   `VNCD_UID`, `VNCD_GID` and `VNCD_PORT` respectively.
3. After the port becomes active a session script, specified by
   `VNCD_SESSION` environment variable is executed.

VNCD can be used with any VNC server, but we tested only TurboVNC.  The
advantage of using VNCD over plain VNC is that you no longer need to
log in to the server via SSH to spawn personal VNC server: instead you run VNCD,
connect to your unique port, type login and password (using UnixLogin
authorisation method of TurboVNC) and go on with your work.


# TurboVNC server and session scripts

Here is an example of server script for TurboVNC.

```bash
set -e

# log everything to home directory
log_directory=$HOME/.local/log
mkdir -p "$log_directory"
exec > "$log_directory/turbovnc.server.log" 2>&1

# generate keys
h=$(hostname)
key=/etc/opt/VirtualGL/vgl_xauth_key
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
```

The corresponding session script is the following.
```bash
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
```

# Build and install

We use Meson build system to build and install the programme.
```bash
meson . build
cd build
ninja
ninja install
```
