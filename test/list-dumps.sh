#!/usr/bin/env bash
set -euo pipefail

if [[ "${1:-}" == --help || "${1:-}" == -h ]]; then
    echo "Usage: $0 [QUERY_FILE ...]"
    echo "Print expected dump paths to stdout, one per line."
    echo "Default: test/data/{0,1}/queries/*.query"
    exit 0
fi

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
data_dir="$ROOT_DIR/test/data"
if (( $# > 0 )); then
    queries=("$@")
else
    shopt -s nullglob
    queries=("$data_dir"/{0,1}/queries/*.query)
fi
if (( ${#queries[@]} == 0 )); then
    echo "ERROR: no query files selected" >&2
    exit 1
fi

list_dumps() {
    local query track name dump
    for query in "${queries[@]}"; do
        track="$(basename -- "$(dirname -- "$(dirname -- "$query")")")"
        name="$(basename -- "$query" .query)"
        if [[ ! -f "$query" || ! "$track" =~ ^[01]$ ||
              "$query" != *.query ]]; then
            echo "ERROR: invalid query file: $query" >&2
            return 1
        fi
        while IFS= read -r dump; do
            printf '%s/%s/dump/%s/%s\n' \
                "$data_dir" "$track" "$name" "${dump##*/}"
        done < <(sed -nE \
            's@^[[:space:]]*DUMP[[:space:]]+[^[:space:]]+[[:space:]]+([^[:space:];]+)[[:space:]]*;.*$@\1@p' \
            "$query")
    done
}

list_dumps | LC_ALL=C sort -u
