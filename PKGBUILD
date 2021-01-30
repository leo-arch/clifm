# Maintainer: archcrack <johndoe.arch@outlook.com>

pkgname=clifm-git
_pkgname=clifm
pkgver=0.27.2
pkgrel=1
pkgdesc="The KISS file manager: cli-based, ultra-lightweight, and lightning fast (development version)"
arch=(any)
url="https://github.com/leo-arch/clifm"
license=(GPL2)
depends=('libcap' 'readline' 'acl')
makedepends=('git')
optdepends=(
	'sshfs: SFTP support'
	'cifs-utils: Samba support'
	'curlftpfs: FTPFS support'
	'archivemount: Archives mount'
	'atool: Archives/compression support'
	)
source=("git+${url}.git")
sha256sums=('SKIP')

pkgver() {
  cd "$srcdir/$_pkgname"
  git describe --long --tags | sed 's/^v//;s/\([^-]*-g\)/r\1/;s/-/./g'
}

build() {
  cd "$srcdir/$_pkgname"
  gcc -O3 -march=native -s -fstack-protector-strong -o "$_pkgname" "${_pkgname}.c" -lreadline -lacl -lcap
}

package() {
  cd "$srcdir/$_pkgname"
  install -Dm755 "$_pkgname" "$pkgdir/usr/bin/$_pkgname"
  install -g 0 -o 0 -Dm644 manpage "$pkgdir/usr/share/man/man1/${_pkgname}.1"
  gzip "$pkgdir/usr/share/man/man1/$_pkgname.1"
  install -g 0 -o 0 -Dm644 "translations/spanish/${_pkgname}.mo" "$pkgdir/usr/share/locale/es/LC_MESSAGES/${_pkgname}.mo"
}
