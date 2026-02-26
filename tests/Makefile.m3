# Makefile for M3 Module Test (Certificate Distribution)
#
# Compiles test_m3_cert_dist with strongSwan and GmSSL dependencies

# Paths
STRONGSWAN_SRC = /home/ipsec/strongswan
GMSSL_PREFIX = /usr/local
PROJECT_DIR = /home/ipsec/PQGM-IPSec

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -g -O2
CFLAGS += -include $(STRONGSWAN_SRC)/config.h
CFLAGS += -I$(STRONGSWAN_SRC)/src/libstrongswan
CFLAGS += -I$(STRONGSWAN_SRC)/src/libstrongswan/plugins/gmalg
CFLAGS += -I$(GMSSL_PREFIX)/include/gmssl
CFLAGS += -I$(PROJECT_DIR)/tests

# Linker settings
LDFLAGS = -L$(STRONGSWAN_SRC)/src/libstrongswan/.libs
LDFLAGS += -L$(GMSSL_PREFIX)/lib
LDFLAGS += -Wl,-rpath,$(STRONGSWAN_SRC)/src/libstrongswan/.libs
LDFLAGS += -Wl,-rpath,$(GMSSL_PREFIX)/lib

# Libraries
LIBS = -lstrongswan -lgmssl -lpthread -ldl -lm

# Targets
TARGET = test_m3_cert_dist

all: $(TARGET)

$(TARGET): test_m3_cert_dist.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)
	@echo "Built $@ successfully"

clean:
	rm -f $(TARGET)

test: $(TARGET)
	@echo "Running M3 module tests..."
	LD_LIBRARY_PATH=$(STRONGSWAN_SRC)/src/libstrongswan/.libs:$(GMSSL_PREFIX)/lib ./$(TARGET)

.PHONY: all clean test
