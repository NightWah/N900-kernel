#
# Makefile for the linux kernel.
#

# Common support
obj-y := irq.o id.o io.o sdrc.o control.o prcm.o clock.o mux.o \
		devices.o serial.o gpmc.o timer-gp.o powerdomain.o \
		clockdomain.o omapdev.o

obj-$(CONFIG_OMAP_MCBSP) += mcbsp.o

# Functions loaded to SRAM
obj-$(CONFIG_ARCH_OMAP2420)		+= sram242x.o
obj-$(CONFIG_ARCH_OMAP2430)		+= sram243x.o
obj-$(CONFIG_ARCH_OMAP3)		+= sram34xx.o

# SMS/SDRC
obj-$(CONFIG_ARCH_OMAP2)		+= sdrc2xxx.o
# obj-$(CONFIG_ARCH_OMAP3)		+= sdrc3xxx.o

# Power Management
ifeq ($(CONFIG_PM),y)
obj-y					+= pm.o
obj-$(CONFIG_ARCH_OMAP2)		+= pm24xx.o
obj-$(CONFIG_ARCH_OMAP24XX)		+= sleep24xx.o
obj-$(CONFIG_ARCH_OMAP3)		+= pm34xx.o sleep34xx.o cpuidle34xx.o
obj-$(CONFIG_PM_DEBUG)			+= pm-debug.o
endif

# SmartReflex driver
obj-$(CONFIG_OMAP_SMARTREFLEX)		+= smartreflex.o

# Clock framework
obj-$(CONFIG_ARCH_OMAP2)		+= clock24xx.o
obj-$(CONFIG_ARCH_OMAP3)		+= clock34xx.o
obj-$(CONFIG_OMAP_PM_SRF)		+=  resource34xx.o

obj-$(CONFIG_MPU_BRIDGE)		+= dspbridge.o

# DSP
obj-$(CONFIG_OMAP_MMU_FWK)	+= mmu_mach.o
obj-$(CONFIG_OMAP_MBOX_FWK)	+= mailbox_mach.o
mailbox_mach-objs		:= mailbox.o
mmu_mach-objs			:= mmu.o

# Debobs
obj-$(CONFIG_OMAP3_DEBOBS)	+= debobs.o

iommu-y					+= iommu2.o
iommu-$(CONFIG_ARCH_OMAP3)		+= omap3-iommu.o

obj-$(CONFIG_OMAP_IOMMU)		+= $(iommu-y)

# Specific board support
obj-$(CONFIG_MACH_OMAP_GENERIC)		+= board-generic.o
obj-$(CONFIG_MACH_OMAP_H4)		+= board-h4.o board-h4-mmc.o
obj-$(CONFIG_MACH_OMAP_2430SDP)		+= board-2430sdp.o \
					   board-2430sdp-flash.o \
					   mmc-twl4030.o \
					   usb-musb.o \
					   usb-ehci.o
obj-$(CONFIG_MACH_OMAP2EVM)		+= board-omap2evm.o \
					   mmc-twl4030.o
obj-$(CONFIG_MACH_OMAP_3430SDP)		+= board-3430sdp.o \
					   mmc-twl4030.o \
					   usb-musb.o \
					   usb-ehci.o \
					   board-3430sdp-flash.o
obj-$(CONFIG_MACH_OMAP3EVM)		+= board-omap3evm.o \
					   mmc-twl4030.o \
					   usb-musb.o usb-ehci.o \
					   board-omap3evm-flash.o \
					   twl4030-generic-scripts.o
obj-$(CONFIG_MACH_OMAP3_BEAGLE)		+= board-omap3beagle.o \
					   usb-musb.o usb-ehci.o \
					   mmc-twl4030.o \
					   twl4030-generic-scripts.o
obj-$(CONFIG_MACH_OMAP_LDP)		+= board-ldp.o \
					   mmc-twl4030.o \
					   usb-musb.o
obj-$(CONFIG_MACH_OMAP_APOLLON)		+= board-apollon.o \
					   board-apollon-mmc.o	\
					   board-apollon-keys.o
obj-$(CONFIG_MACH_NOKIA_N800)		+= board-n800.o board-n800-flash.o \
					   board-n800-mmc.o board-n800-bt.o \
					   board-n800-audio.o board-n800-usb.o \
					   board-n800-dsp.o \
					   board-n800-camera.o
obj-$(CONFIG_MACH_NOKIA_N810)		+= board-n810.o
obj-$(CONFIG_VIDEO_MACH_RX51)		+= board-rx51-camera.o
obj-$(CONFIG_MACH_NOKIA_RX51)		+= board-rx51.o \
					   board-omap-bt.o \
					   board-n800-flash.o \
					   board-rx51-audio.o \
					   board-rx51-flash.o \
					   board-rx51-sdram.o \
					   board-rx51-video.o \
					   board-rx51-network.o \
					   board-rx51-camera-base.o \
					   board-rx51-peripherals.o \
					   mmc-twl4030.o \
					   ssi.o \
					   usb-musb.o
obj-$(CONFIG_MACH_NOKIA_RX71)		+= board-rx71.o \
					   board-rx71-peripherals.o

obj-$(CONFIG_MACH_OVERO)		+= board-overo.o \
					   mmc-twl4030.o \
					   usb-musb.o \
					   usb-ehci.o \
					   twl4030-generic-scripts.o
obj-$(CONFIG_MACH_OMAP3_PANDORA)	+= board-omap3pandora.o \
					   mmc-twl4030.o \
					   usb-musb.o \
					   usb-ehci.o

# TUSB 6010 chips
obj-$(CONFIG_MACH_OMAP2_TUSB6010)	+= usb-tusb6010.o

obj-$(CONFIG_RX51_CAMERA_BUTTON)	+= rx51_camera_btn.o
