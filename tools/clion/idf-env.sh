# tools/clion/idf-env.sh -- put an ESP-IDF toolchain on PATH for a
# NON-interactive shell.  Source this; do not run it.
#
# The CLion wrappers in this directory start OpenOCD and GDB from a clean,
# non-interactive shell, where no IDF has been activated.  EIM's activation
# script refuses to be sourced from a script (it checks $0 and exits), so ask
# it for its environment with `-e` and export the parts the tools need.
#
# Selection:
#   IDF_ACTIVATE=/path/activate.sh  use this IDF activation script
#   IDF_VERSION=x.y                 use ~/.espressif/tools/activate_idf_vx.y.sh
#   (default)                       IDF 6.1, which this tree is built with

IDF_VERSION="${IDF_VERSION:-6.1}"
IDF_ACTIVATE="${IDF_ACTIVATE:-$HOME/.espressif/tools/activate_idf_v${IDF_VERSION}.sh}"

if [ ! -f "$IDF_ACTIVATE" ]; then
    echo "error: no IDF activation script at $IDF_ACTIVATE" >&2
    echo "       set IDF_VERSION or IDF_ACTIVATE; available:" >&2
    ls "$HOME/.espressif/tools/" 2>/dev/null | grep '^activate_idf' >&2
    return 1 2>/dev/null || exit 1
fi

while IFS= read -r _line; do
    _key="${_line%%=*}"
    _val="${_line#*=}"
    case "$_key" in
        IDF_PATH|IDF_TOOLS_PATH|IDF_PYTHON_ENV_PATH|ESP_ROM_ELF_DIR| \
        OPENOCD_SCRIPTS|IDF_COMPONENT_LOCAL_STORAGE_URL|ESP_IDF_VERSION)
            export "$_key=$_val"
            ;;
        PATH)
            PATH="$_val:$PATH"
            export PATH
            ;;
    esac
done <<EOT
$(sh "$IDF_ACTIVATE" -e)
EOT
unset _line _key _val

if [ -z "${IDF_PATH:-}" ]; then
    echo "error: $IDF_ACTIVATE did not report an IDF_PATH" >&2
    return 1 2>/dev/null || exit 1
fi
