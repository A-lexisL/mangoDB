#!/usr/bin/env bash
set -euo pipefail

if (( $# != 1 )) || [[ ! $1 =~ ^[A-Za-z0-9_-]+$ ]]; then
    echo "Usage: $0 <custom-case-name>" >&2
    exit 2
fi

case_name=$1
root_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
case_dir="$root_dir/test/custom"
binary="$root_dir/build/mangoDB"
source "$root_dir/test/result-links.sh"

if [[ ! -x "$binary" ]]; then
    echo "ERROR: executable not found: $binary" >&2
    exit 1
fi
for required in "$case_dir/queries/$case_name.query" \
                "$case_dir/stdout/$case_name.out"; do
    if [[ ! -f "$required" ]]; then
        echo "ERROR: fixture not found: $required" >&2
        exit 1
    fi
done

link_result_directory "$case_dir/tmp" \
    "/tmp/${USER:?USER is not set}/custom-results"
cd "$case_dir"
shopt -s nullglob
expected_dumps=(dump/"$case_name"_*.tbl)
if grep -Eq '^[[:space:]]*DUMP[[:space:]]' "queries/$case_name.query" &&
   (( ${#expected_dumps[@]} == 0 )); then
    echo "ERROR: no expected dump for $case_name" >&2
    exit 1
fi

validate_table() {
    awk '
        NR == 1 {
            if (NF != 2 || $2 !~ /^[0-9]+$/) exit 1
            width = $2
            next
        }
        NF != width { exit 1 }
        END { if (NR < 2) exit 1 }
    ' "$1"
}

sorted_table() {
    local file=$1
    head -n 2 -- "$file" | awk '{$1 = $1; print}'
    tail -n +3 -- "$file" | awk '{$1 = $1; print}' | LC_ALL=C sort -k1,1
}

for expected in "${expected_dumps[@]}"; do
    file_name=${expected##*/}
    actual="tmp/${file_name#${case_name}_}"
    : > "$actual"
done

"$binary" --listen "queries/$case_name.query" > "tmp/$case_name.out"
diff -u "stdout/$case_name.out" "tmp/$case_name.out"
for expected in "${expected_dumps[@]}"; do
    file_name=${expected##*/}
    actual="tmp/${file_name#${case_name}_}"
    if ! validate_table "$expected" || ! validate_table "$actual"; then
        echo "FAIL $case_name: invalid table column count: $file_name" >&2
        exit 1
    fi
    diff -u --label "$expected" --label "$actual" \
        <(sorted_table "$expected") <(sorted_table "$actual")
done
echo "PASS $case_name"
