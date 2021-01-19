# Maintainer: archcrack <johndoe.arch@outlook.com>

pkgname=clifm
pkgver=0.22.1
pkgrel=2
pkgdesc="The KISS file manager: cli-based, ultra-lightweight, and lightning fast"
arch=(any)
url="https://github.com/leo-arch/clifm"
license=(GPL2)
depends=('ncurses' 'libcap' 'file')
makedepends=('git')
optdepends=('sshfs: SFTP support' 'cifs-utils: Samba support' 'curlftpfs: FTPFS support')
source=("git+${url}.git")
sha256sums=('SKIP')
#source=("${pkgname}-${pkgver}.tar.gz::https://github.com/leo-arch/clifm/archive/v${pkgver}.tar.gz")
#sha256sums=('d6cae72041303351a5b2655a707a0d81a75b52eb936590c75a7dac691ba65f93')

build() {
  cd "$srcdir/${pkgname}-${pkgver}"
  gcc -O3 -march=native -s -fstack-protector-strong -o "$pkgname" "${pkgname}.c" -lreadline -lacl -lcap
}

package() {
  cd "$srcdir/${pkgname}-${pkgver}"
  install -Dm755 "$pkgname" "$pkgdir/usr/bin/$pkgname"
  install -g 0 -o 0 -Dm644 manpage "$pkgdir/usr/share/man/man1/${pkgname}.1"
  gzip "${pkgdir}/usr/share/man/man1/${pkgname}.1"
  install -g 0 -o 0 -Dm644 "translations/spanish/${pkgname}.mo" "$pkgdir/usr/share/locale/es/LC_MESSAGES/${pkgname}.mo"
}
