#!/usr/bin/env bash

set -euo pipefail
IFS=$'\n\t'

if [[ $# -eq 0 ]]; then
    echo "Usage: ./test.sh <file>"
    echo "ERROR: No file was provided"
    exit 1
fi

if [[ ! -e $1 ]]; then
    echo "Usage: ./test.sh <file>"
    echo "ERROR: file \`$1\` does not exist"
    exit 1
fi

encoded="encoded.txt"
decoded="decoded.txt"

./main "$1" -o "$encoded"
./main -d "$encoded" -o "$decoded"
diff "$1" "$decoded"

echo "Success!"
