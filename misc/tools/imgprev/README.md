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
# directory, usually '/usr/local/share/clifm/plugins/' or '/usr/share/clifm/plugins'.

^text/rtf$|^application/.*(officedocument|msword|ms-excel|ms-powerpoint|opendocument).*=~/.config/clifm/clifmimg doc %f %u;
^application/epub\+zip$=~/.config/clifm/clifmimg epub %f %u;
^application/x-mobipocket-ebook$=~/.config/clifm/clifmimg mobi %f %u;
^application/pdf$=~/.config/clifm/clifmimg pdf %f %u;
^image/vnd.djvu$=~/.config/clifm/clifmimg djvu %f %u;
^image/svg\+xml$=~/.config/clifm/clifmimg svg %f %u;
^image/(jpeg|png|tiff|webp|x-xwindow-dump)$=~/.config/clifm/clifmimg image %f %u;
^image/.*=~/.config/clifm/clifmimg gif %f %u;
^video/.*=~/.config/clifm/clifmimg video %f %u;
^audio/.*=~/.config/clifm/clifmimg audio %f %u;
^application/postscript$=~/.config/clifm/clifmimg postscript %f %u;
^font/.*|^application/(font.*|.*opentype)=~/.config/clifm/clifmimg font %f %u;
N:.*\.(cbz|cbr|cbt)$=~/.config/clifm/clifmimg comic %f %u;

# Directories
...

```

This instructs **clifm** to use the [clifmimg script](#the-clifmimg-script) to generate previews for the specified file types (both for TAB completion (in [fzf mode](https://github.com/leo-arch/clifm/wiki/Specifics#tab-completion)) and via the [`view` command](https://github.com/leo-arch/clifm/wiki/Introduction#view)).

Note: In case you don't want image previews for some of these files types, just comment out the corresponding line or change its value to your preferred previewing application.

2. Run **clifm** as usual.

> [!NOTE]
> If using the [`ueberzug` method](#previewing-methods), you must run **clifm** via the [clifmrun script](https://github.com/leo-arch/clifm/blob/master/misc/tools/imgprev/clifmrun).
> You can find this script under `~/.config/clifm/` or `DATADIR/clifm/plugins/` (usually `/usr/local/share/clifm/plugins/` or `/usr/share/clifm/plugins/`).

### Previewing methods

The previewing method is controlled by the `method` variable in the [`clifmimg` script](#the-clifmimg-script).

By default, this variable is unset, meaning that **clifm** will try to [guess the previewing method](#automatic-method-detection). To manually choose a method, set the `method` variable to any of the available methods:

| Method | Description | Observation | Recommended terminal emulator |
| -- | -- | --- | --- |
| `sixel` | Preview images in full color using the sixel protocol | [**chafa**(1)](https://github.com/hpjansson/chafa) is used to generate sixel images. Note that not all terminal emulators support this protocol. Visit https://www.arewesixelyet.com/ for more information. | XTerm, Wezterm, Mlterm, [st (sixel patch)](https://github.com/bakkeby/st-flexipatch), Contour, Foot |
| `ueberzug` | Preview images  in full color using [ueberzug](https://github.com/ueber-devel/ueberzug) | Run **clifm** via the `clifmrun` script (see point 2 in the Usage section).  | Any (X11 only) |
| `kitty` | Preview images  in full color using the [kitty image protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/) | The Kitty terminal is required. | Kitty |
| `ansi` | Preview images using ANSI art (text mode) | Several applications to generate ANSI previews are available: `chafa`, `pixterm`, `img2text`, `viu`, `catimg`, `tiv`, and `timg`. Use the `ansi_method` variable in the [`clifmimg` script](#the-clifmimg-script) to set your preferred application. It defaults to `chafa`. | Any |

> [!NOTE]
> Since the original `ueberzug` is not maintained anymore, we recommend using this fork instead: https://github.com/ueber-devel/ueberzug.

> [!NOTE]
> The sixel method works only partially for KDE Konsole: images cannot be cleaned up automatically (see [this bug report](https://bugs.kde.org/show_bug.cgi?id=456354)). [A fix](https://invent.kde.org/utilities/konsole/-/commit/cc4539f6bfd8e5b6beac23ecd13897c666e88eaa) was commited on October 11, 2024, so that the issue might be fixed in the next release. Meanwhile, we recommned using the `ueberzug` method instead.

### Automatic method detection

At startup, **clifm** tries to guess the previewing method supported by the running terminal and writes the corresponding value into the **CLIFM_IMG_SUPPORT** environment variable, which is then read by the [clifmimg script](#the-clifmimg-script) to generate previews via the specified method. The procedure is as follows:

1. If **CLIFM_FIFO_UEBERZUG** is set (this vartiable is set by the [clifmrun script](https://github.com/leo-arch/clifm/blob/master/misc/tools/imgprev/clifmrun)), **CLIFM_IMG_SUPPORT** is set to `ueberzug`.
2. If **KITTY_WINDOW_ID** is set, **CLIFM_IMG_SUPPORT** is set to `kitty`.
3. If sixel support is detected<sup>1</sup>, **CLIFM_IMG_SUPPORT** is set to `sixel`.
4. Otherwise, **CLIFM_IMG_SUPPORT** is set to `ansi`.

Note that if **CLIFM_IMG_SUPPORT** is unset, the `clifmimg` script falls back to the `ansi` method.

<sup>1</sup> Note for devs: see the `check_sixel_support()` function in the `term.c` file.

## General procedure

The steps involved in generating image previews are:

1. The `clifmrun` script prepares the environment to generate image previews via `ueberzug` and then launches **clifm**.<sup>1</sup> (If not using the [`ueberzug` method](#previewing-methods), this step is ommited).
2. Every time TAB completion is invoked for files (if running in [fzf mode](https://github.com/leo-arch/clifm/wiki/Specifics#tab-completion)), or the [view command](https://github.com/leo-arch/clifm/wiki/Introduction#view) is executed, `fzf` is launched.
3. `fzf` calls shotgun (via `clifm --preview`) to generate a preview of the currently hovered file.
4. Shotgun executes `clifmimg`, which takes care of genereting a thumbnail of the corresponding file.
5. Once the thumbnail is generated, `clifmimg` takes care of disaplying the thumbnail via any of the available [previewing methods](#previewing-methods).

<sup>1</sup> Parameters passed to `clifmrun` will be passed to **clifm** itself.

### The clifmimg script

[This script](https://github.com/leo-arch/clifm/blob/master/misc/tools/imgprev/clifmimg) converts (if necessary) and generates image previews (as thumbnails) for files.

For performance reasons, thumbnails are cached (in the directory pointed to by the `CACHE_DIR` variable<sup>1</sup>) using MD5 hashes as names.

The script takes three parameters: the first one tells the type of file to be previewed, the second one is the file name to be previewed, and the third one is the file URI for the file name to be previewed.<sup>2</sup> For example:

```sh
clifmimg doc %f %u
# which expands to something like this: "clifmimg doc /path/to/file.docx file:///path/to/file.docx"
```

generates a thumbnail of `file.docx` using the `doc` method.

The first parameter (thumbnailing method) can be any of the following:

| Method | Description | Thumbnail generation |
| --- | --- |  --- |
| `image` | Display image directly, without previous convertion | No |
| `gif`, `svg` | Convert image and display | Yes |
| `audio`, `djvu`, `doc`, `epub`, `font`, `mobi`, `pdf`, `postscript`, and `video` | Convert file to image and display | Yes |

The `audio` method accepts four modes: `cover`, `wave`, `spectogram`, and `info`. Set the desired mode via the `audio_method` variable (defaults to `cover`, which falls back to `info` if there is no cover image available).

### The thumbnails database

Every time a thumbnail is generated `clifmimg` adds a new entry to the thumbnails database (`.thumbs.info` in the thumbnails directory<sup>1</sup>). Each entry has this form: **THUMB@PATH**, where **THUMB** is the name of the thumbnail file (an MD5 hash of **PATH** followed by a file extension, either `png` or `jpg`), and **PATH** the file URI for the absolute path to the original file. This database is read by the [`view purge`](https://github.com/leo-arch/clifm/wiki/Introduction#view) command (available since 1.22.12) to keep the thumbnails directory in a clean state.<sup>3</sup>

<sup>1</sup> By default this directory is `$XDG_CACHE_HOME/clifm/thumbnails` (which usually expands to `~/.cache/clifm/thumbnails`). Note that previous versions of this scripts used `$XDG_CACHE_HOME/clifm/previews` instead.

<sup>2</sup> The third parameter (`%u`) is available since version 1.22.13. Prior to this version, only the first two parameters are recognized.

<sup>3</sup> If running a version prior to 1.22.12, make sure to update your [`clifmimg` script](https://github.com/leo-arch/clifm/blob/master/misc/tools/imgprev/clifmimg): `cp /usr/share/clifm/plugins/clifmimg ~/.config/clifm`.

## Dependencies

The following applications are used to generate thumbnails:

| Application | File type | Observation |
| --- | --- | --- |
| `ffmpegthumbnailer` | Video | |
| `gnome-epub-thumbnailer` | ePub | |
| `gnome-mobi-thumbnailer` | Mobi | |
| `pdftoppm` | PDF | Provided by the `poppler` package |
| `ddjvu` | DjVu | Provided by the `djvulibre` package |
| `ffmpeg` | Audio | |
| `fontpreview` | Font | https://github.com/sdushantha/fontpreview |
| `libreoffice` | Office documents | |
| `librsvg` | SVG image | Required by **magick**(1) to convert SVG files | |
| `gs` | Postscript | Provided by the `ghostscript` package |
| `magick` | Several image formats | Provided by the `imagemagick` package |

> [!NOTE]
> The exact package names providing the above programs may vary depending on your OS/distribution, but they usually have the same name as the corresponding program.

## Troubleshooting

Image previews are misplaced: This is mostly the case when your terminal emulator is using a menu bar and/or a scrollbar. `Konsole`, for example, displays by default a menu bar and a main toolbar on the top of the window, plus a scrollbar on the right, which is the cause of the image misplacement. To fix this, tweak the `X` and `Y` variables in the `display` function of the [`clifmimg` script](https://github.com/leo-arch/clifm/blob/master/misc/tools/imgprev/clifmimg) as follows:

```sh
X=$((CLIFM_TERM_COLUMNS - FZF_PREVIEW_COLUMNS - 1)) # 1 extra column: the scroll bar
Y=$((CLIFM_FZF_LINE + 2)) # 2 extra lines: menu bar and main toolbar
```

If the issue persists, bear in mind that **clifm** uses the `CPR` (cursor position report) [escape code](https://www.xfree86.org/current/ctlseqs.html) to get the current position of the cursor on the screen (which then is passed to the `clifmimg` script to generate the preview). If your terminal does not support `CPR` (most do), you are out of look: just try another terminal.
