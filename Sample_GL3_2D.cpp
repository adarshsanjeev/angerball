#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GRAV_CONST 0.009
#define MAX_POWER 1.8
#define VEL_THRE 0.02
#define MAX_ENEMY 10
#define MAX_WOOD 200

int ENEMY_NUMBER = 5;
int WOOD_NUMBER = 0;

using namespace std;

struct VAO {
	GLuint VertexArrayID;
	GLuint VertexBuffer;
	GLuint ColorBuffer;

	GLenum PrimitiveMode;
	GLenum FillMode;
	int NumVertices;
};
typedef struct VAO VAO;

struct GLMatrices {
	glm::mat4 projection;
	glm::mat4 model;
	glm::mat4 view;
	GLuint MatrixID;
} Matrices;

GLuint programID;

//####################################################################################################

struct Screen {
	double x, y, z;
} Game;

struct Player {
	int lives;
	int score;
	float wind;
	bool game_over;
} Player1;

class Character {
public:
	float x, y;
	float radius;
	glm::vec2 Vel;

	float health;
	float fric_const;
	float elast_const;
	float rest_const;
	float air_const;

	bool alive;

	VAO *sprite;
} Wood[MAX_WOOD], Enemies[MAX_ENEMY], Bird[20];

int bird_count = 0;

class Wall {
public:
	float x, y;
	float size_x, size_y;
	VAO *sprite;
} Floor, CWall;

class Bar {
public:
	float x,y;
	float size;

	VAO * sprite;
} PowerBar;

class Weapon {
public:
	float x,y;
	float angle;
	float power;
	VAO *base;
	VAO *barrel;
} Cannon;

float clamp(float value, float min, float max) {
	return std::max(min, std::min(max, value));
}

bool MoveFixedColl(Wall& W, Character& C)
{
	glm::vec2 center(C.x, C.y);
	glm::vec2 aabb_half_extents(W.size_x, W.size_y);
	glm::vec2 aabb_center(W.x, W.y);
	glm::vec2 difference = center - aabb_center;
	glm::vec2 clamped = glm::clamp(difference, -aabb_half_extents, aabb_half_extents);
	glm::vec2 closest = aabb_center + clamped;
	difference = closest - center;

	float overlap = glm::length(difference)-C.radius;
	glm::vec2 offset = glm::normalize(difference)*overlap;
	glm::vec2 rebound = glm::normalize(difference)*-0.01f;
	// offset[1] -= 0.01;
	// if (glm::length(difference) < C.radius) {
	// 	C.x -= offset[0];
	// 	C.y -= offset[1];
	// 	C.Vel[0] *= C.fric_const;
	// 	if(C.Vel[1] < 0)
	// 		C.Vel[1] = C.rest_const*abs(C.Vel[1]);
	// 	if(abs(C.Vel[1]) < VEL_THRE)
	// 		C.Vel[1] = 0;
	// }
	// else
	// 	C.Vel *= C.air_const;

	if (C.y < -2.9)
		C.y += 0.1;

	enum direction {UP=0, LEFT=1, DOWN=2, RIGHT=3};
	float dot_prod[4];
	dot_prod[0] = glm::dot(difference, glm::vec2(0, 1));
	dot_prod[1] = glm::dot(difference, glm::vec2(-1, 0));
	dot_prod[2] = glm::dot(difference, glm::vec2(0, -1));
	dot_prod[3] = glm::dot(difference, glm::vec2(1, 0));
	if (glm::length(difference) <= C.radius) {
		switch(distance(dot_prod, max_element(dot_prod, dot_prod+4))) {
		case UP:
		case DOWN:
			C.Vel[0] *= C.fric_const;
			C.Vel[1] = C.rest_const*C.Vel[1]*-1;
			if(abs(C.Vel[1]) < VEL_THRE)
				C.Vel[1] = 0;
			break;
		case LEFT:
		case RIGHT:
			C.Vel[0] += rebound[0];
			C.x += offset[0]*2;
			C.y += offset[1];
			C.Vel[1] *= C.fric_const;
			C.Vel[0] = C.rest_const*C.Vel[0]*-1;
			if(abs(C.Vel[0]) < VEL_THRE)
				C.Vel[0] = 0;
			break;
		}
		return true;
	}
	else {
		C.Vel *= C.air_const;
		return false;
	}
}

/**
   killB = 0, Don't kill B
   killB = 1, kill B
   killB = 2, Dunno
   killB = 3, A acts like fixed
*/
bool MovMovColl(Character& A, Character& B, int killB)
{
	if(!B.alive && killB != 2)
		return false;
	if(dot(A.Vel, B.Vel) < 0)
		return false;

	glm::vec2 CharA(A.x, A.y);
	glm::vec2 CharB(B.x, B.y);
	glm::vec2 difference = CharB - CharA;

	float offset = glm::length(difference) - (A.radius + B.radius);

	if (glm::length(difference) <= (A.radius + B.radius)) {

		glm::vec2 change = glm::normalize(difference)*offset;
		glm::vec2 rebound = glm::normalize(difference)*-0.007f;
		glm::vec2 Total = A.Vel + B.Vel;

		if (A.y < B.y)
			Total *= -1;
		if (killB != 3) {
			A.x += change[0];
			A.y += change[1];
			A.Vel = -0.5f * Total;
			A.Vel[0] += rebound[0];
		}
		else
			Total[1] *= 0;
		B.Vel = 0.4f * Total;
		if(abs(A.Vel[0]) < VEL_THRE)
			A.Vel[0] = 0;
		if(abs(A.Vel[1]) < VEL_THRE)
			A.Vel[1] = 0;
		if(abs(B.Vel[0]) < VEL_THRE)
			A.Vel[0] = 0;
		if(abs(B.Vel[1]) < VEL_THRE)
			A.Vel[1] = 0;
		if(killB && killB != 3) {
			B.alive = 0;
			Player1.score += 10;
		}
		return true;
	}
	return false;
}

void gravity() {

	for (int j=0; j<bird_count; j++)
		Bird[j].Vel[1] -= GRAV_CONST;
	for (int i=0; i<ENEMY_NUMBER; i++)
		Enemies[i].Vel[1] -= GRAV_CONST;

	// Bird, Enemy
	for (int i=0; i<ENEMY_NUMBER; i++)
		for (int j=0; j<bird_count; j++)
			MovMovColl(Bird[j], Enemies[i], 1);

	// Enemy, Enemy
	for (int i=0; i<ENEMY_NUMBER; i++)
		for(int j=i+1; j<ENEMY_NUMBER; j++)
			MovMovColl(Enemies[i], Enemies[j], 0);

	// Wood Bird
	for (int i=0; i<WOOD_NUMBER; i++)
		for (int j=0; j<bird_count; j++) {
			if (Wood[i].alive)
				MovMovColl(Bird[j], Wood[i], 2);
			if (!Wood[i].alive)
				Wood[i].Vel[1] -= GRAV_CONST*0.1;
		}

	//Enemy Wood
	for (int i=0; i<ENEMY_NUMBER; i++)
		for(int j=0; j<WOOD_NUMBER; j++)
			if (Wood[j].alive)
				MovMovColl(Wood[j], Enemies[i], 3);

	// Bird Floor
	for (int j=0; j<bird_count; j++)
		MoveFixedColl(Floor, Bird[j]);

	// Bird Wall
	for (int j=0; j<bird_count; j++)
		MoveFixedColl(CWall, Bird[j]);

	// Enemy Wall
	for (int i=0; i<ENEMY_NUMBER; i++)
		MoveFixedColl(CWall, Enemies[i]);

	// Enemy Floor
	for (int i=0; i<ENEMY_NUMBER; i++)
		MoveFixedColl(Floor, Enemies[i]);

	for (int j=0; j<bird_count; j++) {
		Bird[j].Vel[0] = Bird[j].Vel[0]>0.3? 0.3:Bird[j].Vel[0];
		Bird[j].Vel[1] = Bird[j].Vel[1]>0.3? 0.3:Bird[j].Vel[1];

		Bird[j].x += Bird[j].Vel[0];
		Bird[j].y += Bird[j].Vel[1];
	}

	for (int i=0; i<ENEMY_NUMBER; i++) {
		Enemies[i].x += Enemies[i].Vel[0];
		Enemies[i].y += Enemies[i].Vel[1];
	}
	for (int i=0; i<WOOD_NUMBER; i++) {
		Wood[i].x += Wood[i].Vel[0];
		Wood[i].y += Wood[i].Vel[1];
	}
}

//####################################################################################################

/* Function to load Shaders - Use it as it is */
GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path) {

	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code from the file
	std::string VertexShaderCode;
	std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
	if(VertexShaderStream.is_open())
	{
		std::string Line = "";
		while(getline(VertexShaderStream, Line))
			VertexShaderCode += "\n" + Line;
		VertexShaderStream.close();
	}

	// Read the Fragment Shader code from the file
	std::string FragmentShaderCode;
	std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
	if(FragmentShaderStream.is_open()){
		std::string Line = "";
		while(getline(FragmentShaderStream, Line))
			FragmentShaderCode += "\n" + Line;
		FragmentShaderStream.close();
	}

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile Vertex Shader
	printf("Compiling shader : %s\n", vertex_file_path);
	char const * VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
	glCompileShader(VertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> VertexShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &VertexShaderErrorMessage[0]);

	// Compile Fragment Shader
	printf("Compiling shader : %s\n", fragment_file_path);
	char const * FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
	glCompileShader(FragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> FragmentShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &FragmentShaderErrorMessage[0]);

	// Link the program
	fprintf(stdout, "Linking program\n");
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> ProgramErrorMessage( max(InfoLogLength, int(1)) );
	glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
	fprintf(stdout, "%s\n", &ProgramErrorMessage[0]);

	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	return ProgramID;
}

static void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

void quit(GLFWwindow *window)
{
	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}

/* Generate VAO, VBOs and return VAO handle */
struct VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat* color_buffer_data, GLenum fill_mode=GL_FILL)
{
	struct VAO* vao = new struct VAO;
	vao->PrimitiveMode = primitive_mode;
	vao->NumVertices = numVertices;
	vao->FillMode = fill_mode;

	// Create Vertex Array Object
	// Should be done after CreateWindow and before any other GL calls
	glGenVertexArrays(1, &(vao->VertexArrayID)); // VAO
	glGenBuffers (1, &(vao->VertexBuffer)); // VBO - vertices
	glGenBuffers (1, &(vao->ColorBuffer));  // VBO - colors

	glBindVertexArray (vao->VertexArrayID); // Bind the VAO
	glBindBuffer (GL_ARRAY_BUFFER, vao->VertexBuffer); // Bind the VBO vertices
	glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), vertex_buffer_data, GL_STATIC_DRAW); // Copy the vertices into VBO
	glVertexAttribPointer(
		0,                  // attribute 0. Vertices
		3,                  // size (x,y,z)
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		0,                  // stride
		(void*)0            // array buffer offset
		);

	glBindBuffer (GL_ARRAY_BUFFER, vao->ColorBuffer); // Bind the VBO colors
	glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), color_buffer_data, GL_STATIC_DRAW);  // Copy the vertex colors
	glVertexAttribPointer(
		1,                  // attribute 1. Color
		3,                  // size (r,g,b)
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		0,                  // stride
		(void*)0            // array buffer offset
		);

	return vao;
}

/* Generate VAO, VBOs and return VAO handle - Common Color for all vertices */
struct VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat red, const GLfloat green, const GLfloat blue, GLenum fill_mode=GL_FILL)
{
	GLfloat* color_buffer_data = new GLfloat [3*numVertices];
	for (int i=0; i<numVertices; i++) {
		color_buffer_data [3*i] = red;
		color_buffer_data [3*i + 1] = green;
		color_buffer_data [3*i + 2] = blue;
	}

	return create3DObject(primitive_mode, numVertices, vertex_buffer_data, color_buffer_data, fill_mode);
}

/* Render the VBOs handled by VAO */
void draw3DObject (struct VAO* vao)
{
	// Change the Fill Mode for this object
	glPolygonMode (GL_FRONT_AND_BACK, vao->FillMode);

	// Bind the VAO to use
	glBindVertexArray (vao->VertexArrayID);

	// Enable Vertex Attribute 0 - 3d Vertices
	glEnableVertexAttribArray(0);
	// Bind the VBO to use
	glBindBuffer(GL_ARRAY_BUFFER, vao->VertexBuffer);

	// Enable Vertex Attribute 1 - Color
	glEnableVertexAttribArray(1);
	// Bind the VBO to use
	glBindBuffer(GL_ARRAY_BUFFER, vao->ColorBuffer);

	// Draw the geometry !
	glDrawArrays(vao->PrimitiveMode, 0, vao->NumVertices); // Starting from vertex 0; 3 vertices total -> 1 triangle
}

/**************************
 * Customizable functions *
 **************************/

float triangle_rot_dir = 1;
float rectangle_rot_dir = 1;
bool triangle_rot_status = true;
bool rectangle_rot_status = true;

void createBird (int index, float x, float y)
{
	Bird[index].health = 5;
	Bird[index].x = x;
	Bird[index].alive = 1;
	Bird[index].y = y;
	Bird[index].radius = 0.2;
	Bird[index].Vel = glm::vec2(0, 0);
	Bird[index].fric_const = 0.96;
	Bird[index].rest_const = 0.7;
	Bird[index].air_const = 0.999;

	static GLfloat vertex_buffer_data [3*365];
	static GLfloat color_buffer_data [3*365];

	int k = 0;
	vertex_buffer_data[0] = 0;
	color_buffer_data[0] = 0.8;
	k++;
	vertex_buffer_data[1] = 0;
	color_buffer_data[1] = 0;
	k++;
	vertex_buffer_data[2] = 0;
	color_buffer_data[2] = 0;
	k++;

	for (int i=0; i<=360; ++i)
	{
		vertex_buffer_data[k] = cos((i*M_PI)/180)*Bird[index].radius;
		color_buffer_data[k] = 0.9;
		k++;
		vertex_buffer_data[k] = sin((i*M_PI)/180)*Bird[index].radius;
		color_buffer_data[k] = 0;
		k++;
		vertex_buffer_data[k] = 0;
		color_buffer_data[k] = 0;
		k++;
	}

	// create3DObject creates and returns a handle to a VAO that can be used later
	Bird[index].sprite = create3DObject(GL_TRIANGLE_FAN, 362, vertex_buffer_data, color_buffer_data, GL_FILL);
}

void fire_bird() {
	if(bird_count >= 15)
		return;
	createBird(bird_count, Cannon.x, Cannon.y);
	Bird[bird_count].Vel = glm::vec2(
		Cannon.power * 0.2 * cos((Cannon.angle*M_PI)/180),
		Cannon.power * 0.2 * sin((Cannon.angle*M_PI)/180)
		);
	bird_count++;
}

float zoom = 4.0, pan = 0.0;

/* Executed when a regular key is pressed/released/held-down */
/* Prefered for Keyboard events */
void keyboard (GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// Function is called first on GLFW_PRESS.

	if (action == GLFW_RELEASE) {
		switch (key) {
		case GLFW_KEY_C:
			rectangle_rot_status = !rectangle_rot_status;
			break;
		case GLFW_KEY_P:
			triangle_rot_status = !triangle_rot_status;
			break;
		case GLFW_KEY_SPACE:
			fire_bird();
			Cannon.power = 0;
			break;
		}
	}
	else if (action == GLFW_PRESS) {
		switch (key) {
		case GLFW_KEY_ESCAPE:
			quit(window);
			break;
		}
	}
	else if (action == GLFW_REPEAT) {
		switch (key) {
		case GLFW_KEY_UP:
			zoom -= 0.5;
			zoom = max(0.5f, zoom);
			break;
		case GLFW_KEY_DOWN:
			zoom += 0.5;
			zoom = min(6.5f, zoom);
			break;
		case GLFW_KEY_LEFT:
			Game.x -= 0.1;
			Game.x = max(0.0, Game.x);
			PowerBar.x -= 0.1;
			PowerBar.x = 0.0>PowerBar.x? 0.0:PowerBar.x;
			break;
		case GLFW_KEY_RIGHT:
			Game.x += 0.1;
			Game.x = min(4.4, Game.x);
			PowerBar.x += 0.1;
			PowerBar.x = 4.4<PowerBar.x? 4.4:PowerBar.x;
			break;
		case GLFW_KEY_SPACE:
			Cannon.power += 0.1;
			Cannon.power = Cannon.power<MAX_POWER ? Cannon.power:MAX_POWER;
			break;
		}
	}
}

void resetGame();

/* Executed for character input (like in text boxes) */
void keyboardChar (GLFWwindow* window, unsigned int key)
{
	switch (key) {
	case 'Q':
	case 'q':
		quit(window);
		break;
	case 'r':
		resetGame();
		break;
	case 'a':
		Cannon.angle += 2;
		Cannon.angle = Cannon.angle>90? 90:Cannon.angle;
		break;
	case 'b':
		Cannon.angle -= 2;
		Cannon.angle = Cannon.angle<-90? -90:Cannon.angle;
		break;
	case 'f':
		Cannon.power += 0.1;
		Cannon.power = Cannon.power<MAX_POWER ? Cannon.power:MAX_POWER;
		break;
	case 's':
		Cannon.power -= 0.1;
		Cannon.power = Cannon.power>0 ? Cannon.power:0;
		break;
	case 'i':
		zoom += 0.5;
		zoom = min(6.5f, zoom);
		break;
	case 'k':
		zoom -= 0.5;
		zoom = max(0.5f, zoom);
		break;
	case 'j':
		pan+=0.5;
		break;
	case 'l':
		pan-=0.5;
		break;
	}
}

/* Executed when a mouse button is pressed/released */
void mouseButton (GLFWwindow* window, int button, int action, int mods)
{
	switch (button) {
	case GLFW_MOUSE_BUTTON_LEFT:
		if (action == GLFW_RELEASE)
			fire_bird();
		break;
	case GLFW_MOUSE_BUTTON_RIGHT:
		if (action == GLFW_RELEASE) {
			rectangle_rot_dir *= -1;
		}
		break;
	default:
		break;
	}
}

/* Executed when window is resized to 'width' and 'height' */
/* Modify the bounds of the screen here in glm::ortho or Field of View in glm::Perspective */
void reshapeWindow (GLFWwindow* window, int width, int height)
{
	int fbwidth=width, fbheight=height;
	/* With Retina display on Mac OS X, GLFW's FramebufferSize
	   is different from WindowSize */
	glfwGetFramebufferSize(window, &fbwidth, &fbheight);

	GLfloat fov = 90.0f;

	// sets the viewport of openGL renderer
	glViewport (0, 0, (GLsizei) fbwidth, (GLsizei) fbheight);

	// set the projection matrix as perspective
	/* glMatrixMode (GL_PROJECTION);
	   glLoadIdentity ();
	   gluPerspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 0.1, 500.0); */
	// Store the projection matrix in a variable for future use
	// Perspective projection for 3D views
	// Matrices.projection = glm::perspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 0.1f, 500.0f);

	pan = zoom;

	// Ortho projection for 2D views
	Matrices.projection = glm::ortho(zoom*-1.0f - pan, zoom + pan, zoom*-1.0f, zoom, 0.1f, 500.0f);
	// Matrices.projection = glm::ortho(-4.0f, 4.0f, -4.0f, 4.0f, 0.1f, 500.0f);
}

VAO *triangle, *rectangle;

// Creates the triangle object used in this sample code
void createTriangle ()
{
	/* ONLY vertices between the bounds specified in glm::ortho will be visible on screen */

	/* Define vertex array as used in glBegin (GL_TRIANGLES) */
	static const GLfloat vertex_buffer_data [] = {
		0, 1,0, // vertex 0
		-1,-1,0, // vertex 1
		1,-1,0, // vertex 2
	};

	static const GLfloat color_buffer_data [] = {
		1,0,0, // color 0
		0,1,0, // color 1
		0,0,1, // color 2
	};

	// create3DObject creates and returns a handle to a VAO that can be used later
	triangle = create3DObject(GL_TRIANGLES, 3, vertex_buffer_data, color_buffer_data, GL_LINE);
}

// Creates the rectangle object used in this sample code
void createRectangle ()
{
	// GL3 accepts only Triangles. Quads are not supported
	static const GLfloat vertex_buffer_data [] = {
		-1.2,-1,0, // vertex 1
		1.2,-1,0, // vertex 2
		1.2, 1,0, // vertex 3

		1.2, 1,0, // vertex 3
		-1.2, 1,0, // vertex 4
		-1.2,-1,0  // vertex 1
	};

	static const GLfloat color_buffer_data [] = {
		1,0,0, // color 1
		0,0,1, // color 2
		0,1,0, // color 3

		0,1,0, // color 3
		0.3,0.3,0.3, // color 4
		1,0,0  // color 1
	};

	// create3DObject creates and returns a handle to a VAO that can be used later
	rectangle = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

void createEnemies ()
{
	for (int i=0; i<ENEMY_NUMBER ;i++) {
		Enemies[i].x = i;
		Enemies[i].y = 0;
		Enemies[i].radius = 0.2;
		Enemies[i].Vel = glm::vec2(0, 0);
		Enemies[i].alive = 1;
		Enemies[i].fric_const = 0.96;
		Enemies[i].rest_const = 0.7;
		Enemies[i].air_const = 0.95;

		GLfloat vertex_buffer_data [3*365];
		GLfloat color_buffer_data [3*365];

		int k = 0;
		vertex_buffer_data[0] = 0;
		color_buffer_data[0] = 0;
		k++;
		vertex_buffer_data[1] = 0;
		color_buffer_data[1] = 0.8;
		k++;
		vertex_buffer_data[2] = 0;
		color_buffer_data[2] = 0.4;
		k++;

		for (int j=0; j<=360; ++j)
		{
			vertex_buffer_data[k] = cos((j*M_PI)/180)*Enemies[i].radius;
			color_buffer_data[k] = 0;
			k++;
			vertex_buffer_data[k] = sin((j*M_PI)/180)*Enemies[i].radius;
			color_buffer_data[k] = 0.5;
			k++;
			vertex_buffer_data[k] = 0;
			color_buffer_data[k] = 0;
			k++;
		}

		// create3DObject creates and returns a handle to a VAO that can be used later
		Enemies[i].sprite = create3DObject(GL_TRIANGLE_FAN, 362, vertex_buffer_data, color_buffer_data, GL_FILL);
	}
}

void createWall ()
{
	CWall.x = 0;
	CWall.y = -2;
	CWall.size_x = 0.5;
	CWall.size_y = 1.09;

	// GL3 accepts only Triangles. Quads are not supported
	static const GLfloat vertex_buffer_data [] = {
		-0.4,-1,0, // vertex 1
		0.4, 1,0, // vertex 2
		0.4, -1,0, // vertex 3

		-0.4,-1,0, // vertex 3
		-0.4, 1,0, // vertex 4
		0.4, 1,0  // vertex 1
	};

	static const GLfloat color_buffer_data [] = {
		1,0,0, // color 1
		1,0,0, // color 2
		1,0,0, // color 3

		1,0,0, // color 3
		1,0,0, // color 4
		1,0,0  // color 1
	};

	// create3DObject creates and returns a handle to a VAO that can be used later
	CWall.sprite = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

void createFloor ()
{
	Floor.x = 0;
	Floor.y = -3.5;
	Floor.size_x = 400;
	Floor.size_y = 0.5;

	// GL3 accepts only Triangles. Quads are not supported
	static const GLfloat vertex_buffer_data [] = {
		-400,-40,0, // vertex 1
		400,-40,0, // vertex 2
		400, -3,0, // vertex 3

		400, -3,0, // vertex 3
		-400, -3,0, // vertex 4
		-400,-40,0  // vertex 1
	};

	static const GLfloat color_buffer_data [] = {
		0.417,0.178,0.15, // color 1
		0.417,0.178,0.15, // color 2
		0.417,0.178,0.15, // color 3

		0.417,0.178,0.15, // color 3
		0.417,0.178,0.15, // color 4
		0.417,0.178,0.15  // color 1
	};

	// create3DObject creates and returns a handle to a VAO that can be used later
	Floor.sprite = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

void createWood ()
{
    WOOD_NUMBER = 200;
	{
		for (int i=0; i<MAX_WOOD ;i++) {
			if(i<30) {
				Wood[i].x = 1.5+i*0.2;
				Wood[i].y = -1.1;
			}
			else if(i<60) {
				Wood[i].x = 1.5+(i-30)*0.2;
				Wood[i].y = 0.9;
			}
			else if (i<80) {
				Wood[i].x = 1.5;
				Wood[i].y = -2.9+(i-60)*0.2;
			}
			else if (i<100) {
				Wood[i].x = 4.5;
				Wood[i].y = -2.9+(i-80)*0.2;
			}
			else if (i<120) {
				Wood[i].x = 7.5;
				Wood[i].y = -2.9+(i-100)*0.2;
			}
			// for (int i=0; i<WOOD_NUMBER ;i++) {
			// 	if (i<20) {
			// 		Wood[i].x = 0.9+i*0.2;
			// 		Wood[i].y = -1.3;
			// 	}
			// 	else if (i<30) {
			// 		Wood[i].x = 0.9;
			// 		Wood[i].y = -1.3+(i-19)*0.2;
			// }
			Wood[i].radius = 0.2;
			Wood[i].Vel = glm::vec2(0, 0);
			Wood[i].alive = 1;
			Wood[i].fric_const = 0.96;
			Wood[i].rest_const = 0.7;
			Wood[i].air_const = 0.95;

			static const GLfloat vertex_buffer_data [] = {
				-0.1,-0.1,0, // vertex 1
				0.1,-0.1,0, // vertex 2
				0.1, 0.1,0, // vertex 3

				0.1, 0.1,0, // vertex 3
				-0.1, 0.1,0, // vertex 4
				-0.1,-0.1,0  // vertex 1
			};

			static const GLfloat color_buffer_data [] = {
				0.6,0.3,0, // color 1
				0.6,0.3,0, // color 2
				0.6,0.3,0, // color 3

				0.6,0.3,0, // color 3
				0.6,0.3,0, // color 4
				0.6,0.3,0  // color 1
			};

			// create3DObject creates and returns a handle to a VAO that can be used later
			Wood[i].sprite = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
		}
	}
}

void createCannon()
{
	Cannon.x = -3.5;
	Cannon.y = -2.7;
	Cannon.angle = 0;
	Cannon.power = 0;

	const GLfloat vertex_buffer_data [] = {
		0, 0.35,0, // vertex 0
		-0.35,-0.3,0, // vertex 1
		0.35,-0.3,0, // vertex 2
	};
	const GLfloat color_buffer_data [] = {
		0,0,0, // color 0
		0,0,0, // color 1
		0,0,0, // color 2
	};
	Cannon.base = create3DObject(GL_TRIANGLES, 3, vertex_buffer_data, color_buffer_data, GL_FILL);

	static const GLfloat vertex_barrel_data [] = {
		-0.5,-0.15,0, // vertex 1
		0.5,-0.15,0, // vertex 2
		0.5, 0.15,0, // vertex 3

		0.5, 0.15,0, // vertex 3
		-0.5, 0.15,0, // vertex 4
		-0.5,-0.15,0  // vertex 1
	};

	static const GLfloat color_barrel_data [] = {
		1,0,0, // color 1
		1,0,0, // color 2
		1,0,0, // color 3

		1,0,0, // color 3
		1,0,0, // color 4
		1,0,0  // color 1
	};

	Cannon.barrel = create3DObject(GL_TRIANGLES, 6, vertex_barrel_data, color_barrel_data, GL_FILL);


}

void createTehPower () {
	PowerBar.x = 3.6;
	PowerBar.y = 3.5;

	static const GLfloat vertex_buffer_data [] = {
		-0.5,-0.15,0, // vertex 1
		0.5,-0.15,0, // vertex 2
		0.5, 0.15,0, // vertex 3

		0.5, 0.15,0, // vertex 3
		-0.5, 0.15,0, // vertex 4
		-0.5,-0.15,0  // vertex 1
	};

	static const GLfloat color_buffer_data [] = {
		1,0.5,0, // color 1
		1,0.5,0, // color 2
		1,0.5,0, // color 3

		1,0.5,0, // color 3
		1,0.5,0, // color 4
		1,0.5,0  // color 1
	};
	PowerBar.sprite = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

class Scenery {
public:
	float x,y;
	VAO *grass, *mountain;
	VAO *sun;
} Scene;



void createScene()
{
	GLfloat vertex_grass_data [] = {
		0, 0, 0, // vertex 3
		-0.1732*2, 0.4,0, // vertex 4
		0.1732*2, 0.4,0  // vertex 1
	};

	GLfloat color_grass_data [] = {
		0.25,0.5,0, // color 1
		0.25,0.5,0, // color 2
		0.25,0.5,0, // color 3
	};
	Scene.grass = create3DObject(GL_TRIANGLES, 3, vertex_grass_data, color_grass_data, GL_FILL);
	GLfloat vertex_mountain_data [] = {
		0, 0, 0, // vertex 3
		-1.732*2, -4,0, // vertex 4
		1.732*2, -4,0  // vertex 1
	};

	GLfloat color_mountain_data [] = {
		0.25,0,0.5, // color 1
		0.25,0,0.5, // color 2
		0.25,0,0.5, // color 3
	};
	Scene.mountain = create3DObject(GL_TRIANGLES, 3, vertex_mountain_data, color_mountain_data, GL_FILL);

	GLfloat vertex_sun_data [3*365];
	GLfloat color_sun_data [3*365];

	int k = 0;
	vertex_sun_data[0] = 0;
	color_sun_data[0] = 1;
	k++;
	vertex_sun_data[1] = 0;
	color_sun_data[1] = 1;
	k++;
	vertex_sun_data[2] = 0;
	color_sun_data[2] = 0;
	k++;

	for (int j=0; j<=360; ++j)
	{
		vertex_sun_data[k] = cos((j*M_PI)/180)*0.5;
		color_sun_data[k] = 1;
		k++;
		vertex_sun_data[k] = sin((j*M_PI)/180)*0.5;
		color_sun_data[k] = 0.6;
		k++;
		vertex_sun_data[k] = 0;
		color_sun_data[k] = 0;
		k++;
	}

	// create3DObject creates and returns a handle to a VAO that can be used later
	Scene.sun = create3DObject(GL_TRIANGLE_FAN, 362, vertex_sun_data, color_sun_data, GL_FILL);
}

float camera_rotation_angle = 90;
float rectangle_rotation = 0;
float triangle_rotation = 0;

/* Render the scene with openGL */
/* Edit this function according to your assignment */
void draw ()
{
	// clear the color and depth in the frame buffer
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// use the loaded shader program
	// Don't change unless you know what you are doing
	glUseProgram (programID);

	// Eye - Location of camera. Don't change unless you are sure!!
	glm::vec3 eye ( 5*cos(camera_rotation_angle*M_PI/180.0f), 0, 5*sin(camera_rotation_angle*M_PI/180.0f) );
	// Target - Where is the camera looking at.  Don't change unless you are sure!!
	glm::vec3 target (0, 0, 0);
	// Up - Up vector defines tilt of camera.  Don't change unless you are sure!!
	glm::vec3 up (0, 1, 0);

	// Compute Camera matrix (view)
	// Matrices.view = glm::lookAt( eye, target, up ); // Rotating Camera for 3D
	//  Don't change unless you are sure!!
	Matrices.view = glm::lookAt(glm::vec3(Game.x, Game.y, Game.z), glm::vec3(Game.x, Game.y, 0), glm::vec3(0, 1, 0)); // Fixed camera for 2D (ortho) in XY plane

	// Compute ViewProject matrix as view/camera might not be changed for this frame (basic scenario)
	//  Don't change unless you are sure!!
	glm::mat4 VP = Matrices.projection * Matrices.view;

	// Send our transformation to the currently bound shader, in the "MVP" uniform
	// For each model you render, since the MVP will be different (at least the M part)
	//  Don't change unless you are sure!!
	glm::mat4 MVP;	// MVP = Projection * View * Model

	// Load identity to model matrix
	Matrices.model = glm::mat4(1.0f);

	/* Render your scene */

	glm::mat4 translateTriangle = glm::translate (glm::vec3(-2.0f, 0.0f, 0.0f)); // glTranslatef
	glm::mat4 rotateTriangle = glm::rotate((float)(triangle_rotation*M_PI/180.0f), glm::vec3(0,0,1));  // rotate about vector (1,0,0)
	glm::mat4 triangleTransform = translateTriangle * rotateTriangle;
	Matrices.model *= triangleTransform;
	MVP = VP * Matrices.model; // MVP = p * V * M

	//  Don't change unless you are sure!!
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);

	// draw3DObject draws the VAO given to it using current MVP matrix
	// draw3DObject(triangle);

	// Pop matrix to undo transformations till last push matrix instead of recomputing model matrix
	// glPopMatrix ();
	Matrices.model = glm::mat4(1.0f);

	glm::mat4 translateRectangle = glm::translate (glm::vec3(2, 0, 0));        // glTranslatef
	glm::mat4 rotateRectangle = glm::rotate((float)(rectangle_rotation*M_PI/180.0f), glm::vec3(0,0,1)); // rotate about vector (-1,1,1)
	Matrices.model *= (translateRectangle * rotateRectangle);
	MVP = VP * Matrices.model;
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);

	// draw3DObject draws the VAO given to it using current MVP matrix
	// draw3DObject(rectangle);

	//####################################################################################################

	MVP = VP * glm::translate (glm::vec3(Cannon.x, Cannon.y+5, 0));
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
	draw3DObject(Scene.sun);

	for(int i=-10; i<10; i++) {
		MVP = VP * glm::translate (glm::vec3(Cannon.x-0.5+5*i, Cannon.y+3, 0));
		glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
		draw3DObject(Scene.mountain);
	}

	MVP = VP;
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
	draw3DObject(Floor.sprite);

	for(int i=-100; i<100; i++) {
		MVP = VP * glm::translate (glm::vec3(Cannon.x-0.5+0.4*i, Cannon.y-0.7, 0));
		glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
		draw3DObject(Scene.grass);
	}


	MVP = VP * glm::translate (glm::vec3(CWall.x, CWall.y, 0));
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
	draw3DObject(CWall.sprite);

	MVP = VP * glm::translate (glm::vec3(Cannon.x, Cannon.y, 0));
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
	// draw3DObject draws the VAO given to it using current MVP matrix
	draw3DObject(Cannon.base);

	for (int j=0; j<bird_count; j++) {
		MVP = VP * glm::translate (glm::vec3(Bird[j].x, Bird[j].y, 0));
		glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
		// draw3DObject draws the VAO given to it using current MVP matrix
		draw3DObject(Bird[j].sprite);
	}

	Matrices.model = glm::mat4(1.0f);
	glm::mat4 translateBarrel = glm::translate (glm::vec3(Cannon.x, Cannon.y, 0));
	glm::mat4 rotateBarrel = glm::rotate((float)(Cannon.angle*M_PI/180.0f), glm::vec3(0,0,1));
	Matrices.model *= translateBarrel * rotateBarrel;
	MVP = VP * Matrices.model;
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
	// draw3DObject draws the VAO given to it using current MVP matrix
	draw3DObject(Cannon.barrel);

	MVP = VP * glm::translate (glm::vec3(PowerBar.x, PowerBar.y, 0)) * glm::scale(glm::vec3(2*Cannon.power, 1.0f, 1.0f));
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
	// draw3DObject draws the VAO given to it using current MVP matrix
	draw3DObject(PowerBar.sprite);

	for(int i=0; i<ENEMY_NUMBER; i++) {
		if (!Enemies[i].alive)
			continue;
		MVP = VP * glm::translate (glm::vec3(Enemies[i].x, Enemies[i].y, 0));
		glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
		draw3DObject(Enemies[i].sprite);
	}

	for(int i=0; i<WOOD_NUMBER; i++) {
		MVP = VP * glm::translate (glm::vec3(Wood[i].x, Wood[i].y, 0));
		glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
		draw3DObject(Wood[i].sprite);
	}

	//####################################################################################################

	// Increment angles
	float increments = 1;

	//camera_rotation_angle++; // Simulating camera rotation
	triangle_rotation = triangle_rotation + increments*triangle_rot_dir*triangle_rot_status;
	rectangle_rotation = rectangle_rotation + increments*rectangle_rot_dir*rectangle_rot_status;
}

void scrollFunc(GLFWwindow *window, double xpos, double ypos)
{
	if(ypos == 1) {
		zoom -= 0.5;
		zoom = max(0.5f, zoom);
	}
	else {
		zoom += 0.5;
		zoom = min(6.5f, zoom);
	}
}

/* Initialise glfw window, I/O callbacks and the renderer to use */
/* Nothing to Edit here */
GLFWwindow* initGLFW (int width, int height)
{
	GLFWwindow* window; // window desciptor/handle

	glfwSetErrorCallback(error_callback);
	if (!glfwInit()) {
		exit(EXIT_FAILURE);
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(width, height, "Anger Bird", NULL, NULL);

	if (!window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
	glfwSwapInterval( 1 );

	/* --- register callbacks with GLFW --- */

	/* Register function to handle window resizes */
	/* With Retina display on Mac OS X GLFW's FramebufferSize
	   is different from WindowSize */
	glfwSetFramebufferSizeCallback(window, reshapeWindow);
	glfwSetWindowSizeCallback(window, reshapeWindow);

	/* Register function to handle window close */
	glfwSetWindowCloseCallback(window, quit);

	/* Register function to handle keyboard input */
	glfwSetKeyCallback(window, keyboard);      // general keyboard input
	glfwSetCharCallback(window, keyboardChar);  // simpler specific character handling

	/* Register function to handle mouse click */
	glfwSetMouseButtonCallback(window, mouseButton);  // mouse button clicks
	glfwSetScrollCallback(window, scrollFunc);

	return window;
}

/* Initialize the OpenGL rendering properties */
/* Add all the models to be created here */
void initGL (GLFWwindow* window, int width, int height)
{
	/* Objects should be created before any other gl function and shaders */
	// Create the models
	createTriangle (); // Generate the VAO, VBOs, vertices data & copy into the array buffer
	createRectangle ();
	createFloor ();
	createWall ();
	createCannon ();
	createTehPower ();
	createEnemies();
	createWood ();
	createScene ();
	// Create and compile our GLSL program from the shaders
	programID = LoadShaders( "Sample_GL.vert", "Sample_GL.frag" );
	// Get a handle for our "MVP" uniform
	Matrices.MatrixID = glGetUniformLocation(programID, "MVP");
	
	reshapeWindow (window, width, height);

	// Background color of the scene
	glClearColor (0.5f, 0.5f, 0.9f, 0.0f); // R, G, B, A
	glClearDepth (1.0f);

	glEnable (GL_DEPTH_TEST);
	glDepthFunc (GL_LEQUAL);

	cout << "VENDOR: " << glGetString(GL_VENDOR) << endl;
	cout << "RENDERER: " << glGetString(GL_RENDERER) << endl;
	cout << "VERSION: " << glGetString(GL_VERSION) << endl;
	cout << "GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
	xpos = xpos/100 - 4;
	ypos = ypos/100 - 4;
	if(xpos <-4 || xpos > 4)
		if(ypos <-4 || ypos > 4)
			return;
	xpos = xpos - Cannon.x;
	ypos = ypos + Cannon.y;
	ypos *= -1;
	double angle = atan2(ypos, xpos);
	double distance = sqrt(xpos*xpos+ypos*ypos);
	distance = min(MAX_POWER, distance/2);
	Cannon.power = distance;
	angle = (angle*180)/M_PI;
	angle = max(min(90.0, angle), -90.0);
	Cannon.angle = angle;
}

void resetGame()
{
	Game.x = 3.6;
	Game.y = 0;
	Game.z = 3;
	Player1.lives = 5;
	Player1.score = 0;
	Player1.game_over = 0;
	bird_count = 0;
	createEnemies();
	createWood();
}

int main (int argc, char** argv)
{
	int width = 1400;
	int height = 800;

	GLFWwindow* window = initGLFW(width, height);

	initGL (window, width, height);
	resetGame();
	double last_update_time = glfwGetTime(), current_time;

	/* Draw in loop */
	while (!glfwWindowShouldClose(window)) {
		reshapeWindow (window, width, height);

		// OpenGL Draw commands
		draw();
		// Swap Frame Buffer in double buffering
		glfwSwapBuffers(window);

		// Poll for Keyboard and mouse events
		glfwPollEvents();

		gravity();
		glfwSetCursorPosCallback(window, cursor_position_callback);
		// The function signature for cursor position callbacks. More...
		// Control based on time (Time based transformation like 5 degrees rotation every 0.5s)
		current_time = glfwGetTime(); // Time in seconds
		if ((current_time - last_update_time) >= 0.5) { // atleast 0.5s elapsed since last frame
			// do something every 0.5 seconds ..
			last_update_time = current_time;
		}
	}

	glfwTerminate();
	exit(EXIT_SUCCESS);
}
