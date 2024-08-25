# Image previews

> Because terminals were built to display text rather than images, making them display images can be problematic.  Some solutions work better in some environments than others, but none of them is perfect.

<p align="right"><a href="https://wiki.vifm.info/index.php/How_to_preview_images">Vifm wiki</p>

## Table of contents

* [Description](#description)
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

## Description

Based on [vifmimg](https://github.com/cirala/vifmimg), these scripts ([clifmimg](https://github.com/leo-arch/clifm/blob/master/misc/tools/imgprev/clifmimg) and [clifmrun](https://github.com/leo-arch/clifm/blob/master/misc/tools/imgprev/clifmrun)) are intended to provide image preview capabilities to **clifm** using either [ueberzug](https://github.com/ueber-devel/ueberzug) (default)<sup>1</sup> or the [kitty image protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/) (currently experimental and slower). Bear in mind that `ueberzug` only works on X11.

For the Sixel method, consult the [Sixel section](#sixel-graphics-and-fzf) below.

<sup>1</sup> Since the original `ueberzug` is not maintained anymore, we strongly recommend using this fork instead: https://github.com/ueber-devel/ueberzug.

### clifmrun

Launch an instance of `ueberzug` and set the appropriate values (as environment variables) so that it can be used by [shotgun](https://github.com/leo-arch/clifm/wiki/Advanced#shotgun) (clifm's built-in previewer) to display images via `clifmimg` (see below).

Parameters passed to `clifmrun` will be passed to **clifm** itself.

Sixel graphics and the kitty image protocol are also supported. See the [Usage section](#usage) below.

### clifmimg

Convert (if necessary) and preview images (thumbnails) via an already running instance of `ueberzug`, launched by `clifmrun` (see above).

Thumbnails are cached (in the directory pointed to by the `CACHE_DIR` variable, by default `${XDG_CACHE_HOME:-$HOME/.cache}/clifm/previews`, usually (`$HOME/.cache/clifm/previews`)) using file hashes as names.

It takes two parameters: the first one tells what kind of file is to be converted/displayed. The second one is the file name to be converted/displayed. For example:

```sh
clifmimg doc /path/to/file.docx
```

generates a thumbnail of `file.docx` using the method named `doc`.

The first parameter (thumbnailing method) could be any of the following:

* image
* video
* gif
* epub
* pdf
* djvu
* audio
* font
* doc
* postscript
* svg

## Usage

1. Run `view edit` (<kbd>F7</kbd> is also available) to edit [shotgun's](https://github.com/leo-arch/clifm/wiki/Advanced#shotgun) configuration file (`$HOME/.config/clifm/profiles/PROFILE/preview.clifm`), and add the following lines at the top of the file (to make sure they won't be overriden by previous directives):

```sh
...
# Uncomment and edit this line to use Ranger's scope script: 
#.*=/home/USER/.config/ranger/scope.sh %f 120 80 /tmp/clifm/ True;

# If the clifmimg script cannot be found under '~/.config/clifm/', you can copy it from the data
# directory, usually '/usr/local/share/clifm/plugins/'.

^application/.*(officedocument|msword|ms-excel|opendocument).*=~/.config/clifm/clifmimg doc;
^text/rtf$=~/.config/clifm/clifmimg doc;
^application/epub\+zip$=~/.config/clifm/clifmimg epub;
^application/pdf$=~/.config/clifm/clifmimg pdf;
^image/vnd.djvu=~/.config/clifm/clifmimg djvu;
^image/svg\+xml$=~/.config/clifm/clifmimg svg;
^image/gif$=~/.config/clifm/clifmimg gif;
^image/.*=~/.config/clifm/clifmimg image;
^video/.*=~/.config/clifm/clifmimg video;
^audio/.*=~/.config/clifm/clifmimg audio;
^application/postscript$=~/.config/clifm/clifmimg postscript;
N:.*\.otf$=~/.config/clifm/clifmimg font;
font/.*=~/.config/clifm/clifmimg font;

# Directories
...

```

This instructs **clifm** to use `clifmimg` to generate previews for the specified file names or file types (both for TAB completion (in [fzf mode](https://github.com/leo-arch/clifm/wiki/Specifics#tab-completion)) and via the [`view` command](https://github.com/leo-arch/clifm/wiki/Introduction#view)).

In case you don't want image preview for some of these files types, just comment out the corresponding line or change its value to your preferred previewing application.

2. Edit the `display()` function in the `clifmimg` script and choose your preferred previewing method. It default to `ueberzug`.

3. Run **clifm** via the `clifmrun` script (you can find it under `DATADIR/clifm/plugins/`, usually, `/usr/local/share/clifm/plugins/`). In case you're running with fzf (version 0.44 or later) and the sixel method, running `clifmrun` is not necessary.

```sh
clifmrun
```

### Sixel graphics and Fzf

If you have a [sixel capable terminal](https://github.com/saitoha/libsixel#terminal-requirements) and `fzf` (version 0.44 or later) you can easily take advantage of this capability as follows:

1. Edit the `clifmimg` script and uncomment the `chafa` line (used to generate sixel images):
```sh
chafa -f sixel -s "${C}x$((L - 1))" "$1"; exit 0
```
2. Run **clifm** as usual (`clifmrun` is **not** required).

### Kitty and Wayland

If running on the kitty terminal you can force the use of the kitty image protocol (instead of `ueberzug`) as follows:

```sh
CLIFM_KITTY_NO_UEBERZUG=1 clifmrun --fzfpreview
```
Note that on Wayland the kitty image protocol will be used by default, so that there is no need to set this variable.

### ASCII/Unicode images

If you prefer a character art method (instead of `ueberzug` or `kitty`), just uncomment the corresponding line in the `display` function in the `clifmimg` file. For example, to use [**chafa**(1)](https://github.com/hpjansson/chafa/):

```sh

display() {

    [ -z ] && exit 1

    chafa -f symbols -s "$((FZF_PREVIEW_COLUMNS - 2))x$((FZF_PREVIEW_LINES))" "$1"; exit 0
#  pixterm -s 2 -tc "$((FZF_PREVIEW_COLUMNS - 2))" -tr "$FZF_PREVIEW_LINES" "$1"; exit 0
#  img2txt -H"$FZF_PREVIEW_LINES" -W"$((FZF_PREVIEW_COLUMNS - 2))" "$1"; exit 0
...
```

**Note**: In this case, there is no need to run `clifmrun`.

## General procedure

The steps involved in generating image previews are:

1. `clifmrun` creates an instance of `ueberzug` and then launches **clifm**.
2. Every time TAB completion is invoked for files (if running in fzf mode), or the [view command](https://github.com/leo-arch/clifm/wiki/Introduction#view) is executed, `fzf` is launched.
3. `fzf` calls shotgun (via `clifm --preview`) to generate a preview of the currently hovered file.
4. Shotgun executes `clifmimg`, which takes care of genereting a thumbnail of the corresponding file.
5. Once the thumbnail is generated, `clifmimg` sends the image to `ueberzug`, which takes care of placing it on the fzf preview window.

## Dependencies

The following applications are used to generate thumbnails:

| Application | File type | Observation |
| --- | --- | --- |
| `ueberzug` | Image files | Images are displayed directly. No thumbnail generation is required |
| `ffmpegthumbnailer` | Video files | |
| `epub-thumbnailer` | ePub files | |
| `pdftoppm` | PDF files | Provided by the `poppler` package |
| `ddjvu` | DjVu files | Provided by the `djvulibre` package |
| `ffmpeg` | Audio files | |
| `fontpreview` | Font files |
| `libreoffice` | Office files (odt, docx, xlsx, etc) | |
| `gs` | Postscript files | Provided by the `ghostscript` package |
| `convert` | SVG files | Provided by the `imagemagick` package |

**Note**: The exact package names provinding the above programs vary depending on your OS/distribution, but ususally they have the same name as the corresponding program.

## Troubleshooting

Image previews are misplaced: This is mostly the case when your terminal emulator is using a menu bar and/or a scrollbar. `Konsole`, for example, displays by default a menu bar and a main toolbar on the top of the window, plus a scrollbar on the right, which is the cause of the image misplacement. To fix this, tweak the `X` and `Y` variables in the `display` function of the [`clifmimg` script](https://github.com/leo-arch/clifm/tree/master/misc/tools/imgprev#clifmimg) as follows:

```sh
X=$((CLIFM_TERM_COLUMNS - FZF_PREVIEW_COLUMNS - 1)) # 1 extra column: the scroll bar
Y=$((CLIFM_FZF_LINE + 2)) # 2 extra lines: menu bar and main toolbar
```

If the issue persists, bear in mind that _clifm_ uses the `CPR` (cursor position report) [escape code](https://www.xfree86.org/current/ctlseqs.html) to get the current position of the cursor on the screen (which then is passed to the `clifmimg` script to generate the preview). If your terminal does not support `CPR` (most do), you are out of look: just try another terminal.
