# FIXME: use wl12xx.ko for now to not break the user space
wl12xx-objs		= wl1251_main.o wl1251_netlink.o wl1251_event.o \
			  wl1251_tx.o wl1251_rx.o wl1251_ps.o wl1251_cmd.o \
			  wl1251_acx.o wl1251_boot.o wl1251_init.o \
			  wl1251_debugfs.o wl1251_spi.o
obj-$(CONFIG_WL1251)	+= wl12xx.o

wl1271-objs		= wl1271_main.o wl1271_spi.o wl1271_cmd.o \
			  wl1271_netlink.o wl1271_event.o \
			  wl1271_tx.o wl1271_rx.o wl1271_ps.o \
			  wl1271_acx.o wl1271_boot.o wl1271_init.o \
			  wl1271_debugfs.o
obj-$(CONFIG_WL1271)	+= wl1271.o
