# Source this before running any tool: keeps all tool state inside the project.
SH_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export SH_ROOT
T="$SH_ROOT/tools"
export HOME="$T/home"
export XDG_CONFIG_HOME="$HOME/.config" XDG_CACHE_HOME="$HOME/.cache" XDG_DATA_HOME="$HOME/.local/share" XDG_STATE_HOME="$HOME/.local/state"
export PIP_CACHE_DIR="$XDG_CACHE_HOME/pip" PYTHONPYCACHEPREFIX="$XDG_CACHE_HOME/pycache"
export TMPDIR="$SH_ROOT/scratch/tmp"; mkdir -p "$TMPDIR"
export JAVA_HOME="$T/jdk-21.0.12.1+1"
export JAVA_TOOL_OPTIONS="-Duser.home=$HOME -Djava.io.tmpdir=$TMPDIR"
export WINEPREFIX="$T/wineprefix" WINEDEBUG=-all
# Laptop: tools/venv + build/ + tools/sdl3-linux. JARVIS workers (other Python, no X11
# headers): tools/venv-worker + build-w/ + tools/sdl3-worker (headless SDL3 only).
SH_VENV=$T/venv SH_BUILD=${SH_BUILD:-$SH_ROOT/build} SH_SDL3=$T/sdl3-linux
if [ -x "$T/venv-worker/bin/python3" ] && ! grep -qx "version = $(python3 -c 'import platform; print(platform.python_version())' 2>/dev/null)" "$T/venv/pyvenv.cfg" 2>/dev/null; then
   SH_VENV=$T/venv-worker SH_SDL3=$T/sdl3-worker
   [ "$SH_BUILD" = "$SH_ROOT/build" ] && SH_BUILD=$SH_ROOT/build-w
fi
export SH_VENV SH_BUILD SH_SDL3
export PATH="$T/bin:$SH_VENV/bin:$T/cmake-4.4.4-linux-x86_64/bin:$T/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64/bin:$T/wine-11.19-amd64/bin:$JAVA_HOME/bin:$PATH"
