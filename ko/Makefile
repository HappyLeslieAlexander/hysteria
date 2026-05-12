KMOD= hysteria
PROFILE?= release
WITH_INET6?= 1
WITH_OCF?= 0

SRCS= src/hysteria_mod.c src/hysteria_proto.c src/hysteria_ctl.c
SRCS+= src/hysteria_net.c src/hysteria_bbr.c src/hysteria_session.c src/hysteria_brutal.c
SRCS+= opt_inet.h device_if.h bus_if.h

CFLAGS+= -I${.CURDIR}/include -std=c11 -Wall -Wextra

.if ${PROFILE} == "debug"
CFLAGS+= -O0 -g -DINVARIANTS -DWITNESS
.else
CFLAGS+= -O2 -DNDEBUG
.endif

.if ${WITH_INET6} == 1
CFLAGS+= -DINET6
.endif

.if ${WITH_OCF} == 1
CFLAGS+= -DWITH_OCF
.endif

# FreeBSD kmod build needs kernel source tree (sys/).
# If caller does not provide SYSDIR, auto-detect the common location.
.if !defined(SYSDIR) && exists(/usr/src/sys)
SYSDIR= /usr/src/sys
.endif

.include <bsd.kmod.mk>
