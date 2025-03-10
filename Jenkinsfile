pipeline {
	agent none

	stages {
		/*
			Run the build and test suite on various distributions...
		*/
		stage("Run Tests on Multiple Distributions") {
			matrix {
				axes {
					axis {
						name "DISTRO"
						values \
							"almalinux:9", \
							"archlinux:base-devel", \
							"debian:trixie", \
							"debian:bookworm", \
							"fedora:41", \
							"fedora:42", \
							"ubuntu:24.10", \
							"ubuntu:25.04"
					}

					axis {
						name "COMPILER"
						values "gcc", "clang"
					}
				}

				agent {
					docker {
						image "${DISTRO}"

						// Run as root inside the containers to install dependencies
						args "-u root"

						customWorkspace "${JOB_NAME}/${BUILD_ID}/${env.DISTRO.replace(":", "-")}/${env.COMPILER}"
					}
				}

				stages {
					stage("Install Dependencies") {
						steps {
							script {
								// Arch Linux
								if (env.DISTRO.contains("archlinux")) {
									installBuildDepsArchLinux(env.DISTRO, env.COMPILER)

								// Fedora, Alma Linux, etc.
								} else if (env.DISTRO.contains("fedora") || env.DISTRO.contains("almalinux")) {
									installBuildDepsRedHat(env.DISTRO, env.COMPILER)

								// Debian & Ubuntu
								} else if (env.DISTRO.contains("debian") || env.DISTRO.contains("ubuntu")) {
									installBuildDepsDebian(env.DISTRO, env.COMPILER, "amd64")
								}
							}
						}
					}

					stage("Configure") {
						steps {
							// Run autogen
							sh "./autogen.sh"

							// Run ./configure...
							sh """
								CC=${env.COMPILER} \
								./configure \
									--prefix=/usr \
									--enable-debug \
									--enable-lua \
									--enable-perl
							"""
						}

						post {
							failure {
								archiveArtifacts artifacts: "config.log",
									allowEmptyArchive: true

								echo "config.log has been archived"
							}
						}
					}

					stage("Build") {
						steps {
							sh "make"
						}
					}

					stage("Check") {
						steps {
							script {
								sh "make check"
							}
						}

						post {
							always {
								// Copy test logs into a special directory
								sh """
									mkdir -pv tests/${DISTRO}/${COMPILER}
									find src tests -name "*.log" | xargs --no-run-if-empty \
										cp --verbose --parents --target-directory=tests/${DISTRO}/${COMPILER}/
								"""

								// Archive the logs only if the stage fails
								archiveArtifacts artifacts: "tests/${DISTRO}/${COMPILER}/**/*"

								echo "The test logs have been archived"
							}
						}
					}
				}

				// Cleanup the workspace afterwards
				post {
					always {
						cleanWs()
					}
				}
			}
		}

		stage("Coverage Tests") {
			parallel {
				/*
					Run through Clang's Static Analyzer...
				*/
				stage("Clang Static Analyzer") {
					agent {
						docker {
							image "debian:trixie"

							// Run as root inside the containers to install dependencies
							args "-u root"

							customWorkspace "${JOB_NAME}/${BUILD_ID}/clang-static-analyzer"
						}
					}

					stages {
						stage("Install Dependencies") {
							steps {
								script {
									installBuildDepsDebian("trixie", "clang", "amd64")

									// Install Clang Tools
									sh "apt-get install -y clang-tools"
								}
							}
						}

						stage("Configure") {
							steps {
								sh "./autogen.sh"
								sh """
									scan-build \
									./configure \
										--prefix=/usr \
										--enable-debug \
										--enable-lua \
										--enable-perl
								"""
							}
						}

						stage("Build") {
							steps {
								sh "scan-build -o scan-build-output make -j\$(nproc)"
							}
						}

						stage("Publish Report") {
							steps {
								archiveArtifacts artifacts: "scan-build-output/**/*"
							}
						}
					}

					// Cleanup the workspace afterwards
					post {
						always {
							cleanWs()
						}
					}
				}
			}
		}

		stage("Debian Packages") {
			// Only build packages when we are in the master branch
			// when {
			//	expression {
			//		env.GIT_BRANCH == "origin/master"
			//	}
			// }

			stages {
				stage("Build Debian Packages") {
					matrix {
						axes {
							axis {
								name "IMAGE"
								values "debian:trixie", "debian:bookworm"
							}

							axis {
								name "ARCH"
								values "amd64", "arm64", "armel", "armhf", "i386", "ppc64el", "s390x"
							}
						}

						agent {
							docker {
								image "${IMAGE}"

								// Run as root inside the containers to install dependencies
								args "-u root"

								customWorkspace "${JOB_NAME}/${BUILD_ID}/${IMAGE.replace(":", "-")}/${ARCH}"
							}
						}

						stages {
							stage("Setup Build Environment") {
								steps {
									// Add the architecture
									sh "dpkg --add-architecture ${env.ARCH}"
									sh "apt-get update"

									// Install required packages
									sh """
										apt-get install -y \
											apt-utils \
											build-essential \
											crossbuild-essential-${env.ARCH} \
											devscripts \
											qemu-user-static
									"""
								}
							}

							stage("Install Build Dependencies") {
								steps {
									// Install all build dependencies
									sh "apt-get build-dep -y -a${env.ARCH} ."
								}
							}

							stage("Tag") {
								steps {
									sh "dch -m \"Jenkins Build ${BUILD_ID}\" -l .build-${BUILD_ID}."
								}
							}

							stage("Build") {
								environment {
									VERSION = ""
								}

								steps {
									// Create the source tarball from the checked out source
									sh """
										version="\$(dpkg-parsechangelog --show-field Version | sed "s/-[^-]*\$//")";
										tar \
											--create \
											--verbose \
											--xz \
											--file="../libloc_\$version.orig.tar.xz" \
											--transform="s|^|libloc-\$version/|" \
											*
									"""

									// Build the packages
									sh """
										dpkg-buildpackage \
											--host-arch ${env.ARCH} \
											--build=full
									"""
								}
							}

							stage("Create Repository") {
								environment {
									DISTRO = "${IMAGE.replace("debian:", "")}"
								}

								steps {
									// Create a repository and generate Packages
									sh "mkdir -pv \
										packages/debian/dists/${DISTRO}/main/binary-${ARCH} \
										packages/debian/dists/${DISTRO}/main/source \
										packages/debian/pool/${DISTRO}/main/${ARCH}"

									// Copy all packages
									sh "cp -v ../*.deb packages/debian/pool/${DISTRO}/main/${ARCH}"

									// Generate Packages
									sh "cd packages/debian && apt-ftparchive packages pool/${DISTRO}/main/${ARCH} \
										> dists/${DISTRO}/main/binary-${ARCH}/Packages"

									// Compress Packages
									sh "xz -v9 < packages/debian/dists/${DISTRO}/main/binary-${ARCH}/Packages \
										> packages/debian/dists/${DISTRO}/main/binary-${ARCH}/Packages.xz"

									// Generate Sources
									sh "cd packages/debian && apt-ftparchive sources pool/${DISTRO}/main/${ARCH} \
										> dists/${DISTRO}/main/source/Sources"

									// Compress Sources
									sh "xz -v9 < packages/debian/dists/${DISTRO}/main/source/Sources \
										> packages/debian/dists/${DISTRO}/main/source/Sources.xz"

									// Generate Contents
									sh "cd packages/debian && apt-ftparchive contents pool/${DISTRO}/main/${ARCH} \
										> dists/${DISTRO}/main/Contents-${ARCH}"

									// Compress Contents
									sh "xz -v9 < packages/debian/dists/${DISTRO}/main/Contents-${ARCH} \
										> packages/debian/dists/${DISTRO}/main/Contents-${ARCH}.xz"

									// Stash the packages
									stash includes: "packages/debian/**/*", name: "${DISTRO}-${ARCH}"
								}
							}
						}

						// Cleanup the workspace afterwards
						post {
							always {
								cleanWs()
							}
						}
					}
				}

				stage("Master Debian Repository") {
					agent any

					// We don't need to check out the source for this stage
					options {
						skipDefaultCheckout()
					}

					environment {
						GNUPGHOME  = "${WORKSPACE}/.gnupg"
						KRB5CCNAME = "${WORKSPACE}/.krb5cc"

						// Our signing key
						GPG_KEY_ID = "E4D20FA6FAA108D54ABDFC6541836ADF9D5E2AD9"
					}

					steps {
						// Cleanup the workspace
						cleanWs()

						// Create the GPG stash directory
						sh """
							mkdir -p $GNUPGHOME
							chmod 700 $GNUPGHOME
						"""

						// Import the GPG key
						withCredentials([file(credentialsId: "${env.GPG_KEY_ID}", variable: "GPG_KEY_FILE")]) {
							// Jenkins prefers to have single quotes here so that $GPG_KEY_FILE won't be expanded
							sh 'gpg --import --batch < $GPG_KEY_FILE'
						}

						// Unstash all stashed packages from the matrix build
						script {
							for (distro in ["trixie", "bookworm"]) {
								for (arch in ["amd64", "arm64", "armel", "armhf", "i386", "ppc64el", "s390x"]) {
									unstash "${distro}-${arch}"
								}

								// Create the Release file
								sh """
									(
										echo "Origin: Pakfire Repository"
										echo "Label: Pakfire Repository"
										echo "Suite: stable"
										echo "Codename: $distro"
										echo "Version: 1.0"
										echo "Architectures: amd64 arm64"
										echo "Components: main"
										echo "Description: Pakfire Jenkins Repository"

										# Do the rest automatically
										cd packages/debian && apt-ftparchive release dists/$distro
									) >> packages/debian/dists/$distro/Release
								"""

								// Create InRelease
								sh """
									gpg --batch \
										--clearsign \
										--local-user ${env.GPG_KEY_ID} \
										--output packages/debian/dists/$distro/InRelease \
										packages/debian/dists/$distro/Release
								"""

								// Create Release.gpg
								sh """
									gpg --batch \
										--armor \
										--detach-sign \
										--local-user ${env.GPG_KEY_ID} \
										--output packages/debian/dists/$distro/Release.gpg \
										packages/debian/dists/$distro/Release
								"""
							}
						}

						// Export the public key
						sh "gpg --batch --export --armor ${env.GPG_KEY_ID} \
							> packages/debian/${env.GPG_KEY_ID}.asc"

						// Remove the GPG key material as soon as possible
						sh "rm -rf $GNUPGHOME"

						// Upload everything again
						archiveArtifacts artifacts: "packages/debian/**/*"

						// Fetch a Kerberos ticket
						withCredentials([file(credentialsId: "jenkins.keytab", variable: "KEYTAB")]) {
							sh 'kinit -kV -t $KEYTAB jenkins@IPFIRE.ORG'
						}

						// Publish files
						sh """
							rsync \
								--verbose \
								--recursive \
								--delete \
								--delete-excluded \
								--delay-updates \
								packages/debian/ \
								pakfire@fs01.haj.ipfire.org:/pub/mirror/packages/debian/libloc
						"""

						// Destroy the Kerberos credentials
						sh "kdestroy"
					}
				}
			}
		}
	}
}

// Installs everything we need on RHEL/Fedora/etc.
def installBuildDepsRedHat(distro, compier) {
	// Install basic development tools
	if (distro.contains("almalinux:9")) {
		sh "dnf group install -y 'Development Tools'"

	// Other distributions
	} else {
		sh "dnf group install -y development-tools"
	}

	// Install our own build and runtime dependencies
	sh """
		dnf install -y \
			asciidoc \
			autoconf \
			automake \
			intltool \
			libtool \
			pkg-config \
			${compiler} \
			\
			lua-devel \
			lua-lunit \
			openssl-devel \
			perl-devel \
			"perl(Test::More)" \
			python3-devel \
			systemd-devel
	"""
}

// Installs everything we need on Arch Linux
def installBuildDepsArchLinux(distro, compiler) {
	sh "pacman -Syu --noconfirm"
	sh """
		pacman -Sy \
			--needed \
			--noconfirm \
			asciidoc \
			autoconf \
			automake \
			intltool \
			libtool \
			pkg-config \
			${compiler} \
			\
			lua \
			openssl \
			perl \
			python3 \
			systemd
	"""
}

// Installs everything we need on Debian
def installBuildDepsDebian(distro, compiler, arch) {
	sh "apt-get update"
	sh """
		apt-get install -y \
			--no-install-recommends \
			asciidoc \
			autoconf \
			automake \
			build-essential \
			intltool \
			libtool \
			pkg-config \
			${compiler} \
			\
			liblua5.4-dev \
			libperl-dev \
			libpython3-dev \
			libssl-dev \
			libsystemd-dev \
			lua-unit
	"""
}
