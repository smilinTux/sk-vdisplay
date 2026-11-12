# Changelog

## v0.1.1

- sk-vdisplay@: re-apply the layout 15s after start too, since the desktop re-arranges outputs after a hotplug (seen when adding a 4th screen).

## v0.1.0

First release.
- `sk-vdisplay`: evdi virtual monitor client (generated EDID, frame pump so X never blocks on flip_done, exclusive device claim).
- Config-driven screens (`screens.conf`), one Sunshine instance per screen (`sk-sunshine@`), layout around an anchor output.
- Device cameras and mics (`cams/*.env`): v4l2loopback webcams + PipeWire virtual mics, SRT (Windows sender template) and RTSP (Android) sources, `sk-cam` on/off.
- `install.sh` renders Sunshine configs, PipeWire mics, module options and firewalld ports from an instance dir.
