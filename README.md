# sk-vdisplay

Turn the tablets, phones, old laptops and TVs lying around into **extra monitors,
webcams and microphones** for a Linux (X11) machine, with no dummy plugs and no
vendor apps on the host.

- **Screens:** each device gets its own virtual monitor (via the `evdi` kernel
  module) and its own [Sunshine](https://github.com/LizardByte/Sunshine)
  instance, so you open [Moonlight](https://moonlight-stream.org) on the device
  and it shows that screen, with touch, keyboard and sound. Hardware-encoded
  (VAAPI), low latency.
- **Cameras and mics:** each device's camera shows up as a normal webcam
  (`v4l2loopback`) and its mic as a normal microphone (PipeWire), so any call
  app can pick them. Windows PCs push over SRT with ffmpeg, Android devices
  serve RTSP (e.g. the IP Webcam app).

It is a **framework**: this repo holds only generic code. Your machine's
details (screens, devices, addresses) live in a small private **instance**
directory, ideally its own repo with this one pinned as a submodule at a tag.

```
my-laptop-prod/            <- private instance repo
  framework/               <- this repo, git submodule at a release tag
  sk-vdisplay.conf
  screens.conf
  cams/*.env
```

## Why a helper is needed

Intel laptops under X11 (`modesetting`) refuse to turn on a connector with
nothing plugged in, so `xrandr --addmode` tricks fail. `evdi` (the DisplayLink
kernel module) creates real virtual connectors, but **only completes page
flips when a client grabs the frames**. Without one, Xorg blocks in
`drm_crtc_commit_wait` ("flip_done timed out") and the whole desktop freezes.
`sk-vdisplay` is that client: it plugs a monitor in with a generated EDID and
keeps pumping frames for as long as it runs.

## Requirements

- Linux with X11 (tested: Manjaro, Cinnamon, Intel Iris Xe, `modesetting`)
- `evdi` (`evdi-dkms`) and `libevdi`, `v4l2loopback` (`v4l2loopback-dkms`)
- `sunshine`, `ffmpeg` (with `srt`), PipeWire with `pipewire-pulse`, `xrandr`
- Moonlight on each device

## Install

```sh
git submodule add https://github.com/smilinTux/sk-vdisplay framework
git -C framework checkout v0.1.0
cp -r framework/examples/instance/* .      # then edit
SUNSHINE_USER=me SUNSHINE_PASSWORD=... framework/install.sh . --firewall-zone public --enable
```

Then on each device: Moonlight, pick `<HOST_LABEL>-<screen>`, enter the PIN in
that screen's web UI (`https://<host>:<port+1>`), tap **Desktop**.

## Instance config

`screens.conf`, one line per screen:

```
#name    mode           position   sunshine base port
tablet   1920x1200@60   left-of    47989
windows  1920x1080@60   right-of   48089
```

- Sizes must be unique: outputs are matched by size, because evdi connector
  names depend on start order.
- Base ports must be at least 100 apart (each instance uses base-5 to base+21).

`cams/<name>.env`, one per camera source:

| key | meaning |
|---|---|
| `SRC` | ffmpeg input: `srt://0.0.0.0:9710?mode=listener&latency=500000` or `rtsp://<device>:8080/h264_pcm.sdp` |
| `INPUT_OPTS` | extra input options, e.g. `-rtsp_transport tcp` |
| `DEV` | `/dev/videoN` (install.sh sets up v4l2loopback with these numbers) |
| `LABEL` | name shown in apps: "<LABEL> Cam", "<LABEL> Mic" |
| `AUDIO` | `1` to also route the stream's audio to the mic, `0` for video only |
| `ON_CMD` / `OFF_CMD` | optional commands `sk-cam` runs, e.g. ssh to start a Windows sender |

## Commands

| command | does |
|---|---|
| `sk-cam list` / `sk-cam <name> on [arg]` / `sk-cam <name> off` | cameras |
| `sk-vdisplay-layout` | re-apply screen positions |
| `systemctl --user status sk-vdisplay@<name> sk-sunshine@<name> sk-cam@<name>` | services |

## Windows sender

`windows/sk-cam.cmd.in` is a template for streaming a Windows webcam and mic.
Webcams are only reachable from the user's interactive session, so run it as a
scheduled task with an `Interactive` principal (one task per camera), not as a
service.

## Limits

- X11 only. Wayland compositors have their own virtual-output paths.
- Moonlight is one-way (host to device): device cameras and mics come back
  through the `sk-cam` path, not through Moonlight.
- A virtual screen you are not streaming is still part of the desktop, so the
  cursor and windows can land on it. Stop `sk-vdisplay@<name>` when unused.

## License

Apache-2.0
