# Maintainer: archcrack <johndoe.arch@outlook.com>

pkgname=clifm
pkgver=0.21.5
pkgrel=1
pkgdesc="The KISS file manager: cli-based, ultra-lightweight, and lightning fast"
arch=(any)
url="https://github.com/leo-arch/clifm"
license=(GPL2)
depends=('glibc' 'ncurses' 'libcap' 'readline' 'coreutils' 'file')
makedepends=('git')
optdepends=('sshfs: SFTP support' 'cifs-utils: Samba support' 'curlftpfs: FTPFS support')
source=("git+${url}.git")
sha256sums=('SKIP')

build() {
  cd "$srcdir/$pkgname"
  gcc -O3 -march=native -s -fstack-protector-strong -lcap -lreadline -lacl -o "$pkgname" "${pkgname}.c"
}

package() {
  cd "$srcdir/$pkgname"
  install -Dm755 "$pkgname" "$pkgdir/usr/bin/$pkgname"
  install -g 0 -o 0 -Dm644 manpage "$pkgdir/usr/share/man/man1/${pkgname}.1"
  gzip "${pkgdir}/usr/share/man/man1/${pkgname}.1"
  install -g 0 -o 0 -Dm644 "translations/spanish/${pkgname}.mo" "$pkgdir/usr/share/locale/es/LC_MESSAGES/${pkgname}.mo"
}
