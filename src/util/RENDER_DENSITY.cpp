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

GLuint volumeTex = 0;
GLuint heightTex = 0;
GLuint cubeVAO = 0, cubeVBO = 0, cubeEBO = 0;
GLuint volProgram = 0;

int volW = 0, volH = 0, volD = 64;   // D = # of slices extruded from 2D
// tunables
float uSigmaT = 8.0f, uSigmaS = 5.0f, uExposure = 1.5f;
vec3  uSigmaA = vec3(0.12f, 0.06f, 0.03f); // more red absorption -> bluish ice
float uStepMul = 300.0f;  // samples per unit distance through the box


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


static GLuint makeHeight2D(const std::vector<float>& vals, int W, int H) {
    glGenTextures(1, &heightTex);
    glBindTexture(GL_TEXTURE_2D, heightTex);

    // pack to uint8 (0..255), since your PPM reader gave 0..255 floats
    std::vector<uint8_t> r(W*H);
    for (size_t i = 0; i < r.size(); ++i) r[i] = (uint8_t)glm::clamp(vals[i], 0.0f, 255.0f);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, W, H, 0, GL_RED, GL_UNSIGNED_BYTE, r.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);   // or GL_NEAREST for pixel-crisp
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);   // ^
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
    return heightTex;
}


static void makeCube()
{
  const float V[] = {
    -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
    -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f
  };
  const unsigned I[] = {
    0,1,2, 2,3,0,  4,5,6, 6,7,4,
    0,4,7, 7,3,0,  1,5,6, 6,2,1,
    3,2,6, 6,7,3,  0,1,5, 5,4,0
  };
  glGenVertexArrays(1, &cubeVAO);
  glGenBuffers(1, &cubeVBO);
  glGenBuffers(1, &cubeEBO);
  glBindVertexArray(cubeVAO);
  glBindBuffer(GL_ARRAY_BUFFER, cubeVBO); glBufferData(GL_ARRAY_BUFFER, sizeof(V), V, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO); glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(I), I, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
  glBindVertexArray(0);
}


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


void render()
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Keep view fresh (camera may have moved with keys)
  camera.view = glm::lookAt(camera.eye, camera.origin, camera.up);

  // Identity model: cube centered at origin spanning [-0.5,0.5]^3
  const glm::mat4 model(1.0f);

  glUseProgram(volProgram);

  // Upload MVP
  glUniformMatrix4fv(camera.model, 1, GL_FALSE, glm::value_ptr(model));
  glUniformMatrix4fv(camera.viewU, 1, GL_FALSE, glm::value_ptr(camera.view));
  glUniformMatrix4fv(camera.projU, 1, GL_FALSE, glm::value_ptr(camera.proj));

  // Camera position in object space (model is identity → same as world)
  const GLint uCamObjLoc = glGetUniformLocation(volProgram, "uCamPosObj");
  if (uCamObjLoc >= 0) glUniform3fv(uCamObjLoc, 1, glm::value_ptr(camera.eye));

  // Ensure the height texture is bound to unit 0
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, heightTex);

  // Draw the proxy
  glBindVertexArray(cubeVAO);
  glDisable(GL_CULL_FACE); // see both front/back surfaces for rays exiting/entering
  glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
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
  srand(time(NULL));
  printCommands();

  // ray-march program
  volProgram = createRenderProgram("./src/render/marchVert.glsl",
                                   "./src/render/marchFrag.glsl");

  camera.model = glGetUniformLocation(volProgram, "uModel");
  camera.viewU = glGetUniformLocation(volProgram, "uView");
  camera.projU = glGetUniformLocation(volProgram, "uProj");

  glEnable(GL_DEPTH_TEST);

  // geometry: unit cube as proxy volume
  makeCube();

  const float aspect = float(xScreenRes) / float(yScreenRes);
  camera.proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 10.0f);
  camera.view = glm::lookAt(camera.eye, camera.origin, camera.up);

  // ppload proj once 
  glUseProgram(volProgram);
  glUniformMatrix4fv(camera.projU, 1, GL_FALSE, glm::value_ptr(camera.proj));
  glUseProgram(0);

  // load PPM  
  int W = 1024, H = 1024; 
  readPPM("./media/densityMap.ppm", W, H, values);
  heightTex = makeHeight2D(values, W, H);

  // bind static uniforms + texture unit
  glUseProgram(volProgram);

  // sampler binding
  const GLint uHeightLoc = glGetUniformLocation(volProgram, "uHeight");
  glUniform1i(uHeightLoc, 0);  // texture unit 0

  // raymarch/appearance tunables
  const GLint uStepLoc  = glGetUniformLocation(volProgram, "uStepMul");
  const GLint uExpoLoc  = glGetUniformLocation(volProgram, "uExposure");
  glUniform1f(uStepLoc,  uStepMul);
  glUniform1f(uExpoLoc,  uExposure);

  // lighting/scatter params 
  const GLint uSigmaTLoc = glGetUniformLocation(volProgram, "uSigmaT");
  const GLint uSigmaSLoc = glGetUniformLocation(volProgram, "uSigmaS");
  const GLint uSigmaALoc = glGetUniformLocation(volProgram, "uSigmaA");
  const GLint uLdirObj   = glGetUniformLocation(volProgram, "uLightDirObj");
  if (uSigmaTLoc >= 0) glUniform1f(uSigmaTLoc, uSigmaT);
  if (uSigmaSLoc >= 0) glUniform1f(uSigmaSLoc, uSigmaS);
  if (uSigmaALoc >= 0) glUniform3fv(uSigmaALoc, 1, glm::value_ptr(uSigmaA));
  if (uLdirObj  >= 0) glUniform3f(uLdirObj, -0.4f, 0.8f, 0.4f);

  glUseProgram(0);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, heightTex);
}

void runEverytime()       
{          
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // always render
  render();

  glutSwapBuffers();
}


  