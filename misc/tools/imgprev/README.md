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

1. Run `view edit` (<kbd>F7</kbd> is also available) to edit [shotgun's](https://github.com/leo-arch/clifm/wiki/Advanced#shotgun) configuration file (`$HOME/.config/clifm/profiles/PROFILE/preview.clifm`), and uncomment the following lines from the top of the file:

```sh
...
# Uncomment and edit this line to use Ranger's scope script: 
#.*=/home/USER/.config/ranger/scope.sh %f 120 80 /tmp/clifm/ True;

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

This instructs **clifm** to use the [clifmimg script](https://github.com/leo-arch/clifm/blob/master/misc/tools/imgprev/clifmimg) to generate previews for the specified file names or file types (both for TAB completion (in [fzf mode](https://github.com/leo-arch/clifm/wiki/Specifics#tab-completion)) and via the [`view` command](https://github.com/leo-arch/clifm/wiki/Introduction#view)).

In case you don't want image preview for some of these files types, just comment out the corresponding line or change its value to your preferred previewing application.

2. If using either the `ueberzug` or the `kitty` [methods](#previewing-methods), run **clifm** via the [clifmrun script](https://github.com/leo-arch/clifm/blob/master/misc/tools/imgprev/clifmrun) (you can find it under `~/.config/clifm/` or `DATADIR/clifm/plugins/` (usually, `/usr/local/share/clifm/plugins/` or `/usr/share/clifm/plugins`). Otherwise, run **clifm** as usual.

### Previewing methods

Available previewing methods are: `sixel`, `ueberzug`, `kitty`, and `ascii`.

The previwing method is controlled by the `method` variable in the `clifmimg` script. By default, this variable is unset, meaning that **clifm** will try to guess the available method. To manually choose a method, set the `method` variable to any of the available methods.

If using either `ueberzug`<sup>1</sup> or `kitty` methods, you must run **clifm** via the `clifmrun` script.

If using rather the `ascii` method, several applications to generate ASCII previews are available: `chafa`, `pixterm`, `img2text`, `viu`, `catimg`, `tiv`, and `timg`. Use the `ascii_method` variable in the `clifmimg` script to set you preferred application. It defaults to `chafa`.

In the case of the `sixel` method, **chafa**(1) is used to generate sixel images.

<sup>1</sup> Since the original `ueberzug` is not maintained anymore, we recommend using this fork instead: https://github.com/ueber-devel/ueberzug.

### Kitty and Wayland

If running on the kitty terminal you can force the use of the kitty image protocol (instead of `ueberzug`) as follows:

```sh
CLIFM_KITTY_NO_UEBERZUG=1 clifmrun
```
Note that on Wayland the kitty image protocol will be used by default, so that there is no need to set this variable.

## General procedure

The steps involved in generating image previews are:

1. The `clifmrun` script prepares the environment to generate image previews (via either `ueberzug` or `kitty`) and then launches **clifm**.<sup>1</sup>
2. Every time TAB completion is invoked for files (if running in [fzf mode](https://github.com/leo-arch/clifm/wiki/Specifics#tab-completion)), or the [view command](https://github.com/leo-arch/clifm/wiki/Introduction#view) is executed, `fzf` is launched.
3. `fzf` calls shotgun (via `clifm --preview`) to generate a preview of the currently hovered file.
4. Shotgun executes `clifmimg`, which takes care of genereting a thumbnail of the corresponding file.
5. Once the thumbnail is generated, `clifmimg` takes care of disaplying the thumbnail via any of the available [previewing methods](#previewing-methods).

<sup>1</sup> Parameters passed to `clifmrun` will be passed to **clifm** itself.

### The clifmimg script

This script converts (if necessary) and generates image previews (as thumbnails) for files.

For performance reasons, thumbnails are cached (in the directory pointed to by the `CACHE_DIR` variable, by default `${XDG_CACHE_HOME:-$HOME/.cache}/clifm/previews`, usually (`~/.cache/clifm/previews`)) using file hashes as names.

It takes two parameters: the first one tells the type of file is to be previewed. The second one is the file name to be previewed. For example:

```sh
clifmimg doc /path/to/file.docx
```

generates a thumbnail of `file.docx` using the method named `doc`.

The first parameter (thumbnailing method) can be any of the following: `image`, `video`, `audio`, `gif`,  `svg`, `epub`, `mobi`, `pdf`, `djvu`, `doc`, `postscript`, and `font`.

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
