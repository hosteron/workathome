# COPYRIGHT_CHUNFENG
include config.mk

CFLAGS += -I./include -Wall

#LDFLAGS += -L./ -lhilinkdevicesdk
LDFLAGS += -L./

ifeq ($(DEBUG),yes)
CFLAGS += -g -fPIC -funwind-tables
LDFLAGS += -rdynamic -ldl
else
CFLAGS += -s -O2 -fPIC
endif
CFLAGS += -Wl,-Bsymbolic
MDS_PLUG_DIR = $(INSTALL_ROOT)/usr/lib/medusa

TARGET = plug_fw_v5.so

HILINk_SRC = plug_fw_v5.c
all:$(TARGET)

plug_fw_v5.so: $(HILINk_SRC)
	$(CC) -shared -o $@ $^ $(CFLAGS) $(LDFLAGS)
#	$(CC) -shared $^ $(CFLAGS) $(LDFLAGS) -o $@ -lanl

install:
	install -d $(MDS_PLUG_DIR)
	cp -dfR $(TARGET) $(MDS_PLUG_DIR)

uninstall:
	rm -rf $(MDS_PLUG_DIR)

clean:
	rm -f $(TARGET) *.o

distclean:clean
	-rm config.mk

.PHONY : all clean install uninstall

