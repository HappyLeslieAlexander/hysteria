KMOD= hysteria
SRCS= src/hysteria_mod.c src/hysteria_proto.c src/hysteria_ctl.c
SRCS+= src/hysteria_net.c src/hysteria_bbr.c
SRCS+= opt_inet.h device_if.h bus_if.h
CFLAGS+= -I${.CURDIR}/include -std=c11 -Wall -Wextra

.include <bsd.kmod.mk>
