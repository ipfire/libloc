#!/bin/bash

ARCHITECTURES=( amd64 arm64 i386 armhf )
RELEASES=( buster bullseye bookworm sid )

CHROOT_PATH="/var/tmp"

main() {
    if [ $# -lt 2 ]; then
        echo "Not enough arguments" >&2
        return 2
    fi

    # Get host architecture
    local host_arch="$(dpkg --print-architecture)"
    if [ -z "${host_arch}" ]; then
        echo "Could not discover host architecture" >&2
        return 1
    fi

    local package="${1}"
    local sources="${2}"

    # Create some temporary directory
    local tmp="$(mktemp -d)"

    # Extract the sources into it
    mkdir -p "${tmp}/sources"
    tar xvfa "${sources}" -C "${tmp}/sources"

    # Copy the tarball under the correct Debian name
    cp -vf "${sources}" "${tmp}/sources/${package//-/_}.orig.tar.xz"

    # Change into temporary directory
    pushd "${tmp}"

    # Build the package for each release
    local release
    for release in ${RELEASES[@]}; do
        local chroot="${release}-${host_arch}-sbuild"
        if [ "${release}" = "buster" ]; then
            local buster_backport=( --extra-repository "deb http://deb.debian.org/debian buster-backports main" --build-dep-resolver=aspcud )
        fi

        mkdir -p "${release}"
        pushd "${release}"

        # Create a chroot environment
        if [ ! -d "/etc/sbuild/chroot/${chroot}" ]; then
            if ! sbuild-createchroot --arch="${host_arch}" "${release}" \
                    "${CHROOT_PATH}/${chroot}"; then
                echo "Could not create chroot for ${release} on ${host_arch}" >&2
                rm -rf "${tmp}"
                return 1
            fi
        fi

        # And for each architecture we want to support
        local arch
        for arch in ${ARCHITECTURES[@]}; do
            mkdir -p "${arch}"
            pushd "${arch}"

            # Copy sources
            cp -r "${tmp}/sources" .

            # Run the build process
            if ! sbuild --dist="${release}" --host="${arch}" --source "${buster_backport[@]}" "sources/${package}"; then
                echo "Could not build package for ${release} on ${arch}" >&2
                rm -rf "${tmp}"
                return 1
            fi

            # Remove the sources
            rm -rf "sources/${package}"
            popd
        done
        popd
    done

    # Remove sources
    rm -rf "${tmp}/sources"
    popd

    # Done!
    echo "SUCCESS!"
    echo "  You can find your Debian packages in ${tmp}"
    return 0
}

main "$@" || exit $?
