#!/bin/bash
set -e

all_builds='[
  {"os":"linux","arch":"amd64","runner":"ubuntu-latest","container":"debian:bullseye"},
  {"os":"linux","arch":"arm64","runner":"ubuntu-22.04-arm","container":"debian:bullseye"},
  {"os":"windows","arch":"386","runner":"windows-latest","container":""},
  {"os":"windows","arch":"amd64","runner":"windows-latest","container":""},
  {"os":"darwin","arch":"amd64","runner":"macos-15-intel","container":""},
  {"os":"darwin","arch":"arm64","runner":"macos-latest","container":""}
]'

if [ "$EVENT_NAME" != "workflow_dispatch" ]; then
  matrix=$(echo "$all_builds" | jq -c .)
else
  matrix=$(echo "$all_builds" | jq -c '[.[] | select(
    (.os == "linux" and .arch == "amd64" and $linux_amd64 == "true") or
    (.os == "linux" and .arch == "arm64" and $linux_arm64 == "true") or
    (.os == "windows" and .arch == "386" and $windows_386 == "true") or
    (.os == "windows" and .arch == "amd64" and $windows_amd64 == "true") or
    (.os == "darwin" and .arch == "amd64" and $darwin_amd64 == "true") or
    (.os == "darwin" and .arch == "arm64" and $darwin_arm64 == "true")
  )]' \
    --arg linux_amd64 "$DEPLOY_LINUX_AMD64" \
    --arg linux_arm64 "$DEPLOY_LINUX_ARM64" \
    --arg windows_386 "$DEPLOY_WINDOWS_386" \
    --arg windows_amd64 "$DEPLOY_WINDOWS_AMD64" \
    --arg darwin_amd64 "$DEPLOY_DARWIN_AMD64" \
    --arg darwin_arm64 "$DEPLOY_DARWIN_ARM64")
fi

output="matrix={\"include\":$matrix}"
if [ -n "$GITHUB_OUTPUT" ]; then
  echo "$output" >> "$GITHUB_OUTPUT"
else
  echo "$output"
fi
