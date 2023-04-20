CXX=arm-none-eabi-g++
CXXFLAGS=-mcpu=arm1176jzf-s -fpic -ffreestanding

SOURCES=main.cpp
OBJECTS=$(addsuffix .o, $(SOURCES))

.PHONY: all clean

all: prekernel/Prekernel Kernel

prekernel/Prekernel:
	$(MAKE) -C prekernel all

Kernel: $(OBJECTS)
	$(CXX) -T linker.ld -o Kernel $(OBJS) $(CXXFLAGS)

%.cpp.o: %.cpp
	$(CXX) $< -o $@ $(CXXFLAGS)

%.S.o: %.S
	$(AS) $< -o $@ $(ASFLAGS)

clean:
	$(RM) -f $(OBJS)
	$(RM) -f Kernel
	$(MAKE) -C prekernel clean
