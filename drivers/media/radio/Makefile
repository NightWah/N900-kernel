#
# Makefile for the kernel character device drivers.
#

obj-$(CONFIG_RADIO_AZTECH) += radio-aztech.o
obj-$(CONFIG_RADIO_RTRACK2) += radio-rtrack2.o
obj-$(CONFIG_RADIO_SF16FMI) += radio-sf16fmi.o
obj-$(CONFIG_RADIO_SF16FMR2) += radio-sf16fmr2.o
obj-$(CONFIG_RADIO_CADET) += radio-cadet.o
obj-$(CONFIG_RADIO_TYPHOON) += radio-typhoon.o
obj-$(CONFIG_RADIO_TERRATEC) += radio-terratec.o
obj-$(CONFIG_RADIO_MAXIRADIO) += radio-maxiradio.o
obj-$(CONFIG_RADIO_RTRACK) += radio-aimslab.o
obj-$(CONFIG_RADIO_ZOLTRIX) += radio-zoltrix.o
obj-$(CONFIG_RADIO_GEMTEK) += radio-gemtek.o
obj-$(CONFIG_RADIO_GEMTEK_PCI) += radio-gemtek-pci.o
obj-$(CONFIG_RADIO_TRUST) += radio-trust.o
obj-$(CONFIG_I2C_SI4713) += fmtx-si4713.o
fmtx-si4713-objs := radio-si4713.o si4713.o
obj-$(CONFIG_RADIO_MAESTRO) += radio-maestro.o
obj-$(CONFIG_USB_DSBR) += dsbr100.o
obj-$(CONFIG_I2C_BCM2048) += radio-bcm2048.o
obj-$(CONFIG_USB_SI470X) += radio-si470x.o
obj-$(CONFIG_USB_MR800) += radio-mr800.o

EXTRA_CFLAGS += -Isound
