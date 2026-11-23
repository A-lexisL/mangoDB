#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
data_dir="$ROOT_DIR/test/data"
build_dir="${BUILD_DIR:-$ROOT_DIR/build}"
source "$ROOT_DIR/test/result-links.sh"
if (( $# < 2 )); then
    echo "Usage: $0 {mangoDB|mangoDB-val|mangoDB-asan|mangoDB-msan|mangoDB-ubsan} {0|1}/case.query [...]" >&2
    exit 2
fi
target=$1
shift
case "$target" in
    mangoDB|mangoDB-val|mangoDB-asan|mangoDB-msan|mangoDB-ubsan) ;;
    *) echo "ERROR: unknown executable: $target" >&2; exit 2 ;;
esac
DIFF_LINES="${DIFF_LINES:-40}"
if [[ ! "$DIFF_LINES" =~ ^[0-9]+$ ]] || (( DIFF_LINES < 1 )); then
    echo "ERROR: DIFF_LINES must be a positive integer" >&2
    exit 1
fi
if [[ "$build_dir" != /* ]]; then
    build_dir="$ROOT_DIR/$build_dir"
fi

result_root="$ROOT_DIR/test/test-results/$target"
trap 'exit 130' INT
trap 'exit 143' TERM

queries=()
for query_file in "$@"; do
    if [[ ! "$query_file" =~ ^[01]/[[:alnum:]][[:alnum:]_.-]*\.query$ ]]; then
        echo "ERROR: invalid query filename: $query_file" >&2
        exit 2
    fi
    track="${query_file%%/*}"
    query="$data_dir/$track/queries/${query_file#*/}"
    if [[ ! -f "$query" ]]; then
        echo "ERROR: query file not found: $query" >&2
        exit 1
    fi
    queries+=("$query")
done

compare_files() {
    local expected=$1 actual=$2 report=$3 label=$4
    if [[ ! -f "$expected" ]]; then
        echo "FAIL $label: missing expected file: $expected" >&2
        return 1
    fi
    if [[ ! -f "$actual" ]]; then
        echo "FAIL $label: missing result file: $actual" >&2
        return 1
    fi
    if [[ ${5:-} == table ]]; then
        if ! validate_table "$expected" || ! validate_table "$actual"; then
            echo "FAIL $label: table has an invalid column count" >&2
            return 1
        fi
        if diff -u --label "$expected" --label "$actual" \
            <(sorted_table "$expected") <(sorted_table "$actual") > "$report" 2>&1; then
            return 0
        fi
    else
        if diff -u -- "$expected" "$actual" > "$report" 2>&1; then
            return 0
        fi
    fi
    echo "FAIL $label: files differ (full diff: $report)" >&2
    return 1
}

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
    # Preserve field order and values; ignore spacing and record order.
    head -n 2 -- "$file" | awk '{$1 = $1; print}'
    tail -n +3 -- "$file" | awk '{$1 = $1; print}' | LC_ALL=C sort -k1,1
}

binary="$build_dir/$target"
if [[ ! -x "$binary" ]]; then
    echo "ERROR: $target executable not found; build it with cmake --build $build_dir --target $target" >&2
    exit 1
fi
command=("$binary" --listen /dev/stdin)
if [[ "$target" == mangoDB-val ]]; then
    command=(valgrind --tool=memcheck --leak-check=full \
        --track-origins=yes --error-exitcode=1 "${command[@]}")
fi

passed=0
failed=0

for query in "${queries[@]}"; do
    track="$(basename -- "$(dirname -- "$(dirname -- "$query")")")"
    query_file="$(basename -- "$query")"
    name="${query_file%.query}"
    expected="$data_dir/$track/stdout/$name.out"
    expected_dump_dir="$data_dir/$track/dump/$name"
    run_dir="$result_root/$track"
    stdout_file="$run_dir/stdout/$name.stdout"
    stderr_file="$run_dir/stderr/$name/stderr"
    diff_dir="$run_dir/diff/$name"
    for output_dir in tmp stdout stderr diff; do
        link_result_directory "$run_dir/$output_dir" \
            "/tmp/$(id -un)/test-results/$target/$track/$output_dir"
    done
    mkdir -p "$run_dir/db" "$run_dir/tmp/$name" \
        "$run_dir/stdout" "$run_dir/stderr/$name"
    mkdir -p "$diff_dir"
    ln -sfnT -- "$data_dir/$track/db/$name" "$run_dir/db/$name"

    if [[ ! -f "$expected" ]]; then
        echo "FAIL $track/$name: missing expected stdout: $expected" >&2
        failed=$((failed + 1))
        continue
    fi

    mapfile -t dumps < <(sed -nE \
        's@^[[:space:]]*DUMP[[:space:]]+[^[:space:]]+[[:space:]]+([^[:space:];]+)[[:space:]]*;.*$@\1@p' \
        "$query" | LC_ALL=C sort -u)
    shopt -s nullglob
    expected_dumps=("$expected_dump_dir"/*.tbl)
    shopt -u nullglob
    if (( ${#dumps[@]} != ${#expected_dumps[@]} )); then
        echo "FAIL $track/$name: query has ${#dumps[@]} DUMP paths, expected ${#expected_dumps[@]} tables" >&2
        failed=$((failed + 1))
        continue
    fi
    valid_paths=1
    for dump in "${dumps[@]}"; do
        if [[ ! "$dump" =~ ^tmp/[[:alnum:]_.-]+/[[:alnum:]_.-]+\.tbl$ ]] ||
           [[ "$dump" != "tmp/$name/"* ]]; then
            echo "FAIL $track/$name: DUMP path must be inside tmp/$name/: $dump" >&2
            valid_paths=0
            continue
        fi
        rm -f -- "$run_dir/$dump"
    done
    if (( ! valid_paths )); then
        failed=$((failed + 1))
        continue
    fi
    echo "Testing $track/$name with $target"
    if (cd "$run_dir" && "${command[@]}" < "$query" \
        > "$stdout_file" 2> "$stderr_file"); then
        status=0
    else
        status=$?
    fi
    case_failed=0
    if [[ ! -s "$stdout_file" ]]; then
        echo "FAIL $track/$name: empty stdout (exit status $status)" >&2
        sed -n '1,30p' "$stderr_file" >&2
        case_failed=1
    fi
    if (( status != 0 )); then
        echo "FAIL $track/$name: $target exited unsuccessfully" >&2
        sed -n '1,30p' "$stderr_file" >&2
        case_failed=1
    fi
    if ! compare_files "$expected" "$stdout_file" \
        "$diff_dir/stdout.diff" "$track/$name stdout"; then
        sed -n "1,${DIFF_LINES}p" "$diff_dir/stdout.diff" >&2
        case_failed=1
    fi
    for dump in "${dumps[@]}"; do
        if ! compare_files "$expected_dump_dir/$(basename -- "$dump")" \
            "$run_dir/$dump" "$diff_dir/$(basename -- "$dump").diff" \
            "$track/$name table $dump" table; then
            # sed -n "1,${DIFF_LINES}p" "$result_dir/$(basename -- "$dump").diff" >&2
            case_failed=1
        fi
    done
    if (( case_failed == 0 )); then
        echo "PASS $track/$name"
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi
done

echo "$target: $passed passed, $failed failed"
(( failed == 0 ))
