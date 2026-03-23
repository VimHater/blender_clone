
main : main.cpp
	g++ main.cpp ./rlImGui/*.cpp ./imgui/*.cpp -o ./bin/main \
	-I./imgui/ \
	-I./raylib-linux/include/ \
	-L./raylib-linux/lib/ \
	-Wl,-rpath,./raylib-linux/lib/ \
	-lraylib -lGL -lm -lpthread -ldl -lrt -lX11
