# wlr-gammactl-fzn

### Usage

```sh
wlr-gammactl-fzn [-c r:g:b] [-b r:g:b] [-g r:g:b]

-c: Contrast
-b: Brightness
-g: Gamma
```

## Build

```sh
./cbuild.py build
```

### Pre-requisites

* python3
* Any decent C compiler
* libwlroots.so
* libwayland-client.so

## Install

```sh
cp build/wlr-gammactl-fzn <wherever-you-want-to>
```


## Introduction

Adapted from: https://github.com/mischw/wl-gammactl

I wanted to be able to adjust the brightness, contrast and gamma separately for
each of the RGB channels to remove the blue shift of the cheap TN panel in my
cheap ASUS E410MA laptop.

`wl-gammactl` only allowed updating them together for the RGB channels.

So I adapted the code to do it separately for each channel.

### Other changes

* Removed GTK GUI to keep the code simple.
* Replaced meson+ninja build system with cbuild.py.  
  https://github.com/itsfarseen/cbuild.py
* Un-gitignore the `src/wlr-gamma-control-unstable-v1-client-protocol.{c,h}` files.

### Tips

**Here's the configuration I use for my TN panel**

```sh
wlr-gammactl-fzn -c 0.97:1.01:0.95 -g 1.02:1.02:1.0 -b 1.01:1.005:0.99
```

**Live experimentation without GUI**

* Run `run-watch.sh` in a terminal.
* Edit `run.sh` and save. It will get automatically applied.
