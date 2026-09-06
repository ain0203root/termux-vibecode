TERMUX_PKG_HOMEPAGE=https://github.com/ain0203root/termux-vibecode
TERMUX_PKG_DESCRIPTION="Native high-performance shell for Termux VibeCode"
TERMUX_PKG_LICENSE="Apache-2.0"
TERMUX_PKG_MAINTAINER="@ain0203root"
TERMUX_PKG_VERSION="0.1.0"
TERMUX_PKG_SRCURL="file:///home/builder/termux-packages/sources/tvsh"
TERMUX_PKG_SHA256=SKIP_CHECKSUM
TERMUX_PKG_BUILD_IN_SRC=true
TERMUX_PKG_DEPENDS="libc++"

termux_step_make() {
    make CC="$CC" CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS" vsh
}

termux_step_make_install() {
    install -Dm755 vsh "$TERMUX_PREFIX/bin/vsh"
    install -Dm644 "$TERMUX_PKG_BUILDER_DIR/README" "$TERMUX_PREFIX/share/doc/tvsh/README"
}
