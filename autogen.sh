#! /bin/sh
# script shamelessy taken from openbox

sh() {
  /bin/sh -c "set -x; $*"
}

# test if gettext-devel is installed.

	sh autopoint --force 
	if [ "$?" -ne "0" ]; then
	        echo "You need to install the gettext-devel package"
		exit 1
	else
		sh libtoolize --copy --force --automake
	fi


# ok, this is wierd, but apparantly every subdir
# in the centericq sources has its own configure
# no idea why. should fix this.

TOP=$PWD
traverse=`find $PWD -name "configure.[ia][nc]" -print`
for i in $traverse; do
	echo Changing directory to `dirname $i`
	cd `dirname $i` > /dev/null
    if test "`dirname $i`" = "$TOP/kkstrtext"; then
        #config.rpath is needed for AM_ICONV
        #Here we should do autopoint to get the config.rpath,
        #yet it would complain about missing po and intl directories,
        #so we simply copy config.rpath from the top directory
        sh cp $TOP/config.rpath .
    fi

	if test "$PWD" = "$TOP"; then
		sh aclocal -I m4 $ACLOCAL_FLAGS
	else
		sh aclocal $ACLOCAL_FLAGS
	fi
	headneeded=`grep -E "A(M|C)_CONFIG_HEADER" configure.[ia][nc]`
	if test ! -z "$headneeded"; then sh autoheader; fi
	sh autoconf
	sh automake --add-missing --copy

	cd - > /dev/null
done

echo
echo You are now ready to run ./configure
echo enjoy!
