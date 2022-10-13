# Image previews

> Because terminals were built to display text rather than images, making them display images can be problematic.  Some solutions work better in some environments than others, but none of them is perfect.

<p align="right"><a href="https://wiki.vifm.info/index.php/How_to_preview_images">Vifm wiki</p>

## Table of contents

* [Description](#description)
* [Usage](#usage)
* [General procedure](#general-procedure)
* [Dependencies](#dependencies)

---

<h4 align="right">TAB completion with image previews</h4>
<p align="right"><img src="https://i.postimg.cc/fTG6W3yb/fzf-preview.jpg"></p>

<h4 align="left">Preview files in full screen (via the <i>view</i> command)</h4>
<p align="left"><img src="https://i.postimg.cc/52PKY6Nv/view-preview.jpg"></p>

---

## Description

Based on [vifmimg](https://github.com/cirala/vifmimg), these scripts ([clifmimg](https://github.com/leo-arch/clifm/blob/master/misc/tools/imgprev/clifmimg) and [clifmrun](https://github.com/leo-arch/clifm/blob/master/misc/tools/imgprev/clifmrun)) are intended to provide image preview capabilities to _CliFM_ using either [ueberzug](https://github.com/seebye/ueberzug) (default) or the [kitty image protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/) (currently experimental and slower). Bear in mind that `ueberzug` only works on X11.

### clifmrun

Launch an instance of **ueberzug**(1) and set the appropriate values (as environment variables) so that it can be used by [shotgun](https://github.com/leo-arch/clifm/wiki/Advanced#shotgun) (clifm's built-in previewer) to display images via `clifmimg` (see below).

Parameters passed to `clifmrun` will be passed to _clifm_ itself.

The kitty image protocol is also supported. See the [Usage section](#usage) below.

### clifmimg

Convert (if necessary) and preview images (thumbnails) via an already running instance of **ueberzug**(1), launched by `clifmrun` (see above).

Thumbnails are cached (in the directory pointed to by the `CACHE_DIR` variable, by default `${XDG_CACHE_HOME:-$HOME/.cache}/clifm/previews`, usually (`$HOME/.cache/clifm/previews`)) using file hashes as names.

It takes two parameters: the first one tells what kind of file is to be converted/displayed. The second one is the file name to be converted/displayed. For example:

```sh
clifmimg doc /path/to/file.docx
```

generates a thumbnail of `file.docx` using the method named `doc`.

The first parameter (thumbnailing method) could be any of the following:

* image
* video
* epub
* pdf
* djvu
* audio
* font
* doc
* postscript
* svg

## Usage

1. Copy both scripts (`clifmrun` and `clifmimg`) to somewhere in you **$PATH** (say `/usr/local/bin`). You can find them in `DATADIR/clifm/plugins` (usually `/usr/local/share/clifm/plugins`).

2. Edit [shotgun's](https://github.com/leo-arch/clifm/wiki/Advanced#shotgun) configuration file (`$HOME/.config/clifm/profiles/PROFILE/preview.clifm`) by pressing <kbd>F7</kbd> or editing the file manually, and add the following lines at the top of the file (to make sure they won't be overriden by previous directives):

```sh
X:^application/.*(officedocument|msword|ms-excel|opendocument).*=clifmimg doc;
X:^text/rtf$=clifmimg doc;
X:^application/epub\+zip$=clifmimg epub;
X:^appliaction/pdf$=clifmimg pdf
X:^image/vnd.djvu=clifmimg djvu
X:^image/svg\+xml$=clifmimg svg;
X:^image/.*=clifmimg image;
X:^video/.*=clifmimg video;
X:^audio/.*=clifmimg audio;
X:^application/postscript$=clifmimg postscript;
X:N:.*\.otf$=clifmimg font;
X:font/.*=clifmimg font;
```

This instructs _clifm_ to use `clifmimg` to generate previews (and display them via `ueberzug`) for the specified file names or file types.

In case you don't want image preview for some of these files types, just comment out the corresponding line or change its value to your preferred previewing application.

3. Run _clifm_ via the `clifmrun` script

```sh
clifmrun --fzfpreview
```

`--fzfpreview` is used to show file previews via [TAB completion](https://github.com/leo-arch/clifm/wiki/Specifics#expansions-completions-and-suggestions) (fzf mode only). However, if the `FfzPreview` option (see the [main configuration file](https://github.com/leo-arch/clifm/blob/master/misc/clifmrc)) is already set to `true`, you can omit `--fzfpreview`

The [`view` command](https://github.com/leo-arch/clifm/wiki/Introduction#view), used to preview files in the current directory in full screen, will enable `FzfPreview` automatically provided the `fzf` binary is found, so that `--fzfpreview` is not requiered here either.

### Kitty and Wayland

If running on the kitty terminal you can force the use of the kitty image protocol (instead of `ueberzug`) as follows:

```sh
CLIFM_KITTY_NO_UEBERZUG=1 clifmrun --fzfpreview
```
Note that on Wayland the kitty image protocol will be used by default, so that there is no need to set this variable.

### General procedure

The steps involved in generating image previews are:

1. `clifmrun` creates an instance of `ueberzug` and then launches _clifm_.
2. Every time TAB completion is invoked for files (if running in fzf mode and `FzfPreview` is enabled), or the [view command](https://github.com/leo-arch/clifm/wiki/Introduction#view) is executed, [**fzf**(1)](https://www.mankier.com/1/fzf) is launched.
3. **fzf**(1) calls shotgun (via `clifm --preview`) to generate a preview of the currently hovered file.
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
