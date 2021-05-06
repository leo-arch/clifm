# Maintainer: archcrack <johndoe.arch@outlook.com>

pkgname=clifm-git
_pkgname=clifm
pkgver=1.0.r148.g05714e7
pkgrel=1
pkgdesc="The KISS file manager: cli-based, ultra-lightweight, and lightning fast (development version)"
arch=('i686' 'pentium4' 'x86_64' 'arm' 'aarch64' 'armv7h')
url="https://github.com/leo-arch/clifm"
license=(GPL2)
provides=('clifm')
depends=('libcap' 'readline' 'acl')
makedepends=('git')
optdepends=(
	'sshfs: SFTP support'
	'cifs-utils: Samba support'
	'curlftpfs: FTPFS support'
	'archivemount: Archives mount'
	'atool: Archives/compression support'
	'p7zip: ISO 9660 support'
	'cdrtools: ISO 9660 support'
	)
source=("git+${url}.git")
sha256sums=('SKIP')

pkgver() {
  cd "$srcdir/$_pkgname"
  git describe --long --tags | sed 's/^v//;s/\([^-]*-g\)/r\1/;s/-/./g'
}

build() {
  cd "$srcdir/$_pkgname/src"
  gcc -O3 -march=native -s -fstack-protector-strong -o "$_pkgname" "${_pkgname}.c" -lreadline -lacl -lcap
}

package() {
  cd "$srcdir/$_pkgname"
  install -Dm755 "src/$_pkgname" "$pkgdir/usr/bin/$_pkgname"
  install -g 0 -o 0 -Dm644 manpage "$pkgdir/usr/share/man/man1/${_pkgname}.1"
  gzip -9n "$pkgdir/usr/share/man/man1/$_pkgname.1"
  install -g 0 -o 0 -Dm644 "${_pkgname}.desktop" "$pkgdir/usr/share/applications/${_pkgname}.desktop"
  install -g 0 -o 0 -Dm644 completions.bash "$pkgdir/usr/share/bash-completion/completions/${_pkgname}"
  install -g 0 -o 0 -Dm644 completions.zsh "$pkgdir/usr/share/zsh/site-functions/_${_pkgname}"
  install -g 0 -o 0 -Dm644 "translations/spanish/${_pkgname}.mo" "$pkgdir/usr/share/locale/es/LC_MESSAGES/${_pkgname}.mo"
  mkdir -p "$pkgdir/usr/share/$_pkgname"
  cp -r plugins "$pkgdir/usr/share/$_pkgname"
  cp -r functions "$pkgdir/usr/share/$_pkgname"
}
