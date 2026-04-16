#!/usr/bin/env bash

set -euo pipefail

ICON_DIR="assets/Textures/Icons"
OUT_PNG="${ICON_DIR}/../atlas.png"
OUT_META="${ICON_DIR}/../atlas.txt"
COLS=8
CELL=24

while [ "$#" -gt 0 ]; do
	case "$1" in
	--icon-dir)
		ICON_DIR="$2"
		shift 2
		;;
	--out-png)
		OUT_PNG="$2"
		shift 2
		;;
	--out-meta)
		OUT_META="$2"
		shift 2
		;;
	--cols)
		COLS="$2"
		shift 2
		;;
	--cell)
		CELL="$2"
		shift 2
		;;
	*)
		echo "Unknown argument: $1" >&2
		exit 1
		;;
	esac
done

if [ ! -d "$ICON_DIR" ]; then
	echo "Icon directory does not exist: $ICON_DIR" >&2
	exit 1
fi

if ! command -v magick >/dev/null 2>&1; then
	echo "missing 'magick'." >&2
	exit 1
fi

atlas_name="$(basename "$OUT_PNG")"

files=()
for path in "$ICON_DIR"/*.png; do
	[ -e "$path" ] || continue
	base="$(basename "$path")"
	if [ "$base" = "$atlas_name" ]; then
		continue
	fi
	files+=("$path")
done

if [ "${#files[@]}" -eq 0 ]; then
	echo "No PNG files found in: $ICON_DIR" >&2
	exit 1
fi

mapfile -t files < <(printf '%s\n' "${files[@]}" | sort)

count="${#files[@]}"
rows=$(( (count + COLS - 1) / COLS ))
atlas_width=$(( COLS * CELL ))
atlas_height=$(( rows * CELL ))

mkdir -p "$(dirname "$OUT_PNG")"
mkdir -p "$(dirname "$OUT_META")"

magick -size "${atlas_width}x${atlas_height}" xc:none "$OUT_PNG"

for i in "${!files[@]}"; do
	x=$(( (i % COLS) * CELL ))
	y=$(( (i / COLS) * CELL ))
	magick "$OUT_PNG" \( "${files[$i]}" \
		-resize "${CELL}x${CELL}" \
		-background none \
	\) -gravity northwest \
		-geometry "+${x}+${y}" \
		-composite \
		"$OUT_PNG"
done

orig_alpha="$(mktemp "${OUT_PNG}.origalpha.XXXXXX.png")"
new_alpha="$(mktemp "${OUT_PNG}.newalpha.XXXXXX.png")"

magick "$OUT_PNG" -alpha extract "$orig_alpha"

magick "$OUT_PNG" \
	-background white \
	-alpha remove \
	-colorspace gray \
	-negate \
	"$new_alpha"

magick "$orig_alpha" "$new_alpha" \
	-compose Multiply \
	-composite \
	"$new_alpha"

magick "$OUT_PNG" \
	-alpha off \
	-fill white -colorize 100 \
	"$OUT_PNG"

magick "$OUT_PNG" "$new_alpha" \
	-compose CopyOpacity \
	-composite \
	"$OUT_PNG"

rm -f "$orig_alpha" "$new_alpha"

{
	echo "atlas=$OUT_PNG"
	echo "atlas_width=$atlas_width"
	echo "atlas_height=$atlas_height"
	echo "cell=$CELL"
	echo "columns=$COLS"
	echo "count=$count"
	echo
	echo "# name x y w h"
	for i in "${!files[@]}"; do
		name="$(basename "${files[$i]}" .png)"
		x=$(( (i % COLS) * CELL ))
		y=$(( (i / COLS) * CELL ))
		echo "$name $x $y $CELL $CELL"
	done
} > "$OUT_META"

oxipng -o max -i 0 --strip all "$OUT_PNG"

echo "Wrote atlas: $OUT_PNG"
echo "Wrote metadata: $OUT_META"
