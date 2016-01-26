all: angerball

angerball: Sample_GL3_2D.cpp glad.c
	g++ -o angerball Sample_GL3_2D.cpp glad.c -lGLEW -lGL -ldl -lglfw

clean:
	rm angerball
