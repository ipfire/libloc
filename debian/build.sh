#!/bin/bash

set -x

ARCHITECTURES=( amd64 arm64 i386 armhf riscv64 )
RELEASES=( buster )

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
    tar xvfa "${sources}" -C "${tmp}"

    # Copy the tarball under the correct Debian name
    cp -vf "${sources}" "${tmp}/${package//-/_}.orig.tar.xz"

    # Change into source directory
    pushd "${tmp}/${package}"

    # Prepare the build environment
    #if ! debuild -us -uc; then
    #    echo "Could not prepare build environment" >&2
    #    return 1
    #fi

    # Build the package for each release
    local release
    for release in ${RELEASES[@]}; do
        local chroot="${release}-${host_arch}-sbuild"

        # Create a chroot environment
        if [ ! -d "/etc/sbuild/chroot/${chroot}" ]; then
            if ! sbuild-createchroot --arch="${host_arch}" "${release}" \
                    "${CHROOT_PATH}/${chroot}"; then
                echo "Could not create chroot for ${release} on ${host_arch}" >&2
                return 1
            fi
        fi

        # And for each architecture we want to support
        local arch
        for arch in ${ARCHITECTURES[@]}; do
            # Run the build process
            if ! sbuild --dist="${release}" --host="${arch}"; then
                echo "Could not build package for ${release} on ${arch}" >&2
                return 1
            fi
        done
    done

    popd

    # Cleanup
    rm -rf "${tmp}"
}

main "$@" || exit $?
