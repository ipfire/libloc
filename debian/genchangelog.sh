#!/bin/bash -e
gitshow () {
  local format=$1
  local commit=$2

  git show --no-patch --format=format:"$format" "$commit"
}

main () {
  if [ $# -lt 1 ]; then
    local bn="$(basename $0)"
    echo "Usage:    $bn  <commit range>" >&2
    echo "Example:  $bn  0.9.7..HEAD" >&2
    echo "Example:  $bn  0.9.5..0.9.6^" >&2
    return 1
  fi

  local commitrange=$1

  local commit
  for commit in $(git rev-list --reverse "$commitrange"); do
    # Skip commits with diffs that only have Makefile.am or d/ changes.
    if [ "$(git diff --name-only "${commit}^..${commit}" -- . ':^Makefile.am' ':^debian/' | wc -l)" == 0 ]; then
      continue
    fi

    local author_name="$(gitshow %an "$commit")"
    local author_email="$(gitshow %ae "$commit")"
    local subject="$(gitshow %s "$commit")"

    echo "$author_name <$author_email>  $subject"
    DEBFULLNAME="$author_name" DEBEMAIL="$author_email" debchange --upstream --multimaint-merge "$subject"
  done

  debchange --release ''
}

main "$@" || exit $?
