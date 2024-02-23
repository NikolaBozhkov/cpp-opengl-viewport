project:
	g++ -std=c++20 main.cpp include/imgui/imgui*.cpp -o run -I include -I include/imgui -I include/SDL2 -L lib -l SDL2-2.0.0 -framework OpenGL
