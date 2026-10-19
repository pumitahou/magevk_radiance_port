#!/bin/bash
# Shared environment; paths are discovered on this machine, never committed.
RADIANCE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export RADIANCE_ROOT
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
if ! command -v brew >/dev/null 2>&1 && [[ -x /opt/homebrew/bin/brew ]]; then
  export PATH="/opt/homebrew/bin:$PATH"
fi
if command -v brew >/dev/null 2>&1; then
  RADIANCE_PYTHON_PREFIX="$(brew --prefix python@3.13 2>/dev/null || true)"
  if [[ -x "$RADIANCE_PYTHON_PREFIX/libexec/bin/python3" ]]; then
    export PATH="$RADIANCE_PYTHON_PREFIX/libexec/bin:$PATH"
  fi
fi
if [[ -z "${JAVA_HOME:-}" ]]; then
  if command -v brew >/dev/null 2>&1 && [[ -d "$(brew --prefix openjdk@21 2>/dev/null)/libexec/openjdk.jdk/Contents/Home" ]]; then
    export JAVA_HOME="$(brew --prefix openjdk@21)/libexec/openjdk.jdk/Contents/Home"
  else
    export JAVA_HOME="$(/usr/libexec/java_home -v 21 2>/dev/null || true)"
  fi
fi
if [[ -n "${JAVA_HOME:-}" ]]; then export PATH="$JAVA_HOME/bin:$PATH"; fi
if [[ -z "${VULKAN_SDK:-}" ]]; then
  for sdk_candidate in "$RADIANCE_ROOT/.tools/vulkan-sdk/macOS" "$RADIANCE_ROOT/vulkan-sdk/macOS"; do
    if [[ -x "$sdk_candidate/bin/dxc" ]]; then export VULKAN_SDK="$sdk_candidate"; break; fi
  done
fi
export GRADLE_USER_HOME="${GRADLE_USER_HOME:-$RADIANCE_ROOT/.cache/gradle}"
export HOMEBREW_NO_AUTO_UPDATE=1
