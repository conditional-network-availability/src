################################################################################
#
# txctl
#
################################################################################

# Note: Buildroot does not support to build a LKM only, so we need some wrapper
# application that is build alongside the module. In this case, this is just
# a hello world application.
# More information can be retrieved at:
# https://buildroot.org/downloads/manual/manual.html#_infrastructure_for_packages_building_kernel_modules

TXCTL_VERSION = 1.0.0
TXCTL_LICENSE = GPL-2.0
TXCTL_LICENSE_FILES = COPYING

TXCTL_SITE = ../../src/nw/txctl
TXCTL_SITE_METHOD = local

TXCTL_MODULE_SUBDIRS = lkm

TXCTL_INSTALL_STAGING = NO
TXCTL_INSTALL_TARGET = YES

define TXCTL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/hello $(TARGET_DIR)/usr/bin/hello
endef

$(eval $(kernel-module))
$(eval $(cmake-package))
