SUBDIRS = mqtt_bridge

all: $(SUBDIRS)

$(SUBDIRS):
		$(MAKE) -C $@

.PHONY: $(SUBDIRS)
