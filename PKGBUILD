# Maintainer: Necirvan <ibikhan2525@gmail.com>
pkgname=meow
pkgver=0.1.0
pkgrel=1
pkgdesc="Prints cat pictures in your terminal"
arch=('x86_64' 'aarch64')
url="https://github.com/NecirvanA/meow"
license=('MIT')
depends=('curl')
makedepends=('cmake' 'gcc')
optdepends=('kitty: image rendering via the icat kitten'
            'wezterm: image rendering via imgcat'
            'viu: image rendering fallback'
            'chafa: image rendering fallback')
source=("$pkgname-$pkgver.tar.gz::https://github.com/NecirvanA/meow/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('430791589693c257d50d2f0b4f1b06036ee5c608b24b869a2bbdcbc0517841bb')

build() {
  cd "$pkgname-$pkgver"
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build
}

package() {
  cd "$pkgname-$pkgver"
  install -Dm755 build/meow "$pkgdir/usr/bin/meow"
  install -Dm644 README.md "$pkgdir/usr/share/doc/$pkgname/README.md"
}
