#include <GL/glew.h>
#include <GL/freeglut.h>

#include <string>
#include <vector>

#include "./src/sim/SETTINGS.h"
#include "SHADER.h"
#include "PPM.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

using namespace std;

// forward declarations
void runOnce();
void runEverytime();

// Text for the title bar of the window
string windowLabel("Crystal Renderer");

// the resolution of the OpenGL window
int xScreenRes = 800; 
int yScreenRes = 800;

// animate the current runEverytime()?
bool animate = true;

float rot = 3.0f;
GLuint renderer = 0;
GLuint boundVAO = 0, boundVBO = 0, boundEBO = 0;

struct CameraData {
    // world-space info
    vec3 eye     = {0.0f, 2.0f, -2.95f};
    vec3 origin  = {0.0f, 0.0f, 0.0f};
    vec3 up      = {0.0f, 1.0f, 0.0f};
    mat4 view    = mat4(1.0f);
    mat4 proj    = mat4(1.0f);

    // shader uniforms
    GLint model = -1;
    GLint viewU  = -1;
    GLint projU  = -1;
};
CameraData camera;

void rotateCamLeft() {
	float angle = -3.0f * M_PI / 180.0f;
	float x = camera.eye.x, z = camera.eye.z;

	camera.eye.x =  x * cos(angle) + z * sin(angle);
	camera.eye.z = -x * sin(angle) + z * cos(angle);
}
void rotateCamRight() {
	float angle = 3.0f  * M_PI / 180.0f;
	float x = camera.eye.x, z = camera.eye.z;

	camera.eye.x =  x * cos(angle) + z * sin(angle);
	camera.eye.z = -x * sin(angle) + z * cos(angle);
}
void moveCamUp()      { camera.eye.y += 0.25f; };
void moveCamDown()    { camera.eye.y -= 0.25f; }; 
void zoomCamIn()      { camera.eye += glm::normalize(camera.origin - camera.eye) * 0.1f; };
void zoomCamOut()     { camera.eye -= glm::normalize(camera.origin - camera.eye) * 0.1f; };
void updateCamera() { 
    camera.view = glm::lookAt(camera.eye, camera.origin, camera.up);
	glUseProgram(renderer);
	glUniformMatrix4fv(camera.viewU, 1, GL_FALSE, glm::value_ptr(camera.view));
	glUseProgram(0);
}

vector<float> values;

// current zoom level into the field
float zoom = 1.0;

///////////////////////////////////////////////////////////////////////
// GL and GLUT callbacks
///////////////////////////////////////////////////////////////////////
void glutDisplay()
{
  runEverytime();
}

///////////////////////////////////////////////////////////////////////
// Map the arrow keys to something here
///////////////////////////////////////////////////////////////////////
void glutSpecial(int key, int x, int y)
{
  switch (key) {
  case GLUT_KEY_LEFT:
    break;
  case GLUT_KEY_RIGHT:
    break;
  case GLUT_KEY_UP:
    break;
  case GLUT_KEY_DOWN:
    break;
  default:
    break;
  }
}

///////////////////////////////////////////////////////////////////////
// map the keyboard keys to something here
///////////////////////////////////////////////////////////////////////
void glutKeyboard(unsigned char key, int x, int y)
{
  switch (key) {
  case 'z':
    zoomCamIn();
    break;
  case 'x':
    zoomCamOut();
    break;
  case 'q':
    exit(0);
    break;
  default:
    break;
  }

  updateCamera();     // upload new view matrix
  glutPostRedisplay();        // request redraw
}

void onSpecial(int key, int, int)
{
  switch (key) {
    case GLUT_KEY_LEFT:   rotateCamLeft();  break;
    case GLUT_KEY_RIGHT:  rotateCamRight(); break;
    case GLUT_KEY_UP:     moveCamUp();      break;
    case GLUT_KEY_DOWN:   moveCamDown();    break;
  }
  updateCamera();     // upload new view matrix
  glutPostRedisplay();        // request redraw
}

///////////////////////////////////////////////////////////////////////
// animate and display new result
///////////////////////////////////////////////////////////////////////
void glutIdle()
{
  if (animate) {
    runEverytime();
  }
  glutPostRedisplay();
}

//////////////////////////////////////////////////////////////////////////////
// open the GLVU window
//////////////////////////////////////////////////////////////////////////////
int glvuWindow()
{
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_MULTISAMPLE);

  glutInitWindowSize(xScreenRes, yScreenRes);
  glutInitWindowPosition(10, 10);

  // request OpenGL 4.3 and core profile
  glutInitContextVersion(4, 3); 
  glutInitContextProfile(GLUT_CORE_PROFILE); 

  // create the context
  glutCreateWindow(windowLabel.c_str());

  // enable modern OpenGL extensions  
  glewExperimental = GL_TRUE; 
  if (glewInit() != GLEW_OK) {
      fprintf(stderr, "Failed to initialize GLEW\n");
      exit(EXIT_FAILURE);
  }

  // initialize everything
  runOnce();

  // set the viewport resolution (w x h)
  glViewport(0, 0, (GLsizei)xScreenRes, (GLsizei)yScreenRes);
  glClearColor(0.0, 0.0, 0.0, 0);

  // register all the callbacks
  glutDisplayFunc(&glutDisplay);
  glutIdleFunc(&glutIdle); 
  glutKeyboardFunc(&glutKeyboard);
  glutSpecialFunc(&onSpecial);  

  // enter the infinite GL loop
  glutMainLoop();

  // Control flow will never reach here
  return EXIT_SUCCESS;
}  

/////////////////////////////////////////////////////////////////////// 
/////////////////////////////////////////////////////////////////////// 
void printCommands()
{
  cout << "=============================================================== " << endl;
  cout << " Frost Simulation for CPSC 4900" << endl;
  cout << "=============================================================== " << endl;
  cout << " q           - quit" << endl;
  cout << " z           - zoom camera in" << endl;
  cout << " x           - zoom camera out" << endl;
  cout << " arrow keys  - move and rotate the camera" << endl;

}

void initRenderer() {
	renderer = createRenderProgram("./src/render/boundVert.glsl", 
										 "./src/render/boundFrag.glsl");

	camera.model = glGetUniformLocation(renderer, "uModel");
	camera.viewU = glGetUniformLocation(renderer, "uView");
	camera.projU = glGetUniformLocation(renderer, "uProj");

	glEnable(GL_DEPTH_TEST);

	const float aspect = (float)xScreenRes / (float)yScreenRes;
	mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 10.0f);

	// upload proj
	glUseProgram(renderer);
	glUniformMatrix4fv(camera.projU, 1, GL_FALSE, glm::value_ptr(proj));
	glUseProgram(0);

    float xBound = 1.0f, zBound = 1.0f;

	// create square bound data
	const GLfloat verts[] = {
		-xBound, 0.0f, -zBound,
		 xBound, 0.0f, -zBound,
		 xBound, 0.0f,  zBound,
		-xBound, 0.0f,  zBound
	};
	const GLuint square_idx[] = { 0,1, 1,2, 2,3, 3,0 };

	glGenVertexArrays(1, &boundVAO);
	glBindVertexArray(boundVAO);

	glGenBuffers(1, &boundVBO);
	glBindBuffer(GL_ARRAY_BUFFER, boundVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	glGenBuffers(1, &boundEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, boundEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(square_idx), square_idx, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	updateCamera();
}

void render() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glUseProgram(renderer);

	glUniformMatrix4fv(camera.model, 1, GL_FALSE, glm::value_ptr(mat4(1.0f)));

	glBindVertexArray(boundVAO);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glLineWidth(2.0f);
	glDrawElements(GL_LINES, 8, GL_UNSIGNED_INT, 0);

	glBindVertexArray(0);
	glUseProgram(0);
}

/////////////////////////////////////////////////////////////////////// 
/////////////////////////////////////////////////////////////////////// 
int main(int argc, char **argv)
{
  // initialize GLUT and GL
  glutInit(&argc, argv); 

  // open the GL window
  glvuWindow();
  return 0;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

void runOnce()
{
  // seed the RNG
  srand(time(NULL));

  printCommands();

  initRenderer();

  if (readPPM("./media/densityMap.ppm", 1024, 1024, values)) {
  }
}

void runEverytime()       
{          
  glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // always render
  render();

  glutSwapBuffers();
}


  