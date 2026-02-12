#!/usr/bin/env bash
# Build pfind and spfind, set up tests/fixtures with known permissions,
# then run pfind and spfind and show sample output.
set -e
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

echo "=== Building ==="
make clean
make all

echo ""
echo "=== Setting up tests/fixtures ==="
FIXTURES="$REPO_ROOT/tests/fixtures"
mkdir -p "$FIXTURES/subdir1" "$FIXTURES/subdir2"
touch "$FIXTURES/subdir1/file1" "$FIXTURES/subdir1/file2"
touch "$FIXTURES/subdir2/file1" "$FIXTURES/subdir2/file2"
# Permissions matching the project spec:
# subdir1: drwxrwxrwx (0777), subdir2: drwxr-xr-x (0755)
# subdir1/file1: ---------- (000), subdir1/file2: -rw-r--r-- (0644)
# subdir2/file1, file2: -rw-r--r-- (0644)
chmod 0777 "$FIXTURES/subdir1"
chmod 0755 "$FIXTURES/subdir2"
chmod 000 "$FIXTURES/subdir1/file1"
chmod 644 "$FIXTURES/subdir1/file2"
chmod 644 "$FIXTURES/subdir2/file1" "$FIXTURES/subdir2/file2"

echo ""
echo "=== pfind: files with rw-r--r-- (644) ==="
./pfind -d "$FIXTURES" -p rw-r--r--
echo "(exit code: $?)"

echo ""
echo "=== spfind: same search, sorted output + total ==="
./spfind -d "$FIXTURES" -p rw-r--r--
echo "(exit code: $?)"

echo ""
echo "=== pfind -h ==="
./pfind -h

echo ""
echo "=== spfind with invalid permission string (expect error) ==="
./spfind -d "$FIXTURES" -p rwxr-xr-q 2>&1 || true

echo ""
echo "OK: tests completed."
