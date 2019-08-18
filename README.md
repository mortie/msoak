# MSoak

`msoak` is a tool to soak up build output and present it nicely. It will:

* Open the result in `less` if the build failed, so that you immediately see
  the first (i.e most relevant) lines.
* Color code nesting in C++ templates, to make template errors slightly more
  survinable.

![Comparison](https://raw.githubusercontent.com/mortie/msoak/master/screenshot.jpg)

The screenshot shows the same build failure with and without msoak.

## Build

	git clone https://github.com/mortie/msoak.git
	cd msoak
	make
	sudo make install

# Usage

	msoak <command...>

For example, to use msoak with make, you would probably do `msoak make -j 8`.

You can set the pager command with the `MSOAK_PAGER` env variable, so
`MSOAK_PAGER=more msoak make` would use `more` instead of `less`.
