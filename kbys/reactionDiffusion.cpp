#include "FIELD_2D.h"
#include <cmath>
#include <iostream>

#if _WIN32
#include <gl/glut.h>
#elif __APPLE__
#include <GLUT/glut.h>
#elif __linux__
#include <GL/glut.h>
#endif

using namespace std;

// helper function forward declarations:
void  zeroField        (FIELD_2D &temp);
void  KBYS_updateT     (FIELD_2D &a, FIELD_2D &b, FIELD_2D &update, FIELD_2D &oldH, FIELD_2D &newH);
void  KBYS_updateP     (FIELD_2D &a, FIELD_2D &b, FIELD_2D &update);
float space_derivative (const FIELD_2D &f, int dim, int x, int y, float delta);
float find_theta       (FIELD_2D &f, int x, int y, float delta);
void  fill_KBYS_field  (FIELD_2D &f, FIELD_2D &pField, int dim, float delta);
void  create_toroid    (FIELD_2D &f);
float find_dpdt        (FIELD_2D &p, FIELD_2D &T, FIELD_2D &dpdy, FIELD_2D &dpdx,
                        int x, int y, float delta);
void normalize         (FIELD_2D &f);

// resolution of the field
int xRes = 200;
int yRes = 200;

// the field being drawn and manipulated
FIELD_2D field(xRes, yRes);
FIELD_2D aField(xRes, yRes), bField(xRes, yRes);
FIELD_2D newA(xRes, yRes), newB(xRes, yRes);
FIELD_2D KBYS_dpdy(xRes, yRes), KBYS_dpdx(xRes, yRes), oldH(xRes,yRes), newH(xRes,yRes);

float *aData, *bData, *newAData, *newBData;

// the resolution of the OpenGL window -- independent of the field resolution
int xScreenRes = 850;
int yScreenRes = 850;

// Text for the title bar of the window
string windowLabel("Phase Field Model");

// mouse tracking variables
int xMouse = -1;
int yMouse = -1;
int mouseButton = -1;
int mouseState = -1;
int mouseModifiers = -1;

// current grid cell the mouse is pointing at
int xField = -1;
int yField = -1;

// animate the current runEverytime()?
bool animate = true;

// draw the grid over the field?
bool drawingGrid = false;

// print out what the mouse is pointing at?
bool drawingValues = true;

// currently capturing frames for a movie?
bool captureMovie = false;

// the current viewer eye position
float eyeCenter[] = {0.5, 0.5, 1};

// current zoom level into the field
float zoom = 1.0;

// settings
float d_a, d_b, length, time_step;
int euler_steps;

// constants
float alpha = 0.75;
float tau = 0.0003, K = 1.2, eps_bar = 0.01, dee = 0.04;
float j = 6, kbys_gamma = 10.0; 

// forward declare the caching function
void runOnce();

// forward declare the timestepping function
void runEverytime();

///////////////////////////////////////////////////////////////////////
// Figure out which field element is being pointed at, set xField and
// yField to them 
///////////////////////////////////////////////////////////////////////
void refreshMouseFieldIndex(int x, int y)
{
  // make the lower left the origin
  y = yScreenRes - y;

  float xNorm = (float)x / xScreenRes;
  float yNorm = (float)y / yScreenRes;

  float halfZoom = 0.5 * zoom;
  float xWorldMin = eyeCenter[0] - halfZoom;
  float xWorldMax = eyeCenter[0] + halfZoom;

  // get the bounds of the field in screen coordinates
  //
  // if non-square textures are ever supported, change the 0.0 and 1.0 below
  float xMin = (0.0 - xWorldMin) / (xWorldMax - xWorldMin);
  float xMax = (1.0 - xWorldMin) / (xWorldMax - xWorldMin);

  float yWorldMin = eyeCenter[1] - halfZoom;
  float yWorldMax = eyeCenter[1] + halfZoom;

  float yMin = (0.0 - yWorldMin) / (yWorldMax - yWorldMin);
  float yMax = (1.0 - yWorldMin) / (yWorldMax - yWorldMin);

  float xScale = 1.0;
  float yScale = 1.0;

  if (xRes < yRes)
    xScale = (float)yRes / xRes;
  if (xRes > yRes)
    yScale = (float)xRes / yRes;

  // index into the field after normalizing according to screen
  // coordinates
  xField = xScale * xRes * ((xNorm - xMin) / (xMax - xMin));
  yField = yScale * yRes * ((yNorm - yMin) / (yMax - yMin));

  // clamp to something inside the field
  xField = (xField < 0) ? 0 : xField;
  xField = (xField >= xRes) ? xRes - 1 : xField;
  yField = (yField < 0) ? 0 : yField;
  yField = (yField >= yRes) ? yRes - 1 : yField;
}

///////////////////////////////////////////////////////////////////////
// Print a string to the GL window
///////////////////////////////////////////////////////////////////////
void printGlString(string output)
{
  glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
  for (unsigned int x = 0; x < output.size(); x++)
    glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, output[x]);
}

///////////////////////////////////////////////////////////////////////
// dump the field contents to a GL texture for drawing
///////////////////////////////////////////////////////////////////////
void updateTexture(FIELD_2D &texture)
{
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, 3, texture.xRes(), texture.yRes(), 0, GL_LUMINANCE, GL_FLOAT, texture.data());

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
  glEnable(GL_TEXTURE_2D);
}

///////////////////////////////////////////////////////////////////////
// draw a grid over everything
///////////////////////////////////////////////////////////////////////
void drawGrid()
{
  glColor4f(0.1, 0.1, 0.1, 1.0);

  float dx = 1.0 / xRes;
  float dy = 1.0 / yRes;

  if (xRes < yRes)
    dx *= (float)xRes / yRes;
  if (xRes > yRes)
    dy *= (float)yRes / xRes;

  glBegin(GL_LINES);
  for (int x = 0; x < field.xRes() + 1; x++) {
    glVertex3f(x * dx, 0, 1);
    glVertex3f(x * dx, 1, 1);
  }
  for (int y = 0; y < field.yRes() + 1; y++) {
    glVertex3f(0, y * dy, 1);
    glVertex3f(1, y * dy, 1);
  }
  glEnd();
}

///////////////////////////////////////////////////////////////////////
// GL and GLUT callbacks
///////////////////////////////////////////////////////////////////////
void glutDisplay()
{
  // Make ensuing transforms affect the projection matrix
  glMatrixMode(GL_PROJECTION);

  // set the projection matrix to an orthographic view
  glLoadIdentity();
  float halfZoom = zoom * 0.5;

  glOrtho(-halfZoom, halfZoom, -halfZoom, halfZoom, -10, 10);

  // set the matrix mode back to modelview
  glMatrixMode(GL_MODELVIEW);

  // set the lookat transform
  glLoadIdentity();
  gluLookAt(eyeCenter[0], eyeCenter[1], 1, // eye
            eyeCenter[0], eyeCenter[1], 0, // center
            0, 1, 0);                      // up

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  float xLength = 1.0;
  float yLength = 1.0;

  if (xRes < yRes)
    xLength = (float)xRes / yRes;
  if (yRes < xRes)
    yLength = (float)yRes / xRes;

  glEnable(GL_TEXTURE_2D);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0, 0.0);
  glVertex3f(0.0, 0.0, 0.0);
  glTexCoord2f(0.0, 1.0);
  glVertex3f(0.0, yLength, 0.0);
  glTexCoord2f(1.0, 1.0);
  glVertex3f(xLength, yLength, 0.0);
  glTexCoord2f(1.0, 0.0);
  glVertex3f(xLength, 0.0, 0.0);
  glEnd();
  glDisable(GL_TEXTURE_2D);

  // draw the grid, but only if the user wants
  if (drawingGrid)
    drawGrid();

  // if there's a valid field index, print it
  if (xField >= 0 && yField >= 0 && xField < field.xRes() && yField < field.yRes()) {
    glLoadIdentity();

    // must set color before setting raster position, otherwise it won't take
    glColor4f(1.0f, 0.0f, 0.0f, 1.0f);

    // normalized screen coordinates (-0.5, 0.5), due to the glLoadIdentity
    float halfZoom = 0.5 * zoom;
    glRasterPos3f(-halfZoom * 0.95, -halfZoom * 0.95, 0);

    // build the field value string
    char buffer[256];
    string fieldValue("(");
    sprintf(buffer, "%i", xField);
    fieldValue = fieldValue + string(buffer);
    sprintf(buffer, "%i", yField);
    fieldValue = fieldValue + string(", ") + string(buffer) + string(") = ");
    sprintf(buffer, "%f", field(xField, yField));
    fieldValue = fieldValue + string(buffer);

    // draw the grid, but only if the user wants
    if (drawingValues)
      printGlString(fieldValue);
  }

  glutSwapBuffers();
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
void printCommands()
{
  cout << "=============================================================== " << endl;
  cout << " Field viewer code CPSC 679" << endl;
  cout << "=============================================================== " << endl;
  cout << " q           - quit" << endl;
  cout << " v           - type the value of the cell under the mouse" << endl;
  cout << " g           - throw a grid over everything" << endl;
  cout << " w           - write out a PPM file " << endl;
  cout << " left mouse  - pan around" << endl;
  cout << " right mouse - zoom in and out " << endl;
  cout << " shift left mouse - draw on the grid " << endl;
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
// Map the keyboard keys to something here
///////////////////////////////////////////////////////////////////////
void glutKeyboard(unsigned char key, int x, int y)
{
  switch (key) {
  case 'a':
    animate = !animate;
    break;
  case 'g':
    drawingGrid = !drawingGrid;
    break;
  case '?':
    printCommands();
    break;
  case 'v':
    drawingValues = !drawingValues;
    break;
  case 'w': {
    static int count = 0;
    char buffer[256];
    sprintf(buffer, "output_%i.ppm", count);
    field.writePPM(buffer);
    count++;
  } break;
  case 'q':
    exit(0);
    break;
  default:
    break;
  }
}

///////////////////////////////////////////////////////////////////////
// Do something if the mouse is clicked
///////////////////////////////////////////////////////////////////////
void glutMouseClick(int button, int state, int x, int y)
{
  int modifiers = glutGetModifiers();
  mouseButton = button;
  mouseState = state;
  mouseModifiers = modifiers;

  if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && modifiers & GLUT_ACTIVE_SHIFT) {
    // figure out which cell we're pointing at
    refreshMouseFieldIndex(x, y);

    // set the cell
    for (int y = yField - 4; y < yField + 5; y++) {
      for (int x = xField - 4; x < xField + 5; x++) {
        bField(x, y) = 0;
      }
    }

    // make sure nothing else is called
    return;
  }

  xMouse = x;
  yMouse = y;
}

///////////////////////////////////////////////////////////////////////
// Do something if the mouse is clicked and moving
///////////////////////////////////////////////////////////////////////
void glutMouseMotion(int x, int y)
{
  if (mouseButton == GLUT_LEFT_BUTTON && mouseState == GLUT_DOWN && mouseModifiers & GLUT_ACTIVE_SHIFT) {
    // figure out which cell we're pointing at
    refreshMouseFieldIndex(x, y);

    // set the cell
    for (int y = yField - 4; y < yField + 5; y++) {
      for (int x = xField - 4; x < xField + 5; x++) {
        bField(x, y) = 0;
      }
    }

    // make sure nothing else is called
    return;
  }

  float xDiff = x - xMouse;
  float yDiff = y - yMouse;
  float speed = 0.001;

  if (mouseButton == GLUT_LEFT_BUTTON) {
    eyeCenter[0] -= xDiff * speed;
    eyeCenter[1] += yDiff * speed;
  }
  if (mouseButton == GLUT_RIGHT_BUTTON)
    zoom -= yDiff * speed;

  xMouse = x;
  yMouse = y;
}

///////////////////////////////////////////////////////////////////////
// Do something if the mouse is not clicked and moving
///////////////////////////////////////////////////////////////////////
void glutPassiveMouseMotion(int x, int y)
{
  refreshMouseFieldIndex(x, y);
}

///////////////////////////////////////////////////////////////////////
// animate and display new result
///////////////////////////////////////////////////////////////////////
void glutIdle()
{
  if (animate) {
    runEverytime();
  }
  updateTexture(field);
  glutPostRedisplay();
}

//////////////////////////////////////////////////////////////////////////////
// open the GLVU window
//////////////////////////////////////////////////////////////////////////////
int glvuWindow()
{
  glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
  glutInitWindowSize(xScreenRes, yScreenRes);
  glutInitWindowPosition(10, 10);
  glutCreateWindow(windowLabel.c_str());

  // set the viewport resolution (w x h)
  glViewport(0, 0, (GLsizei)xScreenRes, (GLsizei)yScreenRes);

  // set the background color to gray
  glClearColor(0.1, 0.1, 0.1, 0);

  // register all the callbacks
  glutDisplayFunc(&glutDisplay);
  glutIdleFunc(&glutIdle);
  glutKeyboardFunc(&glutKeyboard);
  glutSpecialFunc(&glutSpecial);
  glutMouseFunc(&glutMouseClick);
  glutMotionFunc(&glutMouseMotion);
  glutPassiveMotionFunc(&glutPassiveMouseMotion);

  // enter the infinite GL loop
  glutMainLoop();

  // Control flow will never reach here
  return EXIT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
  // In case the field is rectangular, make sure to center the eye
  if (xRes < yRes) {
    float xLength = (float)xRes / yRes;
    eyeCenter[0] = xLength * 0.5;
  }
  if (yRes < xRes) {
    float yLength = (float)yRes / xRes;
    eyeCenter[1] = yLength * 0.5;
  }

  aField.clear();
  bField.clear();
  newA.clear();
  newB.clear();
  KBYS_dpdx.clear();
  KBYS_dpdy.clear();
  newH.clear();
  oldH.clear();
  
  printCommands();

  cout << "Simulating Kobayashi reaction-diffusion " << endl;
  time_step = 0.0002;
  alpha = 0.9;
  length = 10.0;

  runOnce();

  // initialize GLUT and GL
  glutInit(&argc, argv);

  // open the GL window
  glvuWindow();
  return 1;
}

///////////////////////////////////////////////////////////////////////
// This function is called every frame -- do something interesting
// here.
///////////////////////////////////////////////////////////////////////
void runEverytime()     
{
  static int counter = 0;

  create_toroid(aField);
  create_toroid(bField);
  
  float delta = length / xRes;

  fill_KBYS_field(KBYS_dpdy, bField, 1, delta);
  fill_KBYS_field(KBYS_dpdx, bField, 0, delta);

  // aField = Temp, bField = phase
  KBYS_updateP(aField, bField, newB);
  KBYS_updateT(aField, bField, newA, oldH, newH);

  // Draw the new field
  oldH.normalize();
  field = oldH;

  // iterate
  oldH = newH;
  aField = newA;
  bField = newB;

  counter++;
}

///////////////////////////////////////////////////////////////////////
// This is called once at the beginning so you can precache
// something here
///////////////////////////////////////////////////////////////////////
void runOnce()
{
  aField(xRes/2,yRes/2) = 1;
  bField(xRes/2,yRes/2) = 1;
}

// Kobayashi model
void KBYS_updateT(FIELD_2D &T, FIELD_2D &p, FIELD_2D &update, FIELD_2D &h_old, FIELD_2D &h_new) {
  // b is phase
  float delta = length / xRes; 
  float dpdt, laplace, dTdt;

  for (int y = 1; y < yRes-1; y++) {
    for (int x = 1; x < xRes-1; x++) {
      dpdt = find_dpdt(p, T, KBYS_dpdy, KBYS_dpdx, x, y, delta);
      laplace = (T(x+1,y) + T(x,y+1) - 4*T(x,y) + T(x-1,y) + T(x,y-1)) / (delta*delta);
      dTdt = laplace + K * dpdt;

      update(x,y) = T(x,y) + time_step * dTdt;
      h_new(x,y) = h_old(x,y) + time_step * abs(dTdt);
    }
  }
}

void KBYS_updateP(FIELD_2D &T, FIELD_2D &p, FIELD_2D &update) {
  // b is phase
  float delta = length / xRes; 
  float dpdt;

  for (int y = 1; y < yRes-1; y++) {
    for (int x = 1; x < xRes-1; x++) {
      dpdt = find_dpdt(p, T, KBYS_dpdy, KBYS_dpdx, x, y, delta);

      update(x,y) = p(x,y) + time_step * dpdt;
    }
  }
}

float find_dpdt(FIELD_2D &p, FIELD_2D &T, FIELD_2D &dpdy, FIELD_2D &dpdx,
               int x, int y, float delta) {
  float theta = find_theta(p, x, y, delta);
  float KBYS_eps = eps_bar * (1 + dee*cos(j * theta));
  float m = (alpha / M_PI) * atan(kbys_gamma * (1 - T(x,y)));

  float d_dx = space_derivative(dpdy, 0, x, y, delta);
  float d_dy = space_derivative(dpdx, 1, x, y, delta);   
  float m_term = p(x,y) * (1-p(x,y)) * (p(x,y) - 0.5 + m);
  
  float laplace = (p(x+1,y) + p(x,y+1) - 4*p(x,y) + p(x-1,y) + p(x,y-1)) / (delta*delta);

  return (KBYS_eps*KBYS_eps*laplace - d_dx + d_dy + m_term) / tau;
}

float space_derivative(const FIELD_2D &f, int dim, int x, int y, float delta) {
  // dim = 0 -> wrt x
  if (!dim) {
    return (f(x+1,y) - f(x-1,y))/(2*delta);
  }
  // dim = 1 -> wrt y
  else {
    return (f(x,y+1) - f(x,y-1))/(2*delta);
  }
}

// from Kim 2003
float find_theta(FIELD_2D &f, int x, int y, float delta) {
  float dp_dx = space_derivative(f, 0, x, y, delta);
  float dp_dy = space_derivative(f, 1, x, y, delta);

  float theta = -1.0 * (atan2(dp_dx, dp_dy) + M_PI/2);

  return theta;
}

void fill_KBYS_field(FIELD_2D &f, FIELD_2D &pField, int dim, float delta) {
  float dp, theta, KBYS_eps, de_dtheta, result;

  for (int y = 1; y < yRes-1; y++) {
    for (int x = 1; x < xRes-1; x++) {
      dp = space_derivative(pField, dim, x, y, delta);

      theta = find_theta(pField, x, y, delta); // in radians
      KBYS_eps = eps_bar * (1 + dee * cos(j * theta));
      de_dtheta = -eps_bar * dee * j * sin(j * theta);

      result = KBYS_eps * de_dtheta * dp;

      f(x,y) = result;
    }
  }

  create_toroid(f);
}

void create_toroid(FIELD_2D &f) {
  // left and right border
  for (int i = 1; i < yRes - 2; i++) {
    f(0,i) = f(xRes-2, i);
    f(xRes-1,i) = f(1, i);
  }
  // bottom and top border
  for (int i = 1; i < xRes - 2; i++) {
    f(i,0) = f(i, yRes-2);
    f(i,yRes-1) = f(i, 1);
  }
}