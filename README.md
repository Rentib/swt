# swt - simple wayland terminal

swt is basically a port of [st](https://st.suckless.org/) to wayland.

Why? All the terminals for wayland either use 300+ megs of ram or don't have
features I want. Foot is fine, but it has neither ligatures nor color
reloading. This is still a work in progress.

This project aims to do only the things that a terminal emulator should do
without things that can be done by a terminal multiplexer.

## Added features

- csi 4:n, 21 (styled underline)
- csi 58,59 (underline color)
- ligatures (can be enabled in the Makefile)

## TODO

- [ ] fontconfig
- [ ] clipboard
- [ ] application sync
- [ ] kitty graphics or sixel (probably kitty)
- [ ] selection and mouse
- [ ] alpha

features are listed from most to least urgent (in my opinion)

Note that live-reloading colors can be trivially done with like 10 lines of
c code by creating some signal handler.

## Non-goals

- scrollback, reflow, tabs, panes, windows (just use tmux or something)
- gpu acceleration or something
- window decorations
- complicated codebase
- configuration file

## Acknowledgments

- obviously [st](https://st.suckless.org/)
- and also [havoc](https://github.com/ii8/havoc)
