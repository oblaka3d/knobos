#!/usr/bin/env bash
# Регенерация шрифтов для S3 (466x466) из Montserrat-Regular.ttf через lv_font_conv.
# Нужен только при изменении набора размеров/диапазона символов — сгенерённые
# components/ui/fonts/font_m*.c коммитятся в репозиторий.
#
# Использование: tools/gen_fonts.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="${SCRIPT_DIR}/../components/ui/fonts"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

TTF="${TMP_DIR}/Montserrat-Regular.ttf"
FONT_URL="https://github.com/JulietaUla/Montserrat/raw/master/fonts/ttf/Montserrat-Regular.ttf"
# Вариативный Montserrat[wght].ttf с google/fonts lv_font_conv не переваривает —
# используется статический вес Regular из репозитория шрифта.
curl -fsSL -o "${TTF}" "${FONT_URL}"

RANGE="0x20-0x7F,0xB0"

gen() {
    local size="$1" name="$2"
    npx --yes lv_font_conv@1.5.3 \
        --font "${TTF}" --size "${size}" --bpp 4 --range "${RANGE}" \
        --format lvgl -o "${OUT_DIR}/${name}.c"
}

gen 32 font_m32
gen 40 font_m40
gen 56 font_m56
gen 96 font_m96
