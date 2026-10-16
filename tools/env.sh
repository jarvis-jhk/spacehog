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
export PATH="$T/bin:$T/venv/bin:$T/cmake-4.4.4-linux-x86_64/bin:$T/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64/bin:$T/wine-11.19-amd64/bin:$JAVA_HOME/bin:$PATH"
