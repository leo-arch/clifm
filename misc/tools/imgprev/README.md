# Image previews

> Because terminals were built to display text rather than images, making them display images can be problematic.  Some solutions work better in some environments than others, but none of them is perfect.

<p align="right"><a href="https://wiki.vifm.info/index.php/How_to_preview_images">Vifm wiki</p>

## Table of contents

* [Usage](#usage)
* [General procedure](#general-procedure)
* [Dependencies](#dependencies)
* [Troubleshooting](#troubleshooting)

---

<h4 align="right">TAB completion with image previews</h4>
<p align="right"><img src="https://i.postimg.cc/fTG6W3yb/fzf-preview.jpg"></p>

<h4 align="left">Preview files in full screen (via the <i>view</i> command)</h4>
<p align="left"><img src="https://i.postimg.cc/52PKY6Nv/view-preview.jpg"></p>

---

## Usage

1. Run `view edit` (or press <kbd>F7</kbd>) to edit [shotgun's](https://github.com/leo-arch/clifm/wiki/Advanced#shotgun) configuration file, and uncomment the following lines from the top of the file:

```sh
...
# If the clifmimg script cannot be found under '~/.config/clifm/', you can copy it from the data
# directory, usually '/usr/local/share/clifm/plugins/'.

^application/.*(officedocument|msword|ms-excel|ms-powerpoint|opendocument).*=~/.config/clifm/clifmimg doc;
^text/rtf$=~/.config/clifm/clifmimg doc;
^application/epub\+zip$=~/.config/clifm/clifmimg epub;
^application/x-mobipocket-ebook$=~/.config/clifm/clifmimg mobi;
^application/pdf$=~/.config/clifm/clifmimg pdf;
^image/vnd.djvu=~/.config/clifm/clifmimg djvu;
^image/svg\+xml$=~/.config/clifm/clifmimg svg;
^image/gif$=~/.config/clifm/clifmimg gif;
^image/.*=~/.config/clifm/clifmimg image;
^video/.*=~/.config/clifm/clifmimg video;
^audio/.*=~/.config/clifm/clifmimg audio;
^application/postscript$=~/.config/clifm/clifmimg postscript;
^font/.*|^application/(font.*|.*opentype)=~/.config/clifm/clifmimg font;

# Directories
...

```

This instructs **clifm** to use the [clifmimg script](#the-clifmimg-script) to generate previews for the specified file types (both for TAB completion (in [fzf mode](https://github.com/leo-arch/clifm/wiki/Specifics#tab-completion)) and via the [`view` command](https://github.com/leo-arch/clifm/wiki/Introduction#view)).

Note: In case you don't want image previews for some of these files types, just comment out the corresponding line or change its value to your preferred previewing application.

2. Run **clifm** as usual (note that if using the [`ueberzug` method](#previewing-methods), you must run **clifm** via the [clifmrun script](https://github.com/leo-arch/clifm/blob/master/misc/tools/imgprev/clifmrun) (you can find it under `~/.config/clifm/` or `DATADIR/clifm/plugins/` (usually `/usr/local/share/clifm/plugins/` or `/usr/share/clifm/plugins`).

### Previewing methods

The previewing method is controlled by the `method` variable in the [`clifmimg` script](https://github.com/leo-arch/clifm/edit/master/misc/tools/imgprev/README.md#the-clifmimg-script).

By default, this variable is unset, meaning that **clifm** will try to [guess the previewing method](#automatic-method-detection). To manually choose a method, set the `method` variable to any of the available methods:

| Method | Description | Observation |
| -- | -- | --- |
| `sixel` | Preview images in full color using the sixel protocol | [**chafa**(1)](https://github.com/hpjansson/chafa) is used to generate sixel images. Note that not all terminal emulators support this protocol. Visit https://www.arewesixelyet.com/ for more information. |
| `ueberzug`<sup>1</sup> | Preview images  in full color using [ueberzug](https://github.com/ueber-devel/ueberzug) | Run **clifm** via the `clifmrun` script (see point 2 in the Usage section).  |
| `kitty` | Preview images  in full color using the [kitty image protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/) | The Kitty terminal is required. |
| `ascii` | Preview images using ASCII characters | Several applications to generate ASCII previews are available: `chafa`, `pixterm`, `img2text`, `viu`, `catimg`, `tiv`, and `timg`. Use the `ascii_method` variable in the [`clifmimg` script](https://github.com/leo-arch/clifm/edit/master/misc/tools/imgprev/README.md#the-clifmimg-script) to set your preferred application. It defaults to `chafa`. |

<sup>1</sup> Since the original `ueberzug` is not maintained anymore, we recommend using this fork instead: https://github.com/ueber-devel/ueberzug.

### Automatic method detection

At startup, **clifm** tries to guess the previwing method supported by the running terminal and writes the corresponding value into the **CLIFM_IMG_SUPPORT** environment variable, which is then read by the [clifmimg script](#the-clifmimg-script) to generate previews via the specified method. The procedure is as follows:

1. If **CLIFM_FIFO_UEBERZUG** is set (this vartiable is set by the [clifmrun script](https://github.com/leo-arch/clifm/blob/master/misc/tools/imgprev/clifmrun)), **CLIFM_IMG_SUPPORT** is set to `ueberzug`.
2. If **KITTY_WINDOW_ID** is set, **CLIFM_IMG_SUPPORT** is set to `kitty`.
3. If sixel support is detected<sup>1</sup>, **CLIFM_IMG_SUPPORT** is set to `sixel`.
4. Otherwise, **CLIFM_IMG_SUPPORT** is set to `ascii`.

Note that if **CLIFM_IMG_SUPPORT** is unset, the `clifmimg` script falls back to the `ascii` method.

<sup>1</sup> Note for devs: see the `check_sixel_support()` function in the `term.c` file.

## General procedure

The steps involved in generating image previews are:

1. The `clifmrun` script prepares the environment to generate image previews via `ueberzug` and then launches **clifm**.<sup>1</sup> (if not using the [`ueberzug` method](#previewing-methods), this step is ommited).
2. Every time TAB completion is invoked for files (if running in [fzf mode](https://github.com/leo-arch/clifm/wiki/Specifics#tab-completion)), or the [view command](https://github.com/leo-arch/clifm/wiki/Introduction#view) is executed, `fzf` is launched.
3. `fzf` calls shotgun (via `clifm --preview`) to generate a preview of the currently hovered file.
4. Shotgun executes `clifmimg`, which takes care of genereting a thumbnail of the corresponding file.
5. Once the thumbnail is generated, `clifmimg` takes care of disaplying the thumbnail via any of the available [previewing methods](#previewing-methods).

<sup>1</sup> Parameters passed to `clifmrun` will be passed to **clifm** itself.

### The clifmimg script

[This script](https://github.com/leo-arch/clifm/blob/master/misc/tools/imgprev/clifmimg) converts (if necessary) and generates image previews (as thumbnails) for files.

For performance reasons, thumbnails are cached (in the directory pointed to by the `CACHE_DIR` variable<sup>1</sup>) using file hashes as names (this allows us to securly identify files independently of their actual name).

It takes two parameters: the first one tells the type of file is to be previewed. The second one is the file name to be previewed. For example:

```sh
clifmimg doc /path/to/file.docx
```

generates a thumbnail of `file.docx` using the method named `doc`.

The first parameter (thumbnailing method) can be any of the following: `image`, `video`, `audio`, `gif`,  `svg`, `epub`, `mobi`, `pdf`, `djvu`, `doc`, `postscript`, and `font`.

<sup>1</sup> By default this directory is `${XDG_CACHE_HOME:-$HOME/.cache}/clifm/previews` (which usually expands to `~/.cache/clifm/previews`).

## Dependencies

The following applications are used to generate thumbnails:

| Application | File type | Observation |
| --- | --- | --- |
| `ueberzug` | Image files | Images are displayed directly. No thumbnail generation is required |
| `ffmpegthumbnailer` | Video files | |
| `gnome-epub-thumbnailer`/`epub-thumbnailer` | ePub files | |
| `gnome-mobi-thumbnailer` | Mobi files | |
| `pdftoppm` | PDF files | Provided by the `poppler` package |
| `ddjvu` | DjVu files | Provided by the `djvulibre` package |
| `ffmpeg` | Audio files | |
| `fontpreview` | Font files |
| `libreoffice` | Office files (odt, docx, xlsx, etc) | |
| `gs` | Postscript files | Provided by the `ghostscript` package |
| `magick` | SVG files | Provided by the `imagemagick` package |

**Note**: The exact package names provinding the above programs vary depending on your OS/distribution, but ususally they have the same name as the corresponding program.

## Troubleshooting

Image previews are misplaced: This is mostly the case when your terminal emulator is using a menu bar and/or a scrollbar. `Konsole`, for example, displays by default a menu bar and a main toolbar on the top of the window, plus a scrollbar on the right, which is the cause of the image misplacement. To fix this, tweak the `X` and `Y` variables in the `display` function of the [`clifmimg` script](https://github.com/leo-arch/clifm/tree/master/misc/tools/imgprev#clifmimg) as follows:

```sh
X=$((CLIFM_TERM_COLUMNS - FZF_PREVIEW_COLUMNS - 1)) # 1 extra column: the scroll bar
Y=$((CLIFM_FZF_LINE + 2)) # 2 extra lines: menu bar and main toolbar
```

If the issue persists, bear in mind that **clifm** uses the `CPR` (cursor position report) [escape code](https://www.xfree86.org/current/ctlseqs.html) to get the current position of the cursor on the screen (which then is passed to the `clifmimg` script to generate the preview). If your terminal does not support `CPR` (most do), you are out of look: just try another terminal.
