TARGET_BD = udp_server_bd
TARGET_PC = udp_client_pc
CC_BD = aarch64-linux-gnu-gcc
CC_PC = gcc
CFLAGS = -Wall

TARGETS = $(TARGET_BD) $(TARGET_PC)
all: $(TARGETS)
	cp $(TARGET_BD) /nfsroot

$(TARGET_BD): $(TARGET_BD).c
	$(CC_BD) $(CFLAGS) -o $@ $<

$(TARGET_PC): $(TARGET_PC).c
	$(CC_PC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)


