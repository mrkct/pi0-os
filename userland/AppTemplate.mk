# A template for software
# 
# Example usage:
# 
# APP_NAME := sample-app
# APP_CFLAGS :=
# APP_OBJECTS = 
# include ../AppTemplate.mk

# Warnings are enabled by default, unless explicitly disabled by the application
# This is used for third-party software
ifeq ($(DISABLE_WARNINGS),)
	APP_CFLAGS += -Wall -Wextra -Werror
endif

include ../Toolchain.mk 

.PHONY: all clean install bsp/libbsp.a

all: $(APP_NAME)

bsp/libbsp.a:
	$(MAKE) -C $(BSP_HERE)/bsp libbsp.a

$(APP_NAME): bsp/libbsp.a $(APP_OBJECTS)
	@echo "\033[36mLinking $(APP_NAME)\033[0m"
	$(CC) $(CFLAGS) $(APP_CFLAGS) $(APP_OBJECTS) -o $(APP_NAME) -lc -lbsp -lg -lgcc -lm

clean:
	$(RM) $(APP_NAME)
	$(RM) $(APP_OBJECTS)
	$(RM) $(APP_OBJECTS:.o=.d)
	$(MAKE) -C $(BSP_HERE)/bsp clean

install: $(APP_NAME)
	cp $(APP_NAME) $(MOUNT_POINT)/bina/

%.c.o: %.c
	@echo "\033[36mCompiling $<\033[0m"
	$(CC) $(CFLAGS) $(APP_CFLAGS) -c $< -MMD -MP -MF $(@:.o=.d) -o $@

-include $(APP_OBJECTS:.o=.d)
