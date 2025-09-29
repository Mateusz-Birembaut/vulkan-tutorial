
CFLAGS = -std=c++17 -O3 -DNDEBUG

LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

vkTuto:
	g++ $(CFLAGS) -o vkTuto *.cpp tiny_obj_loader.cc $(LDFLAGS)


.PHONY: test test-fps clean

test: vkTuto
	./compile.bat && ./vkTuto

test-fps: vkTuto
	./compile.bat && MANGOHUD=1 ./vkTuto

clean:
	rm -f vkTuto