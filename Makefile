INSTALL = /usr/bin/install -c
INSTALLDATA = /usr/bin/install -c -m 644
SUBDIRS = mqtt_bridge
ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif

all: $(SUBDIRS)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
		$(MAKE) -C $@

.PHONY: install
install: mqtt_bridge
	$(INSTALL) -m 0755 mqtt_bridge/mqtt_bridge $(PREFIX)/bin
