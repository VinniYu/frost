#ifndef FROST_H
#define FROST_H

#include "SETTINGS.h"
#include "./src/util/SHADER.h"


extern bool showSites;
extern bool showEdges;
extern int showBoundary;

using namespace std;

struct CamUniforms { GLint model=-1, view=-1, proj=-1; };
#define N_NBR 8

class Frost {
public:

	Frost(int radius);
	~Frost();

	// rendering init
	void setResolution(int x, int y) { _xScreenRes = x, _yScreenRes = y; };
	void initBoundRenderer();
	void initSiteRenderer();
	void initHexRenderer();

	// geometry init
	void initHexMesh();
	void initInstances();

	void initBorderSites(); // precompute all border site indices
	void initStates();      // initialize the state vectors

	void init();
	
	// 2d lookup table
	inline bool validQR(int q, int r) const;
	inline int lookupIndex(int q, int r) const;
	inline int indexFromQR(int q, int r) const;
	void initLookupTable();

	// 2.5d lookup table
	inline bool validQRC(int q, int r, int c) const;
	int indexFromQRC(int q, int r, int c) const;
	int index3D(int layer, int idx2d) const { return layer * _nSites + idx2d; }

	// camera movement
	void rotateCamLeft();
	void rotateCamRight();
	void moveCamUp()   { _camera.y += _vert; };
	void moveCamDown() { _camera.y -= _vert; }; 
	void zoomCamIn()   { _camera += glm::normalize(_origin - _camera) * 0.1f; };
	void zoomCamOut()  { _camera -= glm::normalize(_origin - _camera) * 0.1f;};
	void updateCamera();

	// rendering functions
	void renderBounds();
	void renderSites();
	void renderHexes();
	void renderHexEdges();
	void render();

	// time stepping functions
	void seed();     // seeds the initial crystals
	void timestep(); // main timestep function

	inline int neighborAt(int i3D, int j) const { return _neighborLUT[i3D * N_NBR + j]; };
	void initNeighborLUT();
	int numAttachedNeighbors(int i3D); // returns the number of neighbors with LUT
	void findBoundarySites(); 		   // sweeps over grid and finds boundary sites 

	// computational
	void diffuse();
	void freeze();
	void attach();
	void melt();

	// GPU functions
	void initGPU();
	void uploadNeighborTable();
	void uploadMasksAndMass();
	void uploadBoundary();
	void createPrograms();
	void setKappa(GLuint prog);
	void setBeta(GLuint prog);
	void setMu(GLuint prog);

	void gpuDiffuse();
	void gpuFindBoundary();
	void gpuFreeze();
	void gpuAttach();
	void gpuMelt();
	void gpuTimestep();

	// analysis 
	void createHeightMap();
	void createDensityMap();

private:

	// openGL screen
	int _xScreenRes;
	int _yScreenRes;

	///////////////////////////////////////////////////////////////////////
	// camera variables
	///////////////////////////////////////////////////////////////////////

	// looking at xz plane, z is up
	vec3 _camera = {0.0, 2.0, -2.95};
	// vec3 _camera = {0.0, 2.0, -0.1}; // straight above
	vec3 _origin = {0.0, 0.0, 0.0}; 
	vec3 _up     = {0.0, 1.0, 0.0};
	mat4 _view{1.0f};
	mat4 _proj{1.0f};

	// movement parameters
	REAL _rot = 3.0f, _vert = 0.25f; 
	float _fov = 60.0f, _near = 0.1f, _far = 10.0f;

	///////////////////////////////////////////////////////////////////////
	// rendering variables
	///////////////////////////////////////////////////////////////////////
	
	// bound renderer
	CamUniforms _boundU{};
	GLuint _boundRenderer = 0;
	GLuint _boundVAO = 0, _boundVBO = 0, _boundEBO = 0;
	GLsizei _boundIndexCount = 0;
	GLfloat _xBound = 2.0f, _zBound = 2.0f; 

	// site renderer
	CamUniforms _siteU{};
	GLuint _siteRenderer = 0;
	GLuint _siteVAO = 0, _siteVBO = 0;
	GLuint _siteQRCVBO = 0; /// to index with (q,r,c) coords

	// hexagon renderer
	CamUniforms _hexU{}, _edgeU{};
	GLuint _hexRenderer = 0, _hexEdgeRenderer = 0;
	GLuint _hexVAO = 0, _hexVBO = 0, _hexEBO = 0, _hexEdgeEBO = 0;
	GLuint _instVBO = 0;
	GLuint _attachVBO = 0, _boundaryVBO = 0;
	int _hexIndexCount = 0, _hexEdgeIndexCount = 0;

	// for PPM
	int _ppmW = 1024;
	int _ppmH = 1024;

	///////////////////////////////////////////////////////////////////////
	// simulation variables
	///////////////////////////////////////////////////////////////////////

	// 2D parameters
	int  _nSites;	   // total hex sites
	int  _gridRadius;  // max number of sites from origin to sim-bounds
	REAL _simLength;   // world space size of grid
	REAL _hexRadius, _hexWidth, _hexHeight, _hexThickness;

	// 2.5D parameters
	int _nBasal = 400; // number of 2.5D layers. +/-c dimension
	int _nSitesFull;  // number of sites in 2.5
	
	vector<int> _neighborLUT;

	// CPU states
	vector<GLuint>  _attached; // flags each site if attached to main crystal
	vector<GLuint>  _boundary; // flags a site if it is part of the boundary
	vector<GLuint> _borderSites;
	vector<REAL> _diffuseMass, _boundaryMass;        // masses of each site 
	vector<GLuint>  _newAttachmentsIdx, _boundaryIdx; // for easy lookups

	// precomputed coords table to for quick switching (axial <-> 1d) 
	vector<AxialCoord> _axialCoords;   // list of axial coords indexed by i
	vector<GLuint>      _axialIndexLUT; // lookup table from (q,r) to index

	// GPU states
	GLuint _ssboNeighbors = 0;    // binding = 0
	GLuint _ssboNbrCount = 0;     // binding = 1
	GLuint _ssboMassA = 0;        // binding = 2
	GLuint _ssboMassB = 0;        // binding = 3
	GLuint _ssboAttached = 0;     // binding = 4
	GLuint _ssboBorder = 0;       // binding = 5
	GLuint _ssboBoundary = 0;     // binding = 6
	GLuint _ssboBoundaryMass = 0; // binding = 7

	// compute shaders
	GLuint _progDiffusePlanar = 0;
	GLuint _progDiffuseBasal = 0;
	GLuint _progFindBoundary = 0;
	GLuint _progFreeze = 0;
	GLuint _progAttach = 0;
	GLuint _progMelt = 0;

	
// physical parameters
REAL _rho = 0.10f;
float _kappa[2][4] = {
	/* nZ=0 */ {0.10f, 0.1f, 0.1f, 0.1f},
	/* nZ=1 */ {0.10f, 0.1f, 0.1f, 0.1f}
};
float _beta[2][4] = {
	/* nZ=0 */ {5.10f, 1.00f, 2.00f, 2.00f},
	/* nZ=1 */ {5.10f, 1.00f, 2.00f, 2.00f}
};
float _mu[2][4] = {
	/* nZ=0 */ {0.00f, 0.01f, 0.01f, 0.01f},
	/* nZ=1 */ {0.01f, 0.01f, 0.01f, 0.01f}
};













};

#endif


// 

/*
	1. filter to extract 2d texture,
		- throw square on top, sample
		- structured parameter tuning

 	2. figure out parameters prod feathering -> interp ---> generalize to nonhex grids
		

*/