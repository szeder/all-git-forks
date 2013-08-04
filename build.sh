#!/bin/bash
SDKVERSION="6.1"
export ARCH="armv7"
if [ "${ARCH}" == "i386" ]; then
PLATFORM="iPhoneSimulator"
else
PLATFORM="iPhoneOS"
fi

LDFLAGS="-lz"

export TOOLPATH=/Applications/XCode.app/Contents/Developer/Platforms/${PLATFORM}.platform/Developer
SDK=${TOOLPATH}/SDKs/${PLATFORM}${SDKVERSION}.sdk

if [ "${ARCH}" == "i386" ]; then
    export CC="${TOOLPATH}/usr/bin/gcc -m32 -miphoneos-version-min=5.0 -g -isysroot ${SDK} -DNDEBUG" 
    export CPP="${CC} -E -isysroot ${SDK}"
else
    export CC="${TOOLPATH}/usr/bin/gcc -miphoneos-version-min=5.0 -arch ${ARCH} -g -isysroot ${SDK} -DNDEBUG"
    export CPP="${CC} -E -isysroot ${SDK}"
fi

echo ${CC}

echo "Building for ${PLATFORM} ${SDKVERSION} ${ARCH}"

#DEVPATH=/Applications/XCode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer
export AR="${TOOLPATH}/usr/bin/ar"
export RANLIB="echo ranlib"

make distclean
make configure

if ! ./configure --host=arm-apple-darwin9 CC="${CC}" CPP="${CPP}"; then
echo "Error configuring for ${ARCH}."
exit 1
fi
if ! make all NO_DARWIN_PORTS=1 CC="${CC}" CPP="${CPP}"; then
echo "Error building for ${ARCH}."
exit 1
fi
