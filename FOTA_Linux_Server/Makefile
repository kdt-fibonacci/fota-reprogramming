CC = gcc

TARGET = ota_run

SRC_DIR = src
INC_DIR = includes

SRCS = \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/server.c \
	$(SRC_DIR)/ota.c \
	$(SRC_DIR)/mqtt_handler.c \
	$(SRC_DIR)/crypto.c \
	$(SRC_DIR)/file.c \
	$(SRC_DIR)/logger.c \
	$(SRC_DIR)/utils.c

CFLAGS = -I$(INC_DIR)

LIBS = \
	-L/usr/local/lib \
	-lpaho-mqtt3c \
	-lssl \
	-lcrypto \
	-lpthread \
	-lmicrohttpd

all:
	$(CC) $(SRCS) -o $(TARGET) $(CFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)