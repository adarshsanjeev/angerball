all: angerball

angerball: angerball.cpp glad.c Sample_GL.frag Sample_GL.vert
	g++ -o angerball angerball.cpp glad.c -lGLEW -lGL -ldl -lglfw

clean:
	rm angerball
