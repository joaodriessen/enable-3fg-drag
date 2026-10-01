# enable-3fg-drag

**Turn on libinput's built-in, macOS-style three-finger dragging** on
Wayland desktops that ship the feature but don't expose a way to enable it
(KDE Plasma, GNOME, and others).

Rest three fingers on the touchpad and move: the thing under the cursor is
dragged, with the left button implicitly held. Lift to drop. A *fast*
three-finger flick is still a swipe. Four-finger gestures (Overview,
desktop switching) keep working exactly as before.

> This is **not** another reimplementation of drag-from-gestures. libinput
> already does the whole thing — finger detection, the button hold, the
> motion, palm rejection — and does it well. It's just switched off by
> default and no desktop turns it on yet. This project is a ~40-line shim
> that flips that one switch inside your compositor. That's all.

---

## Why this exists

libinput has shipped real three-finger dragging since **v1.28** (Feb 2025):
`libinput_device_config_3fg_drag_set_enabled()`, with the drag synthesised
*inside* libinput so the compositor just receives ordinary pointer button +
motion events. **v1.31** (Feb 2026) added "fast-swipe", so a quick
three-finger move still registers as a swipe — you get drag *and* gesture.

But the feature is **disabled by default**, and it can only be enabled at
runtime by the process that owns the libinput context — i.e. your
compositor. As of early 2026, no mainstream desktop calls the API:

- **KDE Plasma 6.7 / KWin** — doesn't call it, no toggle
  ([KWin issue #59](https://invent.kde.org/plasma/kwin/-/issues/59)).
- **GNOME** — only an open feature request
  ([gnome-control-center #3351](https://gitlab.gnome.org/GNOME/gnome-control-center/-/issues/3351)).

And there is **no config file, environment variable, or quirk** to flip it.
So the only way in on a stock desktop is to reach into the compositor's own
libinput context — which is what this shim does.

## Requirements

- **libinput ≥ 1.28** (≥ **1.31** recommended, for fast-swipe).
  Check with `libinput --version`.
- A **Wayland** session (this enables a libinput device-config option; it
  does not apply to Xorg's separate input path).
- A touchpad libinput recognises as supporting 3-finger drag.
- `gcc`/`clang`, `make`, and the libinput development headers
  (Arch: shipped in `libinput`; Debian/Ubuntu: `libinput-dev`;
  Fedora: `libinput-devel`).

## Install

```sh
git clone https://github.com/joaodriessen/enable-3fg-drag
cd enable-3fg-drag
./install.sh          # auto-detects the best method for your desktop
```

Then **log out and back in** (a full session restart, so the compositor
re-reads its devices). That's it — three fingers now drag.

Verify it engaged (KWin shown; use your compositor's journal otherwise):

```sh
journalctl --user -u plasma-kwin_wayland -b | grep enable-3fg-drag
# -> [enable-3fg-drag] enabled 3-finger drag on "<your touchpad>": Success
```

## Uninstall

```sh
./uninstall.sh        # then log out and back in
```

Removes the library, the systemd drop-in, and the `/etc/ld.so.preload`
entry — whichever were used. Nothing is left behind.

## Install methods

`install.sh` picks one automatically; override with `--method`.

### `kwin` — scoped to KDE Plasma Wayland (default there)

Adds a systemd **user drop-in** that preloads the shim into
`plasma-kwin_wayland.service` and nothing else. The most surgical option:
only your compositor ever loads it.

There's a wrinkle: `/usr/bin/kwin_wayland` carries the file capability
`cap_sys_nice` (for real-time compositor scheduling). A file capability
puts the dynamic loader in **secure-execution mode**, in which it *ignores*
an `LD_PRELOAD` that names a path (e.g. one in your home directory). Per
`man ld.so`, the loader still honours a preload given as a **bare soname**,
located in a **standard library directory**, whose **set-user-ID bit is
set**. So the shim installs to `/usr/lib/libenable-3fg-drag.so` with mode
`4755`, and the drop-in preloads it by soname. (The setuid bit is only the
flag the loader checks — a shared library's setuid bit grants it no
privilege of its own.)

### `global` — any Wayland compositor

Adds the shim to `/etc/ld.so.preload`, which is honoured for every process
including secure-execution ones — so it works regardless of compositor or
capabilities. Because the shim has **no libinput dependency** and is inert
in any process that isn't a libinput consumer (see below), this is cheap;
it's just less surgical than the per-compositor method. Use it on GNOME,
sway, Hyprland, or anywhere the `kwin` method doesn't apply:

```sh
./install.sh --method global
```

## How it works

The shim interposes a single libinput function, `libinput_get_event()`.
Whenever the compositor's libinput reports a newly added device
(`LIBINPUT_EVENT_DEVICE_ADDED`) that supports ≥ 3-finger drag, the shim
enables it on that device and passes the event on untouched. It sets one
config property as a side effect and does nothing else — all the actual
drag logic lives in libinput.

Every libinput symbol is resolved lazily through `RTLD_NEXT`, so the object
**links against nothing but libc**. In any process that isn't a libinput
consumer, `libinput_get_event()` is never called and the shim does
literally nothing — which is what makes a system-wide `/etc/ld.so.preload`
install safe and essentially free.

Because it binds the stable, versioned libinput ABI, it **survives libinput
package updates** without a rebuild, and it does **not** patch or pin
libinput.

## Compatibility

Tested on **KDE Plasma 6.7 Wayland, libinput 1.31.3**, on an Apple
`bcm5974` MacBook touchpad. The mechanism is compositor-agnostic (it hooks a
standard libinput API), so the `global` method should work on any
libinput-based Wayland compositor that hasn't enabled native 3fg-drag.
Reports from other setups are welcome — please open an issue.

If your desktop ever grows a native toggle for this, you won't need the
shim: just `./uninstall.sh` and flip the real setting.

## Troubleshooting

- **No `enable-3fg-drag` line in the journal after relogin.** The shim
  didn't load. On KDE this is usually the secure-execution issue above —
  make sure `/usr/lib/libenable-3fg-drag.so` exists and has the setuid bit
  (`ls -l` shows `-rwsr-xr-x`), and that the drop-in uses the bare soname
  (no path). Or just use `--method global`.
- **Line says a status other than `Success`.** Your libinput may be too old
  (`libinput --version` < 1.28) or the touchpad may not support 3fg-drag.
- **Drag works but fast flicks switch desktops.** That's fast-swipe (a
  feature). If it bothers you, adjust your compositor's 3-finger swipe
  binding; four-finger gestures are unaffected.

## Experimental drag-only speed control

An optional [libinput 1.28.1 companion patch](contrib/drag-speed/README.md)
adds a drag-only speed multiplier while preserving ordinary pointer speed.
It requires a private libinput build and maintenance after package updates;
the normal installer and stock libinput do not expose this setting.

## Relationship to other projects

For KDE's hardcoded-3-finger-swipe problem, there are heavier userspace
approaches that grab the touchpad and synthesise events — notably
[lmr97/linux-3-finger-drag](https://github.com/lmr97/linux-3-finger-drag)
and forks of it. Those remain useful where the native feature can't be used
(libinput < 1.28, or setups where you can't enable it). Where you *can*
enable it, letting libinput do the whole job — as this project does — is
simpler and more robust.

## License

MIT — see [LICENSE](LICENSE).
