# SPDX-License-Identifier: GPL-2.0-only

define Device/zyxel_xs1930
  KERNEL_LOADADDR = 0x80220000
  KERNEL_ENTRY = 0x80220000
  SOC := rtl9313
  IMAGE_SIZE := 14336k
  DEVICE_VENDOR := ZyXEL
  DEVICE_MODEL := XS1930 series
  ZYXEL_VERS := ABQF
  DEVICE_PACKAGES := ip-bridge ethtool
  KERNEL_INITRAMFS := kernel-bin | append-dtb | gzip | zyxel-vers $$$$(ZYXEL_VERS) | \
	uImage gzip
endef
TARGET_DEVICES += zyxel_xs1930
