#!/usr/bin/env bash
# Downloads a FritzGate release's manifest.json, written by the release workflow, and the flash
# parts it lists next to this script, so the web installer can flash the release. GitHub release
# downloads send no CORS headers, so the browser can't load them from github.com: the site has to
# serve them itself.
#
# Usage: fetch-firmware.sh [tag]   (default: the latest release, prereleases excluded)
# Set GITHUB_TOKEN to avoid the GitHub API rate limit, e.g. on CI runners.
set -euo pipefail

repo="elebree/fritzgate"
dir="$(cd "$(dirname "$0")" && pwd)"
auth=()
if [[ -n "${GITHUB_TOKEN:-}" ]]; then
  auth=(-H "Authorization: Bearer $GITHUB_TOKEN")
fi

tag="${1:-}"
if [[ -z "$tag" ]]; then
  tag="$(curl -fsSL ${auth[@]+"${auth[@]}"} "https://api.github.com/repos/$repo/releases/latest" |
    sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -n 1)"
fi
if [[ -z "$tag" ]]; then
  echo "No FritzGate release found" >&2
  exit 1
fi

url="https://github.com/$repo/releases/download/$tag"
rm -f "$dir"/fritzgate-*.bin "$dir/manifest.json"
if ! curl -fsSL -o "$dir/manifest.json" "$url/fritzgate-$tag-manifest.json"; then
  echo "Release $tag has no fritzgate-$tag-manifest.json" >&2
  rm -f "$dir/manifest.json"
  exit 1
fi

# The release workflow writes one part per line
parts="$(sed -n 's/.*"path": *"\([^"]*\)".*/\1/p' "$dir/manifest.json")"
if [[ -z "$parts" ]]; then
  echo "The manifest of $tag lists no parts" >&2
  exit 1
fi
for part in $parts; do
  curl -fsSL -o "$dir/$part" "$url/$part"
done
echo "FritzGate $tag is ready for the web installer in $dir"
