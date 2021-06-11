#!/bin/bash
SYMBOLS_PKG=libloc1
LOCAL_FILE=debian/libloc1.symbols
TEMP_FILE="$(mktemp --tmpdir libloc1.XXXXXX.symbols)"
trap "rm -f ${TEMP_FILE}" EXIT

generate () {
  intltoolize --force --automake
  autoreconf --install --symlink
  ./configure CFLAGS='-g -O0' --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib

  make

  dpkg-gensymbols -p"$SYMBOLS_PKG" -O"$TEMP_FILE" -esrc/.libs/libloc.so.*
  sed -i -E -e 's/( [0-9\.]+)-.+$/\1/' "$TEMP_FILE"

  make clean
}

main () {
  local maxver='0.0.0'
  if [ -f "$LOCAL_FILE" ]; then
    cp "$LOCAL_FILE" "$TEMP_FILE"
    maxver="$(grep -E '^ ' "$LOCAL_FILE" | cut -d' ' -f3 | sort -Vru | head -n1)"
    echo "Latest version checked: $maxver"
  fi


  local tag
  for tag in $(git tag -l --sort=version:refname)
  do
    if [ "$(echo -e "${maxver}\n${tag}" | sort -Vr | head -n1)" == "$maxver" ]; then
      echo "Tag $tag -- skip"
      continue
    fi

    echo "Tag $tag -- checking"
    git switch --quiet --detach "$tag" || return 1
    generate || return 1
    git switch --quiet - || return 1
  done

  echo "Current -- checking"
  generate || return 1

  mv "$TEMP_FILE" "$LOCAL_FILE"
  chmod 644 "$LOCAL_FILE"
}

main "$@" || exit $?
