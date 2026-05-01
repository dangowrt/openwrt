# SPDX-License-Identifier: GPL-2.0-only
#
# OpenWrt target for MaxLinear x86_64 gateway SoCs (Lightning Mountain
# family, etc.). These x86_64 SoCs boot via U-Boot with a flattened
# device tree — no ACPI, no UEFI — so they live in their own target
# rather than as a subtarget of x86.
#

include $(TOPDIR)/rules.mk

ARCH:=x86_64
BOARD:=mxl
BOARDNAME:=MaxLinear
FEATURES:=squashfs ext4 ramdisk fpu rtc gpio pci usb emmc
SUBTARGETS:=lgm

KERNEL_PATCHVER:=6.18

# Build a bzImage so make has a target. We package the raw
# `objcopy -O binary vmlinux` output (= $(KDIR)/vmlinux from
# Kernel/CopyImage) — gzip'd and wrapped in a FIT image alongside the
# DTB; U-Boot decompresses to KERNEL_LOADADDR (= entry).
KERNELNAME:=bzImage

include $(INCLUDE_DIR)/target.mk

DEFAULT_PACKAGES += \
	kmod-gpio-button-hotplug \
	uboot-envtools

$(eval $(call BuildTarget))
