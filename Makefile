CFLAGS = -std=gnu99 -O2
LDFLAGS = -lm
TARGET = png_to_6847

.PHONY: all
all: $(TARGET)


TARGET_OBJS = PNG_to_6847.o lodepng.o

$(TARGET): $(TARGET_OBJS)
	mkdir -p $(dir $@)
	$(CC) $(TARGET_OBJS) $(CFLAGS) $(LDFLAGS) -o $@


PNG_to_6847.o: PNG_to_6847.c
lodepng.o: lodepng.c

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	$(RM) $(TARGET) $(CALC) $(MFCALC) *.o
