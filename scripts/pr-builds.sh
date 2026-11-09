#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors







set -euo pipefail

if [ $# -lt 4 ] || [ $# -gt 5 ] || { [ $# -eq 5 ] && [ "$5" != --print ]; }; then
    echo "usage: $0 REPO PR RUN SHA [--print]" >&2
    exit 2
fi
repo=$1 pr=$2 run=$3 sha=$4 print=${5:-}
mark='<!-- YKW4Recomp-builds -->'

if [ -z "$print" ] && [ "$(gh api "repos/$repo/pulls/$pr" --jq .head.sha)" != "$sha" ]; then
    echo "$sha is no longer the head of #$pr: its comment stays"
    exit 0
fi

artifacts=$(gh api "repos/$repo/actions/runs/$run/artifacts?per_page=100")
links=$(jq -r --arg url "https://github.com/$repo/actions/runs/$run/artifacts" '
    .artifacts | map(select((.expired | not) and (.name | endswith("-stage") | not))) | sort_by(.name)[] |
    "- [\(.name)](\($url)/\(.id)), \(.size_in_bytes / 1048576 | floor) MiB"' <<<"$artifacts")
until=$(jq -r '[.artifacts[].expires_at] | min // empty | .[:10]' <<<"$artifacts")
needs=${NEEDS:-}
[ -n "$needs" ] || needs='{}'
failed=$(jq -r 'to_entries[] | select(.value.result != "success") | "\(.key) (\(.value.result))"' <<<"$needs" |
    paste -sd, - | sed 's/,/, /g')

body="$mark
Builds of ${sha::7}${until:+, kept until $until}:
${links:-none}${failed:+

Not built: $failed}"

if [ -n "$print" ]; then
    printf '%s\n' "$body"
    exit 0
fi
id=$(gh api --paginate "repos/$repo/issues/$pr/comments?per_page=100" --jq \
    ".[] | select(.user.login == \"github-actions[bot]\" and (.body | startswith(\"$mark\"))) | .id" | head -n1)
if [ -n "$id" ]; then
    gh api -X PATCH "repos/$repo/issues/comments/$id" -f body="$body" > /dev/null
else
    gh api "repos/$repo/issues/$pr/comments" -f body="$body" > /dev/null
fi
echo "linked the builds of $sha from #$pr"
