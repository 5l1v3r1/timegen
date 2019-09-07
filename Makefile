OBJECTS = main.o
LIBS = -lm
TARGET = timegen

default: timegen

%.o: %.c
	gcc -c $< -o $@ -std=gnu99

timegen: $(OBJECTS)
	gcc $(OBJECTS) $(LIBS) -o $(TARGET)

clean:
	rm -f $(OBJECTS)
	rm -f $(TARGET)