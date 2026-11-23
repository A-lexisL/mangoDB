#!/usr/bin/env bash

link_result_directory() {
    local link_path=$1 target_path=$2 backup_path
    mkdir -p -- "${link_path%/*}" "${target_path%/*}"

    if [[ -L "$link_path" ]]; then
        if [[ "$(readlink -- "$link_path")" != "$target_path" ]]; then
            echo "ERROR: $link_path points to a different location" >&2
            return 1
        fi
    elif [[ -e "$link_path" ]]; then
        if [[ ! -d "$link_path" ]]; then
            echo "ERROR: $link_path is not a directory" >&2
            return 1
        fi
        backup_path="${target_path}.previous.$$"
        while [[ -e "$backup_path" || -L "$backup_path" ]]; do
            backup_path="${backup_path}.previous"
        done
        mv -- "$link_path" "$backup_path"
        echo "Moved previous results to $backup_path" >&2
    fi

    mkdir -p -- "$target_path"
    if [[ ! -L "$link_path" ]]; then
        ln -s -- "$target_path" "$link_path"
    fi
}
