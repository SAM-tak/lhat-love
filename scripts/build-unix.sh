#!/usr/bin/env bash
# Run from the Actions workspace containing lhat-love, lhat and deps.
set -euo pipefail
workspace=$PWD
repo=$workspace/lhat-love
platform=${PACKAGE_PLATFORM:?Set PACKAGE_PLATFORM to linux-x64, macos-arm64 or macos-x64}
prefix="$workspace/deps${CMAKE_PREFIX_PATH:+;$CMAKE_PREFIX_PATH}"
info="$workspace/build-info-$platform.txt"
mkdir -p "$workspace/dist"
{
  echo "platform    $platform"
  echo "lhat-love   $(git -C "$repo" rev-parse HEAD)"
  echo "lhat        $(git -C "$workspace/lhat" rev-parse HEAD)"
  echo "SDL         $(git -C "$workspace/SDL" rev-parse HEAD)"
  if [[ $(uname -s) == Darwin ]]; then
    sw_vers
  else
    cat /etc/os-release
  fi
} > "$info"

for variant in relwithdebinfo vmonly-shipping; do
  build="$workspace/build-$variant"
  options=(-DCMAKE_BUILD_TYPE=RelWithDebInfo -DLHATOVE_VM_ONLY=OFF -DLHATOVE_WITH_DAP=ON)
  if [[ $variant == vmonly-shipping ]]; then
    options=(-DCMAKE_BUILD_TYPE=Release -DLHATOVE_VM_ONLY=ON -DLHATOVE_WITH_DAP=OFF)
  fi
  cmake -S "$repo" -B "$build" -G Ninja \
    -DCMAKE_PREFIX_PATH="$prefix" -DCMAKE_FIND_FRAMEWORK=LAST \
    -DLHATOVE_LHAT_DIR="$workspace/lhat" "${options[@]}"
  cmake --build "$build" --target love --parallel 3
  package="$workspace/dist/lhat-love-$platform-$variant"
  python3 "$repo/scripts/package-unix.py" "$build" "$package" --deps "$workspace/deps"
  cp "$info" "$package/build-info.txt"
  cp "$workspace/lhat/LICENSE" "$package/licenses/lhat.txt"
  if [[ $(uname -s) == Darwin ]]; then
    brew list --versions > "$package/dependencies.txt"
    engine="$package/lhat-love.app/Contents/MacOS/love"
  else
    dpkg-query -W -f='${Package} ${Version}\n' > "$package/dependencies.txt"
    engine="$package/bin/love"
  fi
  if [[ $variant == relwithdebinfo ]]; then
    full=$engine
    "$engine" --no-error-screen "$repo/testing/lh/suite"
    "$engine" --compile-game "$workspace/suite-compiled" "$repo/testing/lh/suite"
    # Compare the actual embedded bytes, independently of header formatting.
    python3 "$repo/scripts/check-generated.py" "$engine"
  else
    "$engine" --no-error-screen "$workspace/suite-compiled"
    python3 "$repo/testing/test_parallel_lton.py" --lovec "$full" --vm "$engine"
    python3 "$repo/testing/test_unicode_paths.py" --lovec "$full" --vm "$engine"
    python3 "$repo/testing/test_extensions.py" --lovec "$full" --vm "$engine" \
      --lhat "$workspace/lhat" --lhat-generated "$workspace/build-relwithdebinfo/lhat/include"
  fi
  tar -czf "$package.tar.gz" -C "$workspace/dist" "$(basename "$package")"
done
