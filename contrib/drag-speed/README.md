# Experimental native drag speed multiplier

This optional libinput **1.28.1** patch slows native three-finger dragging
without changing ordinary pointer speed. For example, a multiplier of `0.5`
moves half as far while dragging. The enable-3fg-drag shim continues to enable
the native gesture as usual.

This is a companion experiment, not a setting supported by stock libinput or
by the shim alone. It requires building and maintaining a private libinput
library. The normal installer does not install this extension.

## Why a libinput patch?

The public pointer event API does not identify a native three-finger drag.
Scaling all motion while the left button is pressed would also change ordinary
click dragging. The patch instead checks libinput's actual native drag state
and scales motion after normal pointer acceleration. It preserves ordinary
pointer movement, click dragging, scrolling, and external mouse movement.
It also affects native four-finger dragging if another consumer enables that
mode, since libinput uses the same internal state for both modes.

The multiplier is read at each drag start from the file named by
`THREE_FINGER_DRAG_SPEED_FILE`. Accepted values are `0.1` through `1.0`, inclusive.
Unset, missing, empty, non-finite, malformed, or out-of-range values fall back
to `1.0`. A valid change takes effect on the next drag without restarting GNOME.
Both accelerated and unaccelerated deltas are scaled consistently.

## Build and test

Start with the source for **the exact distribution package installed** on your
machine, including its downstream fixes. The patch was built against Debian
`1.28.1-1+deb13u1`; it is not validated against newer libinput releases.
For Debian, `apt-get source libinput` obtains source if source repositories are
enabled. Check that it matches `dpkg-query -W libinput10` before continuing.

Required build tools and libraries include a C compiler, Meson, Ninja,
pkg-config, and development packages for libudev, libevdev, mtdev, and libwacom.

From this repository, test against an **unpatched** source directory:

```sh
./contrib/drag-speed/test.sh /path/to/libinput-1.28.1
```

The test copies the source to a temporary directory, verifies that the patch
applies without fuzz, and checks scaling and configuration parsing. It does
not open input devices or require root. Then patch and build your private copy:

```sh
patch --batch --fuzz=0 -d /path/to/libinput-1.28.1 -p1 \
    < contrib/drag-speed/libinput-1.28.1.patch
meson setup /path/to/libinput-build /path/to/libinput-1.28.1 \
    --prefix=/usr --buildtype=release \
    -Dtests=false -Ddebug-gui=false -Ddocumentation=false
meson compile -C /path/to/libinput-build
make
```

Do **not** run `meson install` over your distribution's library. Install the
private library and shim into a dedicated user-owned directory instead:

```sh
mkdir -p "$HOME/.local/lib/enable-3fg-drag"
install -m 755 /path/to/libinput-build/libinput.so.10.13.0 \
    "$HOME/.local/lib/enable-3fg-drag/libinput-drag-speed.so"
install -m 755 libenable-3fg-drag.so "$HOME/.local/lib/enable-3fg-drag/"
mkdir -p "$HOME/.config/enable-3fg-drag"
printf '0.5\n' > "$HOME/.config/enable-3fg-drag/speed"
```

## Loading in GNOME on Debian

A compositor must load the shim **before** the private library, for example:

```sh
export LD_PRELOAD="$HOME/.local/lib/enable-3fg-drag/libenable-3fg-drag.so:$HOME/.local/lib/enable-3fg-drag/libinput-drag-speed.so"
export THREE_FINGER_DRAG_SPEED_FILE="$HOME/.config/enable-3fg-drag/speed"
```

These variables must be set when GNOME Shell starts, through its systemd user
service or a session wrapper. Setting them in a terminal after login does not
modify the already-running compositor. Log out and back in after configuring
startup. Do not restart GNOME Shell in a running Wayland session.

Scope loading to the compositor's startup; do not put the private libinput
library in `/etc/ld.so.preload`. Directly launched child processes may inherit
`LD_PRELOAD`. The KDE capability/secure-execution restrictions discussed in the
main README still apply; this experiment has not been validated on KDE.

**Package updates:** a private library does not receive system updates. A
persistent setup must compare the installed libinput package version against
the version used for the private build on every login. If they differ, preload
only the original shim and use system libinput until the extension is rebuilt.
This preserves native dragging, at its original speed, while allowing Debian's
updated library to take effect. The local GNOME trial uses this guard.

To undo, remove the private library from the compositor's preload list and
log out/in. Keep the original shim to retain native three-finger dragging.
Setting the speed file to `1.0` restores normal speed but does **not** unload
the private library.

## Validation and limitations

- Built with Debian's libinput 1.28.1-1+deb13u1 sources on GNOME 48.7 Wayland.
- Unit tests cover unchanged ordinary motion, drag scaling, and invalid/missing
  configuration. The same tests also passed with address/undefined sanitizers.
- The private build preserved all exported symbols from the system library.
- A hardware probe loaded the shim and private library and successfully enabled
  native drag on a Synaptics touchpad reporting five-finger support.
- GNOME Shell's `--version` command loaded successfully through the local wrapper.
- The full upstream gesture suite was not run. End-to-end drag feel, reboot,
  and suspend behavior for the speed extension are not yet verified.

This proposal is intentionally a draft: a supported libinput configuration API
would be preferable to carrying a private patch long-term.
