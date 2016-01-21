#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GRAV_CONST 0.009
#define MAX_POWER 2.5
#define VEL_THRE 0.02
#define ENEMY_NUMBER 5

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

class Character {
public:
  float x, y;
  float radius;
  float vel_x, vel_y;

  float fric_const;
  float elast_const;
  float rest_const;
  float air_const;

  bool alive;

  VAO *sprite;
} Enemies[ENEMY_NUMBER], Bird;

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

void MoveFixedColl(Wall& W, Character& C)
{
  glm::vec2 center(C.x, C.y);
  glm::vec2 aabb_half_extents(W.size_x, W.size_y);
  glm::vec2 aabb_center(W.x, W.y);
  glm::vec2 difference = center - aabb_center;
  glm::vec2 clamped = glm::clamp(difference, -aabb_half_extents, aabb_half_extents);
  glm::vec2 closest = aabb_center + clamped;
  difference = closest - center;

  if (glm::length(difference) < C.radius){
	C.vel_x *= C.fric_const;
	if (C.vel_y < 0)
	  C.vel_y = C.rest_const*abs(C.vel_y);
	if(abs(C.vel_y) < VEL_THRE) {
	  C.vel_y = 0;
	}
  }
  else {
	C.vel_x *= C.air_const;
	C.vel_y *= C.air_const;
  }
}

void MovMovColl(Character& A, Character& B)
{
  glm::vec2 VelA(A.vel_x, A.vel_y);
  glm::vec2 VelB(B.vel_x, B.vel_y);

  if(dot(VelA, VelB) < 0)
	return;

  glm::vec2 CharA(A.x, A.y);
  glm::vec2 CharB(B.x, B.y);
  glm::vec2 difference = CharB - CharA;

  if (glm::length(difference) <= (A.radius + B.radius)) {
	// int angle = acos(dot(glm::normalize(difference), glm::normalize(glm::vec2(1,0))));
	// angle = (angle/M_PI)*180;
	// if (Bird.y > Enemies[i].y)
	// 	angle *= -1;
	// int v_tan = Bird.vel_x*cos(angle) + Bird.vel_y*sin(angle);
	// int v_per = Bird.vel_y*cos(angle) + Bird.vel_x*sin(angle);
	// Bird.vel_x = v_tan*cos(angle) + v_per*sin(angle);
	// Bird.vel_y = v_tan*sin(angle) + v_per*cos(angle);
	float total_x = A.vel_x + B.vel_x;
	float total_y = A.vel_y + B.vel_y;
	if (A.y < B.y) {
	  total_y *= -1;
	  total_x *= -1;
	}
	B.vel_x = 0.5*total_x;
	B.vel_y = 0.5*total_y;
	A.vel_x = -0.5*total_x;
	A.vel_y = -0.5*total_y;
	// Enemies[i].alive = 0;
  }
}

void gravity() {

  Bird.vel_y -= GRAV_CONST;
  for (int i=0; i<ENEMY_NUMBER; i++)
	Enemies[i].vel_y -= GRAV_CONST;

  for (int i=0; i<ENEMY_NUMBER; i++)
	MovMovColl(Bird, Enemies[i]);

  for (int i=0; i<ENEMY_NUMBER; i++)
	for(int j=i+1; j<ENEMY_NUMBER; j++)
	  MovMovColl(Enemies[i], Enemies[j]);

  MoveFixedColl(Floor, Bird);
  for (int i=0; i<ENEMY_NUMBER; i++)
	MoveFixedColl(Floor, Enemies[i]);

  Bird.x += Bird.vel_x;
  Bird.y += Bird.vel_y;
  for (int i=0; i<ENEMY_NUMBER; i++) {
	Enemies[i].x += Enemies[i].vel_x;
	Enemies[i].y += Enemies[i].vel_y;
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

void fire_bird() {
  Bird.x = Cannon.x-0.5;// + 0.1*sin((Cannon.angle*M_PI)/180);
  Bird.y = Cannon.y;
  Bird.vel_x = Cannon.power * 0.2 * cos((Cannon.angle*M_PI)/180);
  Bird.vel_y = Cannon.power * 0.2 * sin((Cannon.angle*M_PI)/180);
}

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
	default:
	  break;
	}
  }
  else if (action == GLFW_PRESS) {
	switch (key) {
	case GLFW_KEY_ESCAPE:
	  quit(window);
	  break;
	case GLFW_KEY_SPACE:
	  Cannon.power = 0.1;
	  break;
	default:
	  break;
	}
  }
  else if (action == GLFW_REPEAT) {
	switch (key) {
	case GLFW_KEY_SPACE:
	  Cannon.power += 0.1;
	  Cannon.power = Cannon.power<MAX_POWER ? Cannon.power:MAX_POWER;
	  break;
	}
  }
}

/* Executed for character input (like in text boxes) */
void keyboardChar (GLFWwindow* window, unsigned int key)
{
  switch (key) {
  case 'Q':
  case 'q':
	quit(window);
	break;
  case 'e':
	Cannon.angle += 2;
	Cannon.angle = Cannon.angle>90? 90:Cannon.angle;
	break;
  case 'r':
	Cannon.angle -= 2;
	Cannon.angle = Cannon.angle<-90? -90:Cannon.angle;
	break;
  case 'i':
	Enemies[0].y += 1;
	break;
  case 'k':
	Enemies[0].y -= 1;
	break;
  case 'j':
	Enemies[0].x -= 1;
	break;
  case 'l':
	Enemies[0].x += 1;
	break;
  case 'w':
	Bird.y += 1;
	break;
  case 's':
	Bird.y -= 1;
	break;
  case 'a':
	Bird.x -= 1;
	break;
  case 'd':
	Bird.x += 1;
	break;
  default:
	break;
  }
}

/* Executed when a mouse button is pressed/released */
void mouseButton (GLFWwindow* window, int button, int action, int mods)
{
  switch (button) {
  case GLFW_MOUSE_BUTTON_LEFT:
	if (action == GLFW_RELEASE)
	  triangle_rot_dir *= -1;
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

  // Ortho projection for 2D views
  Matrices.projection = glm::ortho(-4.0f, 4.0f, -4.0f, 4.0f, 0.1f, 500.0f);
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
	Enemies[i].vel_x = 0;
	Enemies[i].vel_y = 0;
	Enemies[i].alive = 1;
	Enemies[i].fric_const = 0.96;
	Enemies[i].rest_const = 0.7;
	Enemies[i].air_const = 0.95;

	GLfloat vertex_buffer_data [3*365];
	GLfloat color_buffer_data [3*365];

	int k = 0;
	vertex_buffer_data[0] = 0;
	color_buffer_data[0] = 1;
	k++;
	vertex_buffer_data[1] = 0;
	color_buffer_data[1] = 1;
	k++;
	color_buffer_data[2] = 1;
	vertex_buffer_data[2] = 0;
	k++;

	for (int i=0; i<=360; ++i)
	  {
		vertex_buffer_data[k] = cos((i*M_PI)/180)*Bird.radius;
		color_buffer_data[k] = 0;
		k++;
		vertex_buffer_data[k] = sin((i*M_PI)/180)*Bird.radius;
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
  CWall.size_y = 1;

  // GL3 accepts only Triangles. Quads are not supported
  static const GLfloat vertex_buffer_data [] = {
	-0.25,-1,0, // vertex 1
	0.25, 1,0, // vertex 2
	0.25, -1,0, // vertex 3

	-0.25,-1,0, // vertex 3
	-0.25, 1,0, // vertex 4
	0.25, 1,0  // vertex 1
  };

  static const GLfloat color_buffer_data [] = {
	1,1,0, // color 1
	1,1,0, // color 2
	1,1,0, // color 3

	1,1,0, // color 3
	1,1,0, // color 4
	1,1,0  // color 1
  };

  // create3DObject creates and returns a handle to a VAO that can be used later
  CWall.sprite = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

void createFloor ()
{
  Floor.x = 0;
  Floor.y = -3.5;
  Floor.size_x = 4;
  Floor.size_y = 0.5;

  // GL3 accepts only Triangles. Quads are not supported
  static const GLfloat vertex_buffer_data [] = {
	-4,-4,0, // vertex 1
	4,-4,0, // vertex 2
	4, -3,0, // vertex 3

	4, -3,0, // vertex 3
	-4, -3,0, // vertex 4
	-4,-4,0  // vertex 1
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
  Floor.sprite = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

void createBird ()
{
  Bird.x = -2;
  Bird.y = 0;
  Bird.radius = 0.2;
  Bird.vel_x = 0;
  Bird.vel_y = 0;

  Bird.fric_const = 0.96;
  Bird.rest_const = 0.7;
  Bird.air_const = 0.99;

  static GLfloat vertex_buffer_data [3*365];
  static GLfloat color_buffer_data [3*365];

  int k = 0;
  vertex_buffer_data[0] = 0;
  color_buffer_data[0] = 1;
  k++;
  vertex_buffer_data[1] = 0;
  color_buffer_data[1] = 1;
  k++;
  color_buffer_data[2] = 1;
  vertex_buffer_data[2] = 0;
  k++;

  for (int i=0; i<=360; ++i)
	{
	  vertex_buffer_data[k] = cos((i*M_PI)/180)*Bird.radius;
	  color_buffer_data[k] = 1;
	  k++;
	  vertex_buffer_data[k] = sin((i*M_PI)/180)*Bird.radius;
	  color_buffer_data[k] = 0;
	  k++;
	  vertex_buffer_data[k] = 0;
	  color_buffer_data[k] = 0;
	  k++;
	}

  // create3DObject creates and returns a handle to a VAO that can be used later
  Bird.sprite = create3DObject(GL_TRIANGLE_FAN, 362, vertex_buffer_data, color_buffer_data, GL_FILL);
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
  PowerBar.x = 0;
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
  Matrices.view = glm::lookAt(glm::vec3(0,0,3), glm::vec3(0,0,0), glm::vec3(0,1,0)); // Fixed camera for 2D (ortho) in XY plane

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

  MVP = VP;
  glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
  draw3DObject(Floor.sprite);

  MVP = VP * glm::translate (glm::vec3(CWall.x, CWall.y, 0));
  glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
  draw3DObject(CWall.sprite);

  MVP = VP * glm::translate (glm::vec3(Cannon.x, Cannon.y, 0));
  glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
  // draw3DObject draws the VAO given to it using current MVP matrix
  draw3DObject(Cannon.base);

  MVP = VP * glm::translate (glm::vec3(Bird.x, Bird.y, 0));
  glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
  // draw3DObject draws the VAO given to it using current MVP matrix
  draw3DObject(Bird.sprite);

  Matrices.model = glm::mat4(1.0f);
  glm::mat4 translateBarrel = glm::translate (glm::vec3(Cannon.x, Cannon.y, 0));
  glm::mat4 rotateBarrel = glm::rotate((float)(Cannon.angle*M_PI/180.0f), glm::vec3(0,0,1));
  Matrices.model *= translateBarrel * rotateBarrel;
  MVP = VP * Matrices.model;
  glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
  // draw3DObject draws the VAO given to it using current MVP matrix
  draw3DObject(Cannon.barrel);

  MVP = VP * glm::translate (glm::vec3(PowerBar.x, PowerBar.y, 0)) * glm::scale(glm::vec3(Cannon.power, 1.0f, 1.0f));
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

  //####################################################################################################

  // Increment angles
  float increments = 1;

  //camera_rotation_angle++; // Simulating camera rotation
  triangle_rotation = triangle_rotation + increments*triangle_rot_dir*triangle_rot_status;
  rectangle_rotation = rectangle_rotation + increments*rectangle_rot_dir*rectangle_rot_status;
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
  createBird ();
  createCannon ();
  createTehPower ();
  createEnemies();

  // Create and compile our GLSL program from the shaders
  programID = LoadShaders( "Sample_GL.vert", "Sample_GL.frag" );
  // Get a handle for our "MVP" uniform
  Matrices.MatrixID = glGetUniformLocation(programID, "MVP");
	
  reshapeWindow (window, width, height);

  // Background color of the scene
  glClearColor (1.0f, 0.0f, 10.0f, 0.0f); // R, G, B, A
  glClearDepth (1.0f);

  glEnable (GL_DEPTH_TEST);
  glDepthFunc (GL_LEQUAL);

  cout << "VENDOR: " << glGetString(GL_VENDOR) << endl;
  cout << "RENDERER: " << glGetString(GL_RENDERER) << endl;
  cout << "VERSION: " << glGetString(GL_VERSION) << endl;
  cout << "GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
}

int main (int argc, char** argv)
{
  int width = 800;
  int height = 800;

  GLFWwindow* window = initGLFW(width, height);

  initGL (window, width, height);

  double last_update_time = glfwGetTime(), current_time;

  /* Draw in loop */
  while (!glfwWindowShouldClose(window)) {

	// OpenGL Draw commands
	draw();

	// Swap Frame Buffer in double buffering
	glfwSwapBuffers(window);

	// Poll for Keyboard and mouse events
	glfwPollEvents();

	gravity();

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
