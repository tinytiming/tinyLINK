# tinyLINK shell helpers. Source from ~/.zshrc or ~/.bashrc:
#   . /path/to/tinyLINK/tools/tinylink.sh
#
#   tlport [serial]   list a tinyLINK's serial ports (first one found without a serial)
#   tl [serial]       open its console; Ctrl-D quits (tlcon.py), or picocom (Ctrl-A Ctrl-X)
#
# A serial matches by prefix, so the board id alone works in every mode.

if [ -n "$BASH_VERSION" ]; then
  _tl_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
elif [ -n "$ZSH_VERSION" ]; then
  eval '_tl_dir=${${(%):-%x}:A:h}'
fi

# One line per port: <serial> <device> <interface name>
_tl_ports() {
  if [ "$(uname)" = Darwin ]; then
    ioreg -r -c IOUSBHostDevice -l -w0 | awk '
      /<class IOUSBHostDevice/ { ser = ""; vid = ""; pid = "" }
      /"idVendor" = / { vid = $NF }
      /"idProduct" = / { pid = $NF }
      /"USB Serial Number" = / && ser == "" { split($0, a, "= "); ser = a[2]; gsub(/"/, "", ser) }
      /"kUSBString"/ { split($0, a, "= "); name = a[2]; gsub(/"/, "", name) }
      /"IOCalloutDevice"/ && vid == 4617 && pid == 1 {
        split($0, a, "= "); d = a[2]; gsub(/"/, "", d); print ser, d, name
      }'
  else
    for p in /dev/serial/by-id/usb-tinyLINK_*-if*; do
      [ -e "$p" ] || continue
      s=${p##*_}
      echo "${s%-if*} $p if${p##*-if}"
    done
  fi
}

tlport() {
  _tl_all=$(_tl_ports)
  if [ -z "$_tl_all" ]; then
    echo "no tinyLINK found" >&2
    return 1
  fi
  _tl_ser=$(echo "$_tl_all" | awk -v s="$1" 'index($1, s) == 1 { print $1; exit }')
  if [ -z "$_tl_ser" ]; then
    echo "no tinyLINK $1 (found: $(echo "$_tl_all" | awk '{print $1}' | sort -u | paste -sd ' ' -))" >&2
    return 1
  fi
  if [ -z "$1" ]; then
    _tl_other=$(echo "$_tl_all" | awk -v s="$_tl_ser" '$1 != s { print $1 }' | sort -u | paste -sd ' ' -)
    [ -n "$_tl_other" ] && echo "using $_tl_ser; also connected: $_tl_other" >&2
  fi
  echo "$_tl_all" | awk -v s="$_tl_ser" '$1 == s { $1 = ""; sub(/^ /, ""); print }'
}

tl() {
  _tl_list=$(tlport "$1") || return 1
  if [ "$(uname)" = Darwin ]; then
    _tl_con=$(echo "$_tl_list" | awk '/console/ { print $1; exit }')
  else
    _tl_con=$(echo "$_tl_list" | sort -k 2 | awk 'END { print $1 }')
  fi
  if [ -z "$_tl_con" ]; then
    echo "no console port found" >&2
    return 1
  fi
  if command -v python3 >/dev/null 2>&1 && [ -f "$_tl_dir/tlcon.py" ]; then
    python3 "$_tl_dir/tlcon.py" "$_tl_con"
  else
    echo "$_tl_con (Ctrl-A Ctrl-X quits)"
    picocom -q "$_tl_con"
  fi
}
