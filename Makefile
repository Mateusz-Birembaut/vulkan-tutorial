
CFLAGS = -std=c++17 -O2 -DNDEBUG

LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

vkTuto: *.cpp *.h
	g++ $(CFLAGS) -o vkTuto *.cpp $(LDFLAGS)


.PHONY: test clean

test: vkTuto
	./vkTuto

clean:
	rm -f vkTuto