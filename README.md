# cairo-dwrite-font-view

A font viewer using the cairo dwrite font backend and vanilla DirectWrite.

I'd like to recommend to run multiple instances of the program side-by-side to compere the font renderings.

## Compile and Install

You need Meson, Ninja, pkg-config and Visual Studio.
You can get pkg-config from the [pkg-config website](https://sourceforge.net/projects/pkgconfiglite/) or package managers (Chocolatey or winget).

Get the source code of cairo.

Start "x64 Native Tools Command Prompt for VS 20**".

Compile and install the cairo.
~~~
cd your_cairo_src_dir
meson setup _build_win --prefix c:\path\to\your\install -Dglib=disabled -Dfontconfig=disabled -Dfreetype=disabled
meson install -C _build_win
~~~

Compile and install the cairo-dwrite-font-view.
~~~
cd your_cairo_dwrite_font_view_src_dir
meson setup _build_win --prefix c:\path\to\your\install --pkg-config-path c:\path\to\your\install\lib\pkgconfig
meson install -C _build_win
~~~
