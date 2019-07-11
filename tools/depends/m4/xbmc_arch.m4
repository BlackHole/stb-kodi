AC_DEFUN([XBMC_SETUP_ARCH_DEFINES],[

# build detection and setup - this is the native arch
case $build in
  i*86*-linux-gnu*|i*86*-*-linux-uclibc*|i*86*-*-linux-musl*)
     AC_SUBST(NATIVE_ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX")
     ;;
  x86_64-*-linux-gnu*|x86_64-*-linux-uclibc*|x86_64-*-linux-musl*)
     AC_SUBST(NATIVE_ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX")
     ;;
  i386-*-freebsd*)
     AC_SUBST(NATIVE_ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_FREEBSD -D_LINUX")
     ;;
  amd64-*-freebsd*)
     AC_SUBST(NATIVE_ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_FREEBSD -D_LINUX")
     ;;
  *86*-apple-darwin*)
     AC_SUBST(NATIVE_ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_DARWIN -DTARGET_DARWIN_OSX -D_LINUX")
     ;;
  powerpc-*-linux-gnu*|powerpc-*-linux-uclibc*|powerpc-*-linux-musl*)
     AC_SUBST(NATIVE_ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -D_POWERPC")
     ;;
  powerpc64-*-linux-gnu*|powerpc64-*-linux-uclibc*|powerpc64-*-linux-musl*)
     AC_SUBST(NATIVE_ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -D_POWERPC64")
     ;;
  arm*-*-linux-gnu*|arm*-*-linux-uclibc*|arm*-*-linux-musl*)
     AC_SUBST(NATIVE_ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX")
     ;;
  *)
     AC_MSG_ERROR(unsupported native build platform: $build)
esac


# host detection and setup - this is the target arch
case $host in
  i*86*-linux-gnu*|i*86*-*-linux-uclibc*|i*86*-*-linux-musl*)
     AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX")
     ;;
  x86_64-*-linux-gnu*|x86_64-*-linux-uclibc*|x86_64-*-linux-musl*)
     AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX")
     ;;
  i386-*-freebsd*)
     AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_FREEBSD -D_LINUX")
     ;;
  amd64-*-freebsd*)
     AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_FREEBSD -D_LINUX")
     ;;
  arm-apple-darwin*)
     AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_DARWIN -DTARGET_DARWIN_IOS -D_LINUX")
     ;;
  *86*-apple-darwin*)
     AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_DARWIN -DTARGET_DARWIN_OSX -D_LINUX")
     ;;
  powerpc-apple-darwin*)
     AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_DARWIN -DTARGET_DARWIN_OSX -D_LINUX")
     ;;
  powerpc-*-linux-gnu*|powerpc-*-linux-uclibc*|powerpc-*-linux-musl*)
     AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -D_POWERPC")
     ;;
  powerpc64*-*-linux-gnu*|powerpc64*-*-linux-uclibc*|powerpc64*-*-linux-musl*)
     AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -D_POWERPC64")
     ;;
  arm*-*-linux-gnu*|arm*-*-linux-uclibc*|arm*-*-linux-musl*|aarch64*-*-linux-gnu*|aarch64*-*-linux-uclibc*|aarch64*-*-linux-musl*)
     AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX")
     ;;
  mips*-*-linux-gnu*|mips*-*-linux-uclibc*|mips*-*-linux-musl*)
     AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX")
     ;;
  *-*linux-android*)
     AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -DTARGET_ANDROID")
     ;;
  *)
     AC_MSG_ERROR(unsupported build target: $host)
esac

if test "$target_platform" = "target_android" ; then
  AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -DTARGET_ANDROID")
fi

if test "$target_platform" = "target_raspberry_pi" ; then
  AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -D_ARMEL -DTARGET_RASPBERRY_PI")
fi

if test "$target_videoplatform" = "target_xcore" ; then
  AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -DTARGET_STB -DTARGET_STB_EXTEND -DTARGET_V3D -DTARGET_XCORE")
fi

if test "$target_videoplatform" = "target_v3dnxpl" ; then
  AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -DTARGET_STB -DTARGET_STB_EXTEND -DTARGET_V3D -DTARGET_V3DNXPL")
fi

if test "$target_videoplatform" = "target_dreambox" ; then
  AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -DTARGET_STB -DTARGET_STB_EXTEND -DTARGET_V3D -DTARGET_DREAMBOX")
fi

if test "$target_videoplatform_arch" = "target_vuplus_arm" ; then
  AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -DTARGET_STB -DTARGET_STB_EXTEND -DTARGET_V3D -DTARGET_VUPLUS -DTARGET_VUPLUS_ARM")
fi

if test "$target_videoplatform_arch" = "target_vuplus_mipsel" ; then
  AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -DTARGET_STB -DTARGET_STB_EXTEND -DTARGET_V3D -DTARGET_VUPLUS -DTARGET_VUPLUS_MIPSEL")
fi

if test "$target_videoplatform" = "target_nextv" ; then
  AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -DTARGET_STB -DTARGET_STB_EXTEND -DTARGET_V3D -DTARGET_NEXTV")
fi

if test "$target_videoplatform" = "target_dags" ; then
  AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -DTARGET_STB -DTARGET_STB_EXTEND -DTARGET_V3D -DTARGET_DAGS")
fi

if test "$target_videoplatform" = "target_GB" ; then
  AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -DTARGET_STB -DTARGET_STB_EXTEND -DTARGET_V3D -DTARGET_GB")
fi

if test "$target_videoplatform" = "target_mali" ; then
  AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -DTARGET_STB -DTARGET_STB_EXTEND -DTARGET_MALI")
fi
if test "$target_videoplatform" = "target_aml" ; then
  AC_SUBST(ARCH_DEFINES, "-DTARGET_POSIX -DTARGET_LINUX -D_LINUX -DTARGET_STB -DTARGET_AML")
fi
])
