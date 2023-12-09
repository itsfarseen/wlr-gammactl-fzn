#!/usr/bin/sh

# Run this when the XML files in wlr-protocols are updated, to generate up-to-date C interface.

wayland-scanner private-code wlr-protocols/unstable/wlr-gamma-control-unstable-v1.xml src/wlr-gamma-control-unstable-v1-client-protocol.c
wayland-scanner client-header wlr-protocols/unstable/wlr-gamma-control-unstable-v1.xml src/wlr-gamma-control-unstable-v1-client-protocol.h
