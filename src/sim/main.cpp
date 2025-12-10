#include <iostream>
#include <ctime>

#include "FROST.h"
#include <thread>
#include <chrono>
#include <sstream>

using namespace std;

// forward declarations
void runOnce();
void runEverytime();

// simulator
Frost *simulator = nullptr;
bool showSites = false;
bool showEdges = true;
int showBoundary = false;

bool customParams = false;
float kappa[2][4];
float betaVal[2][4];
float rho;

// Text for the title bar of the window
string windowLabel("Frost Simulation");

// the resolution of the OpenGL window
int xScreenRes = 800; 
int yScreenRes = 800;

// animate the current runEverytime()?
bool animate = true;

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
  case ' ':
    animate = !animate;
    break;
  case 's':
    showSites = !showSites;
    break;
  case 'e':
    showEdges = !showEdges;
    break;
  case 'b':
    showBoundary = !showBoundary;
    break;
  case 'z':
    simulator->zoomCamIn();
    break;
  case 'x':
    simulator->zoomCamOut();
    break;
  case 'h':
    simulator->createHeightMap();
    simulator->createDensityMap();
    break;
  case 'q':
    exit(0);
    break;
  default:
    break;
  }

  simulator->updateCamera();     // upload new view matrix
  glutPostRedisplay();        // request redraw
}

void onSpecial(int key, int, int)
{
  switch (key) {
    case GLUT_KEY_LEFT:   simulator->rotateCamLeft();  break;
    case GLUT_KEY_RIGHT:  simulator->rotateCamRight(); break;
    case GLUT_KEY_UP:     simulator->moveCamUp();      break;
    case GLUT_KEY_DOWN:   simulator->moveCamDown();    break;
  }
  simulator->updateCamera();     // upload new view matrix
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
  cout << " s           - show/hide the lattice sites" << endl;
  cout << " e           - show/hide the hex edges" << endl;
  cout << " b           - show/hide the boundary cells" << endl;
  cout << " z           - zoom camera in" << endl;
  cout << " x           - zoom camera out" << endl;
  cout << " arrow keys  - move and rotate the camera" << endl;
  cout << " space       - pause the animation" << endl;
}

/////////////////////////////////////////////////////////////////////// 
/////////////////////////////////////////////////////////////////////// 
int main(int argc, char **argv)
{
  if (argc == 6) {
    customParams = true;

    rho = stof(argv[1]);

    auto parseRow = [](const std::string& s) {
        stringstream ss(s);
        vector<float> row(4);
        for (int i = 0; i < 4; ++i) ss >> row[i];
        return row;
    };

    auto kappaRow1 = parseRow(argv[2]);
    auto kappaRow2 = parseRow(argv[3]);
    auto betaRow1  = parseRow(argv[4]);
    auto betaRow2  = parseRow(argv[5]);

    for (int j = 0; j < 4; ++j) {
      kappa[0][j] = kappaRow1[j];
      kappa[1][j] = kappaRow2[j];
      betaVal[0][j] = betaRow1[j];
      betaVal[1][j] = betaRow2[j];
    }

  }

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

  int gridRadius = 100;

  // construct simulator
  simulator = new Frost(gridRadius);
  if (customParams) simulator->setParams(rho, kappa, betaVal);
  
  simulator->setResolution(xScreenRes, yScreenRes);
  simulator->init(); 
}

void runEverytime()       
{          
  glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  static int iters = 0;
  if (animate) {
    
    for (int i = 0; i < 20; i++) {
      simulator->gpuTimestep();
    } 
    iters++;
  }

  if (iters == 5) {
    // simulator->createHeightMap();
    simulator->createDensityMap();
    exit(0);
  }

  
  
  // always render
  simulator->render();

  // this_thread::sleep_for(chrono::milliseconds(50));
  
  glutSwapBuffers();
}


  