#!/usr/bin/env bash
# One-shot nxdk (original Xbox homebrew SDK) toolchain setup for this
# podman container, written from a live back-and-forth in this session --
# confirmed working end-to-end (built samples/hello to a real default.xbe
# + bootable hello.iso) before being turned into this script.
#
# Unlike the Wii path (devkitPro/pacman), nxdk does NOT go through any
# apt repo of its own -- it's a plain git clone plus the Debian/Ubuntu
# system LLVM packages. This sidesteps both blockers hit on the Wii path:
# no Cloudflare-blocked devkitpro.org domains, and no devkitpro-pacman
# .deb postinst script trying to chown files (the thing that failed with
# "error setting ownership of '/opt/devkitpro.dpkg-new': Invalid argument"
# in this sandbox). A plain `apt-get install clang lld bison flex cmake`
# installs cleanly here.
#
# The one real gap found: Debian's llvm-14/clang-14/lld-14 packages only
# ship VERSIONED binary names (llvm-dlltool-14, llvm-ar-14, etc.) with no
# update-alternatives entries -- nxdk's Makefiles call the UNVERSIONED
# names (llvm-dlltool, llvm-ar, ...). Fixed by symlinking every
# /usr/bin/llvm-*-14 to its unversioned name. clang/clang++ already had
# unversioned binaries (the `clang` meta-package sets those up), so only
# the llvm-* tool names needed this.

set -uo pipefail

NXDK_PARENT="/workspace/neutron-ports-console/xbox"
NXDK_DIR="$NXDK_PARENT/nxdk"

echo "=== [1/4] system prerequisites (clang, lld, bison, flex, cmake, make, git) ==="
apt-get -o APT::Sandbox::User=root update
apt-get -o APT::Sandbox::User=root install -y clang lld bison flex cmake make git
if ! command -v clang >/dev/null || ! command -v ld.lld >/dev/null; then
  echo "FATAL: clang or ld.lld still not found after install" >&2
  exit 1
fi

echo "=== [2/4] unversioned LLVM tool symlinks ==="
# nxdk's Makefiles invoke e.g. "llvm-dlltool", not "llvm-dlltool-14".
linked_any=0
for f in /usr/bin/llvm-*-[0-9]*; do
  [ -e "$f" ] || continue
  base="$(basename "$f" | sed -E 's/-[0-9]+$//')"
  target="/usr/bin/$base"
  if [ ! -e "$target" ]; then
    ln -s "$f" "$target"
    linked_any=1
  fi
done
if [ "$linked_any" -eq 1 ]; then
  echo "Created missing unversioned llvm-* symlinks."
else
  echo "Unversioned llvm-* names already present, nothing to do."
fi
if ! command -v llvm-dlltool >/dev/null; then
  echo "FATAL: llvm-dlltool still not on PATH after symlinking" >&2
  exit 1
fi

echo "=== [3/4] clone nxdk (if not already present) ==="
if [ -d "$NXDK_DIR/.git" ]; then
  echo "nxdk already cloned at $NXDK_DIR, skipping."
else
  git clone --recursive https://github.com/XboxDev/nxdk.git "$NXDK_DIR"
fi

echo "=== [4/4] build the 'hello' sample as a smoke test ==="
export NXDK_DIR="$NXDK_DIR"
export PATH="$NXDK_DIR/bin:$PATH"
make -C "$NXDK_DIR/samples/hello"
HELLO_XBE="$NXDK_DIR/samples/hello/bin/default.xbe"
if [ ! -f "$HELLO_XBE" ]; then
  echo "FATAL: hello sample did not produce bin/default.xbe" >&2
  exit 1
fi

echo
echo "=== Done ==="
echo "nxdk is at: $NXDK_DIR"
echo "Smoke-test build succeeded: $HELLO_XBE"
echo
echo "To build anything else, in a NEW shell run:"
echo "  export NXDK_DIR=\"$NXDK_DIR\""
echo "  export PATH=\"\$NXDK_DIR/bin:\$PATH\""
echo "  make -C <your-project-dir>"
echo "(bin/activate does the same two exports, but don't execute nxdk's own"
echo " script directly from an environment whose Bash permissions flag"
echo " running freshly-downloaded scripts -- set the two exports by hand"
echo " instead, as this script does.)"
