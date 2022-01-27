#!/bin/bash

TRAVISUID=$1
TASK=$2
COMPONENTS=${3-}

[[ $TASK == noop ]] && exit 0

set -e

case $(</etc/redhat-release) in
    CentOS*\ 6*) OS=centos6 ;;
    CentOS*\ 7*) OS=centos7 ;;
    Rocky\ Linux*\ 8*) OS=rockylinux8 ;;
    *) OS=unknown ;;
esac

# EPEL required for UDT
case $OS in
    centos6)  yum -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-6.noarch.rpm
              ;;
    centos7)  yum -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
              ;;
    rockylinux8)
              yum -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm
              dnf -y install dnf-plugins-core
              dnf config-manager --set-enabled powertools
              ;;
esac

# Clean the yum cache
yum clean all

packages=(gcc gcc-c++ make autoconf automake libtool \
          libtool-ltdl-devel openssl openssl-devel git \
          'perl(Test)' 'perl(Test::More)' 'perl(File::Spec)' \
          'perl(URI)' file sudo bison patch curl \
          pam pam-devel libedit libedit-devel)

if [[ $OS == rockylinux8 ]]; then

    # provides `cmp` used by `packaging/git-dirt-filter`
    packages+=(diffutils)
fi

if [[ $TASK == tests ]]; then
    set +e
    [[ $COMPONENTS == *udt* ]]     && packages+=(udt-devel libnice-devel)
    [[ $COMPONENTS == *myproxy* ]] && packages+=(which)
    [[ $COMPONENTS == *gram* ]]    && packages+=('perl(Pod::Html)')
    set -e
elif [[ $TASK == *rpms ]]; then
    packages+=(rpm-build doxygen graphviz 'perl(Pod::Html)')
    # for globus-gridftp-server:
    packages+=(fakeroot)
    # for globus-xio-udt-driver:
    packages+=(udt udt-devel glib2-devel libnice-devel gettext-devel libffi-devel)
    # for globus-gram-job-manager:
    packages+=(libxml2-devel)
    # for myproxy:
    packages+=(pam-devel voms-devel cyrus-sasl-devel openldap-devel voms-clients initscripts)
    # for globus-net-manager:
    packages+=(python-devel)
    # for globus-gram-audit:
    packages+=('perl(DBI)')
    # for globus-scheduler-event-generator:
    packages+=(redhat-lsb-core)
    # for myproxy-oauth
    packages+=(m2crypto mod_ssl mod_wsgi pyOpenSSL python-crypto)
    # for gsi-openssh
    packages+=(pam libedit libedit-devel)
fi


yum -y -d1 install "${packages[@]}"

# UID of travis user inside needs to match UID of travis user outside
getent passwd travis > /dev/null || useradd travis -u $TRAVISUID -o
# travis will require sudo when building RPMs to do yum installs
if [[ -d /etc/sudoers.d ]]; then
    echo "travis ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/10_travis
else
    echo -e "\ntravis ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers
fi

chown -R travis: /gct
cd /gct


print_file_size_table () {
    (
        set +x
        echo '==========================================================================================='
        echo "                                       $1"
        echo ' Name                                                                             Size'
        echo '-------------------------------------------------------------------------------- ----------'
        find "$2" -type f -printf '%-80f %10s\n' | sort
    )
}

make_tarballs() {
    su travis -c "/bin/bash -x /gct/travis-ci/make_source_tarballs.sh"
    print_file_size_table Tarballs /gct/package-output
}

make_srpms() {
    dist=$1
    su travis -c "/bin/bash -x /gct/travis-ci/make_rpms.sh -s -d $dist"
    print_file_size_table SRPMS /gct/packaging/rpmbuild/SRPMS
}

make_rpms() {
    nocheck=$1
    dist=$2
    su travis -c "/bin/bash -x /gct/travis-ci/make_rpms.sh $nocheck -d $dist"
    print_file_size_table SRPMS /gct/packaging/rpmbuild/SRPMS
    print_file_size_table RPMS /gct/packaging/rpmbuild/RPMS
}


case $TASK in
    tests)
        su travis -c "/bin/bash -x /gct/travis-ci/build_and_test.sh $OS $COMPONENTS"
        ;;
    tarballs)
        make_tarballs
        ;;
    srpms)
        make_tarballs
        echo '==========================================================================================='
        make_srpms .gct

        # copy all the files we want to deploy into one directory b/c
        # can't specify multiple directories in travis
        mkdir -p travis_deploy

        cp -f packaging/rpmbuild/SRPMS/*.rpm package-output/*.tar.gz  \
              travis_deploy/
        ;;
    rpms)
        make_tarballs
        echo '==========================================================================================='
        if [[ $OS == centos6 ]]; then
            # doesn't support rpmbuild --nocheck
            nocheck=
        else
            # -C = skip unit tests
            nocheck=-C
        fi
        make_rpms $nocheck .gct.$OS

        # copy all the files we want to deploy into one directory b/c
        # can't specify multiple directories in travis
        mkdir -p travis_deploy
        # HACK: only deploy the common files (tarballs, srpms) on one OS
        # to avoid attempting to overwrite the build products (which will
        # raise an error).
        # `overwrite: true` in .travis.yml ought to fix that, but doesn't
        # appear to.
        if [[ $OS == centos6 ]]; then
            cp -f packaging/rpmbuild/SRPMS/*.rpm package-output/*.tar.gz  \
                travis_deploy/
        fi
        cp -f packaging/rpmbuild/RPMS/noarch/*.rpm packaging/rpmbuild/RPMS/x86_64/*.rpm  \
            travis_deploy/
        ;;
    *)
        echo "*** INVALID TASK '$TASK' ***"
        exit 2
        ;;
esac
