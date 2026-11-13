# Shared helpers, sourced by the sk-* scripts. POSIX sh.
#
# The instance (your private config) lives in a directory with:
#   sk-vdisplay.conf   HOST_LABEL=..., ANCHOR=eDP-1, AUDIO_SCREENS="tv" (or all / none)
#   screens.conf       one virtual screen per line (see examples/instance)
#   cams/<name>.env    one camera/mic source per file
# Its path is the symlink ~/.config/sk-vdisplay/instance (made by install.sh)
# or $SK_VDISPLAY_INSTANCE_DIR.

SKV_INSTANCE="${SK_VDISPLAY_INSTANCE_DIR:-$HOME/.config/sk-vdisplay/instance}"
SKV_STATE="$HOME/.config/sk-vdisplay"
export DISPLAY="${DISPLAY:-:0}"

skv_die() { echo "sk-vdisplay: $*" >&2; exit 1; }

skv_load_conf() {
	[ -f "$SKV_INSTANCE/sk-vdisplay.conf" ] || skv_die "no instance config at $SKV_INSTANCE/sk-vdisplay.conf"
	# shellcheck disable=SC1091
	. "$SKV_INSTANCE/sk-vdisplay.conf"
	HOST_LABEL="${HOST_LABEL:-$(hostname -s)}"
	ANCHOR="${ANCHOR:-eDP-1}"
	AUDIO_SCREENS="${AUDIO_SCREENS-all}"
}

# Print the screens.conf line for a screen name: "name mode position port"
skv_screen() {
	awk -v n="$1" '!/^[[:space:]]*#/ && NF >= 4 && $1 == n {print; exit}' "$SKV_INSTANCE/screens.conf"
}

# All active screen lines (comments and blanks dropped)
skv_screens() { awk '!/^[[:space:]]*#/ && NF >= 4' "$SKV_INSTANCE/screens.conf"; }

# "1920x1080@60" -> "1920x1080"
skv_size() { echo "${1%@*}"; }

# X output currently showing a virtual (evdi, DVI-I-*) monitor of this size.
# Sizes must be unique across screens.conf: that is how outputs are matched,
# because evdi connector names (DVI-I-1-1, DVI-I-2-2, ...) depend on start order.
skv_output_for_size() {
	xrandr | awk -v s="$1" '$0 ~ "^DVI-I.* connected (primary )?" s "\\+" {print $1; exit}'
}
