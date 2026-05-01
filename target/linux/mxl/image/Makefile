# SPDX-License-Identifier: GPL-2.0-only
#
# Image-build umbrella for the mxl target. Subtarget-specific device
# blocks live in image/<subtarget>.mk.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/image.mk

# CONFIG_PHYSICAL_START in the kernel config. Vendor U-Boot ignores
# ih_ep, so we keep entry == load to remain compatible with any other /
# future loader that does honour it.
KERNEL_LOADADDR := 0x02000000

# Test boot from U-Boot:
#   tftpboot 0x8200000 <device>-initramfs-kernel.itb
#   setenv bootargs "earlycon=lantiq,mmio32,0xe0a00000 console=ttyLTQ0,115200n8r"
#   bootm 0x8200000
define Device/Default
  PROFILES		= Default $$(DEVICE_NAME)
  KERNEL_NAME		:= vmlinux
  KERNEL_LOADADDR	:= $(KERNEL_LOADADDR)
  DEVICE_DTS_DIR 	:= ../dts
  DEVICE_DTS_LOADADDR	:= 0x08000000
  KERNEL		= kernel-bin | gzip | \
			  fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS	= kernel-bin | gzip | \
			  fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_SUFFIX		:= .itb
  KERNEL_INITRAMFS_SUFFIX := .itb
  FILESYSTEMS		:= squashfs
  IMAGES		:= sysupgrade.bin
  IMAGE/sysupgrade.bin	:= append-kernel | append-rootfs | pad-rootfs | append-metadata
endef

include $(SUBTARGET).mk

$(eval $(call BuildImage))
