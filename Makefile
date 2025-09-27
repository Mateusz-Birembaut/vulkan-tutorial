
CFLAGS = -std=c++17 -O2 

LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

vkTuto: *.cpp *.h
	g++ $(CFLAGS) -o vkTuto *.cpp $(LDFLAGS)


.PHONY: test test-fps clean

test: vkTuto
	./vkTuto

test-fps: vkTuto
	MANGOHUD=1 ./vkTuto

clean:
	rm -f vkTuto