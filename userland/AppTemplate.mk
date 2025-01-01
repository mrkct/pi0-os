# A template for software
# 
# Example usage:
# 
# APP_NAME := sample-app
# APP_CFLAGS :=
# APP_OBJECTS = 
# include ../AppTemplate.mk

include ../Toolchain.mk 

.PHONY: all clean install bsp/libbsp.a

all: $(APP_NAME)

bsp/libbsp.a:
	$(MAKE) -C $(BSP_HERE)/bsp libbsp.a

$(APP_NAME): bsp/libbsp.a $(APP_OBJECTS)
	$(CC) $(CFLAGS) $(APP_CFLAGS) $(APP_OBJECTS) -o $(APP_NAME) -lc -lbsp -lg -lgcc -lm

clean:
	$(RM) $(APP_NAME)
	$(RM) $(APP_OBJECTS)
	$(MAKE) -C $(BSP_HERE)/bsp clean

install: $(APP_NAME)
	cp $(APP_NAME) $(MOUNT_POINT)/bina/

%.c.o: %.c
	$(CC) $(CFLAGS) $(APP_CFLAGS) $< -c -o $@
