#include "FROST.h"

Frost::Frost(int radius) {
	_gridRadius = radius;
	_simLength  = _xBound * 2;

	// init site information
	int centerRowLength = 2*_gridRadius + 1;  // number of hexes in middle row
	_hexWidth = _simLength / centerRowLength; // width in x
	_hexRadius = _hexWidth / sqrt(3);         
	_hexHeight = 2 * _hexRadius;              // height in z
	_hexThickness = _hexRadius * 0.9f;        // thickness in y
	
	_nSites = 1 + 3*_gridRadius*(_gridRadius+1); 
	_nSitesFull = _nSites * _nBasal;             
}

Frost::~Frost() {}

///////////////////////////////////////////////////////////////////////
// 2D lookup table 
///////////////////////////////////////////////////////////////////////

inline bool Frost::validQR(int q, int r) const {
	const int s = -q - r;
	return abs(q) <= _gridRadius && 
		   abs(r) <= _gridRadius &&
		   abs(s) <= _gridRadius;
}

inline int Frost::lookupIndex(int q, int r) const {
	const int S = 2 * _gridRadius + 1;
	return (q + _gridRadius) * S + (r + _gridRadius);
}

inline int Frost::indexFromQR(int q, int r) const {
	if (!validQR(q, r)) return -1;
	return _axialIndexLUT[lookupIndex(q, r)];
}

inline int Frost::indexFromQRC(int q, int r, int c) const {
    if (c < 0 || c >= _nBasal) return -1;

    int i2D = indexFromQR(q, r);
    return (i2D < 0) ? -1 : (c * _nSites + i2D);
}

void Frost::initLookupTable() {
	const int R = _gridRadius;
	const int N = _nSites;

	_axialCoords.clear();
	_axialCoords.reserve(N);

	// allows (q,r) -> index
	// 2d square lookup table
	const int S = 2 * R + 1;
	_axialIndexLUT.assign(S * S, -1);

	int i = 0;
	for (int q = -R; q <= R; q++) {
		const int rmin = max(-R, -q-R);
		const int rmax = min( R, -q+R);

		for (int r = rmin; r <= rmax; r++) {
			_axialCoords.push_back({q, r});

			const int L = lookupIndex(q,r);
			_axialIndexLUT[L] = i;

			i++;
		}
	}

	// 2d assignments
	// _attached.assign(N, 0);
    // _boundary.assign(N, 0);
    // _diffuseMass.assign(N, 0.0f);
    // _boundaryMass.assign(N, 0.0f);

    // _newAttachmentsIdx.clear();
    // _boundaryIdx.clear();
}

///////////////////////////////////////////////////////////////////////
// camera movement functions
///////////////////////////////////////////////////////////////////////

void Frost::rotateCamLeft() {
	float angle = -_rot * M_PI / 180.0f;
	float x = _camera.x, z = _camera.z;

	_camera.x =  x * cos(angle) + z * sin(angle);
	_camera.z = -x * sin(angle) + z * cos(angle);
}

void Frost::rotateCamRight() {
	float angle = _rot * M_PI / 180.0f;
	float x = _camera.x, z = _camera.z;

	_camera.x =  x * cos(angle) + z * sin(angle);
	_camera.z = -x * sin(angle) + z * cos(angle);
}

void Frost::updateCamera() {
	_view = glm::lookAt(_camera, _origin, _up);

	// bound
	glUseProgram(_boundRenderer);
	glUniformMatrix4fv(_boundU.view, 1, GL_FALSE, glm::value_ptr(_view));
	glUseProgram(0);

	// site
	glUseProgram(_siteRenderer);
	glUniformMatrix4fv(_siteU.view, 1, GL_FALSE, glm::value_ptr(_view));
	glUseProgram(0);

	// hex
	glUseProgram(_hexRenderer);
	glUniformMatrix4fv(_hexU.view, 1, GL_FALSE, glm::value_ptr(_view));
	glUseProgram(0);
}

///////////////////////////////////////////////////////////////////////
// initialization functions
///////////////////////////////////////////////////////////////////////

void Frost::initBorderSites() {
    _borderSites.assign(_nSitesFull, 0);

    for (int i3D = 0; i3D < _nSitesFull; ++i3D) {
        int c   = i3D / _nSites;
        int i2D = i3D - c * _nSites;

        // top / bottom layers
        if (c == 0 || c == _nBasal - 1) { 
			_borderSites[i3D] = 1; 
			continue; 
		}

        // outer axial ring for any layer
        int q = _axialCoords[i2D].q;
        int r = _axialCoords[i2D].r;
        if (abs(q) == _gridRadius || abs(r) == _gridRadius || abs(q + r) == _gridRadius) 
            _borderSites[i3D] = 1;
    }
}

void Frost::initStates() {
	_attached.assign(_nSitesFull, 0);
    _boundary.assign(_nSitesFull, 0);
    _diffuseMass.assign(_nSitesFull, 0.0f);
    _boundaryMass.assign(_nSitesFull, 0.0f);

    _newAttachmentsIdx.clear();
    _boundaryIdx.clear();
}

void Frost::initBoundRenderer() {
	_boundRenderer = createRenderProgram("./src/render/boundVert.glsl", 
										 "./src/render/boundFrag.glsl");

	_boundU.model = glGetUniformLocation(_boundRenderer, "uModel");
	_boundU.view  = glGetUniformLocation(_boundRenderer, "uView");
	_boundU.proj  = glGetUniformLocation(_boundRenderer, "uProj");

	glEnable(GL_DEPTH_TEST);

	const float aspect = (float)_xScreenRes / (float)_yScreenRes;
	_proj = glm::perspective(glm::radians(_fov), aspect, _near, _far);

	// upload proj
	glUseProgram(_boundRenderer);
	glUniformMatrix4fv(_boundU.proj, 1, GL_FALSE, glm::value_ptr(_proj));
	glUseProgram(0);

	// create square bound data
	const GLfloat verts[] = {
		-_xBound, 0.0f, -_zBound,
		 _xBound, 0.0f, -_zBound,
		 _xBound, 0.0f,  _zBound,
		-_xBound, 0.0f,  _zBound
	};
	const GLuint square_idx[] = { 0,1, 1,2, 2,3, 3,0 };
	_boundIndexCount = 8;

	glGenVertexArrays(1, &_boundVAO);
	glBindVertexArray(_boundVAO);

	glGenBuffers(1, &_boundVBO);
	glBindBuffer(GL_ARRAY_BUFFER, _boundVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	glGenBuffers(1, &_boundEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _boundEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(square_idx), square_idx, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	updateCamera();
}

void Frost::initSiteRenderer() {
	_siteRenderer = createRenderProgram("./src/render/siteVert.glsl", 
										"./src/render/siteFrag.glsl");

	_siteU.model = glGetUniformLocation(_siteRenderer, "uModel");
	_siteU.view  = glGetUniformLocation(_siteRenderer, "uView");
	_siteU.proj  = glGetUniformLocation(_siteRenderer, "uProj");

	glEnable(GL_DEPTH_TEST);

	const float aspect = (float)_xScreenRes / (float)_yScreenRes;
	_proj = glm::perspective(glm::radians(_fov), aspect, _near, _far);

	glUseProgram(_siteRenderer);
	glUniformMatrix4fv(_siteU.proj, 1, GL_FALSE, glm::value_ptr(_proj));
	glUseProgram(0);

	// ------------ BUILD 3D SITES -------------

	/* using axial coordinates to represent the hex grid
	hexes will be pointy top
	hexRadius = center to corner
	width = sqrt(3) * hexRadius
	height = 2 * hexRadius */

	const REAL SQRT3 = sqrt(3);

	vector<GLfloat> sites(3 * _nSitesFull); // vec3 for each site
	vector<GLint>   qrc3D(3 * _nSitesFull);

	int w = 0; // cursor for sites
	int u = 0; // cursor for qrc3D

	for (int c = 0; c < _nBasal; c++) {
        float y = (c - 0.5f * (_nBasal - 1)) * _hexThickness;

        for (int i = 0; i < _nSites; ++i) {
            const int q = _axialCoords[i].q;
            const int r = _axialCoords[i].r;

            // axial(q,r) -> world XZ (pointy-top hex)
            const float x = SQRT3 * _hexRadius * (q + 0.5f * r);
            const float z = 1.5f  * _hexRadius * r;

            sites[w++] = -x;
            sites[w++] =  y;  // layer offset 
            sites[w++] = -z;

            qrc3D[u++] = q;
            qrc3D[u++] = r;   // (q,r) same for every layer
            qrc3D[u++] = c;   
        }
    }

	// upload to GPU

	glGenVertexArrays(1, &_siteVAO);
    glBindVertexArray(_siteVAO);

    glGenBuffers(1, &_siteVBO);
    glBindBuffer(GL_ARRAY_BUFFER, _siteVBO);
    glBufferData(GL_ARRAY_BUFFER, sites.size() * sizeof(GLfloat),
                 sites.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &_siteQRCVBO);
    glBindBuffer(GL_ARRAY_BUFFER, _siteQRCVBO);
    glBufferData(GL_ARRAY_BUFFER, qrc3D.size() * sizeof(GLint),
                 qrc3D.data(), GL_STATIC_DRAW);
    glVertexAttribIPointer(1, 3, GL_INT, 3*sizeof(GLint), (void*)0);
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    updateCamera();
}

void Frost::initHexMesh() {
	// height = 1, unit radius in xz
	vector<vec3> verts;
	verts.reserve(12);
	vector<GLuint> idx;

	const REAL angle = M_PI / 3.0f;

	// top ring: vertices 0-5
	for (int k = 0; k < 6; k++) {
		REAL v = angle * k + M_PI / 6.0f;
		verts.emplace_back(cos(v), 0.5f, sin(v));
	}

	// bottom ring: vertices 6-11
	for (int k = 0; k < 6; k++) {
		REAL v = angle * k + M_PI / 6.0f;
		verts.emplace_back(cos(v), -0.5f, sin(v));
	}
	
	// triangulate fan from vertex 0
	for (int k = 1; k < 5; k++) {
		idx.push_back(0);
		idx.push_back(k);
		idx.push_back(k + 1);
	}
	// triangulate fan from vertex 6: REVERSE WINDING
	for (int k = 1; k < 5; k++) {
		idx.push_back(6);
		idx.push_back(6 + k + 1);
		idx.push_back(6 + k);
	}

	// side quads, each made of two triangles
	for (int k = 0; k < 6; ++k) {
		int kn = (k + 1) % 6;
		int t0 = k,    t1 = kn;     // top ring
		int b0 = 6+k,  b1 = 6+kn;   // bottom ring

		// (t0, b0, b1) and (t0, b1, t1)
		idx.push_back((GLuint)t0); idx.push_back((GLuint)b0); idx.push_back((GLuint)b1);
		idx.push_back((GLuint)t0); idx.push_back((GLuint)b1); idx.push_back((GLuint)t1);
	}

	_hexIndexCount = (int)idx.size();

	// for edges 
	vector<GLuint> eidx;
	// top ring edges
	for (int k=0;k<6;++k) {
		int kn=(k+1)%6;
		eidx.push_back(k); eidx.push_back(kn);
	}
	// vertical edges
	for (int k=0;k<6;++k) {
		eidx.push_back(k); eidx.push_back(6+k);
	}
	// bottom ring edges
	for (int k=0;k<6;++k) {
		int kn=(k+1)%6;
		eidx.push_back(6+k); eidx.push_back(6+kn);
	}
	_hexEdgeIndexCount = (GLsizei)eidx.size();

	// upload to GPU
	glGenVertexArrays(1, &_hexVAO);
	glBindVertexArray(_hexVAO);

	glGenBuffers(1, &_hexVBO);
	glBindBuffer(GL_ARRAY_BUFFER, _hexVBO);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3), verts.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0); // aPos @ loc 0
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &_hexEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _hexEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(GLuint), idx.data(), GL_STATIC_DRAW);

	glBindVertexArray(_hexVAO);
	glGenBuffers(1, &_hexEdgeEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _hexEdgeEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, eidx.size()*sizeof(GLuint), eidx.data(), GL_STATIC_DRAW);
	glBindVertexArray(0);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Frost::initInstances()
{
    std::vector<GLfloat> centers(3 * _nSitesFull);
    const float SQRT3 = sqrt(3.0f);

    int w = 0;
    for (int c = 0; c < _nBasal; c++) {
        float y = (c - 0.5f * (_nBasal - 1)) * _hexThickness; // layer spacing

        for (int i2D = 0; i2D < _nSites; ++i2D) {
            const int q = _axialCoords[i2D].q;
            const int r = _axialCoords[i2D].r;

            const float x = SQRT3 * _hexRadius * (q + 0.5f * r);
            const float z = 1.5f  * _hexRadius * r;

            centers[w++] = -x;
            centers[w++] =  y;   // layer offset here
            centers[w++] = -z;
        }
    }

	// upload instance data
    glBindVertexArray(_hexVAO);

    // centers - loc 1
    glGenBuffers(1, &_instVBO);
    glBindBuffer(GL_ARRAY_BUFFER, _instVBO);
    glBufferData(GL_ARRAY_BUFFER, centers.size() * sizeof(GLfloat),
                 centers.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);

	// attached flags - loc 2  
    // glGenBuffers(1, &_attachVBO);
    // glBindBuffer(GL_ARRAY_BUFFER, _attachVBO);
    // glBufferData(GL_ARRAY_BUFFER, _attached.size() * sizeof(GLint),
    //              _attached.data(), GL_DYNAMIC_DRAW);
    // glVertexAttribIPointer(2, 1, GL_INT, sizeof(GLint), (void*)0);
    // glEnableVertexAttribArray(2);
    // glVertexAttribDivisor(2, 1);

	// // boundary flags - loc 3
    // glGenBuffers(1, &_boundaryVBO);
    // glBindBuffer(GL_ARRAY_BUFFER, _boundaryVBO);
    // glBufferData(GL_ARRAY_BUFFER, _boundary.size() * sizeof(GLint),
    //              _boundary.data(), GL_DYNAMIC_DRAW);
    // glVertexAttribIPointer(3, 1, GL_INT, sizeof(GLint), (void*)0);
    // glEnableVertexAttribArray(3);
    // glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Frost::initHexRenderer()
{
    _hexRenderer = createRenderProgram("./src/render/hexVert.glsl",
                                         "./src/render/hexFrag.glsl");
	_hexEdgeRenderer = createRenderProgram("./src/render/edgeVert.glsl",
										   "./src/render/edgeFrag.glsl");

    _hexU.model = glGetUniformLocation(_hexRenderer, "uModel");
    _hexU.view  = glGetUniformLocation(_hexRenderer, "uView");
    _hexU.proj  = glGetUniformLocation(_hexRenderer, "uProj");


	_edgeU.model = glGetUniformLocation(_hexEdgeRenderer, "uModel");
    _edgeU.view  = glGetUniformLocation(_hexEdgeRenderer, "uView");
    _edgeU.proj  = glGetUniformLocation(_hexEdgeRenderer, "uProj");

}

void Frost::init() {
	
	initLookupTable();
	initNeighborLUT();

	initStates(); 
	seed();
	initBorderSites();

	initHexMesh();
	initInstances();

	initSiteRenderer();
	initBoundRenderer();
	initHexRenderer();

	initGPU();
}

///////////////////////////////////////////////////////////////////////
// render functions
///////////////////////////////////////////////////////////////////////

void Frost::renderBounds() {
	glUseProgram(_boundRenderer);

	glUniformMatrix4fv(_boundU.model, 1, GL_FALSE, glm::value_ptr(mat4(1.0f)));

	glBindVertexArray(_boundVAO);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glLineWidth(2.0f);
	glDrawElements(GL_LINES, _boundIndexCount, GL_UNSIGNED_INT, 0);

	glBindVertexArray(0);
	glUseProgram(0);
}

void Frost::renderSites() {
	glUseProgram(_siteRenderer);

    const GLint highlightQ = 0, highlightR = 0, highlightC = 0;
    GLint uHQR = glGetUniformLocation(_siteRenderer, "uHighlightQRC");

	// set uniforms
    glUniform3i(uHQR, highlightQ, highlightR, highlightC);
    glUniformMatrix4fv(_siteU.model, 1, GL_FALSE, glm::value_ptr(mat4(1.0f)));
    glUniformMatrix4fv(_siteU.proj, 1, GL_FALSE, glm::value_ptr(_proj));
    glUniformMatrix4fv(_siteU.view, 1, GL_FALSE, glm::value_ptr(_view));

    glEnable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_DEPTH_TEST); 

    glBindVertexArray(_siteVAO);
    glDrawArrays(GL_POINTS, 0, _nSitesFull);  
	
    glBindVertexArray(0);
    glUseProgram(0);
}

void Frost::renderHexes()
{
    glUseProgram(_hexRenderer);


	// set uniforms
    glUniformMatrix4fv(_hexU.model, 1, GL_FALSE, glm::value_ptr(mat4(1.0f)));
    glUniformMatrix4fv(_hexU.view,  1, GL_FALSE, glm::value_ptr(_view));
    glUniformMatrix4fv(_hexU.proj,  1, GL_FALSE, glm::value_ptr(_proj));
    glUniform1f(glGetUniformLocation(_hexRenderer, "uHexScale"), _hexRadius);
    glUniform1f(glGetUniformLocation(_hexRenderer, "uHeight"), _hexThickness);
   
	glBindVertexArray(_hexVAO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _hexEBO);

	// make sure the render shader sees the live flags
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _ssboAttached);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, _ssboBoundary);

	// ensure compute writes are visible to the render stage
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);


	// pass 0: crystal
	glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glUniform1i(glGetUniformLocation(_hexRenderer, "uPass"), 0);
	glUniform1i(glGetUniformLocation(_hexRenderer, "uShowBoundary"), showBoundary);
    glDrawElementsInstanced(GL_TRIANGLES, _hexIndexCount, GL_UNSIGNED_INT, 0, _nSitesFull);

    // pass 1: boundary
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);          // don't write depth so multiple boundary layers can blend
	glUniform1i(glGetUniformLocation(_hexRenderer, "uPass"), 1);
	glUniform1i(glGetUniformLocation(_hexRenderer, "uShowBoundary"), showBoundary);
    glDrawElementsInstanced(GL_TRIANGLES, _hexIndexCount, GL_UNSIGNED_INT, 0, _nSitesFull);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_TRUE);

    glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glBindVertexArray(0);

    glUseProgram(0);
}

void Frost::renderHexEdges()
{
    glUseProgram(_hexEdgeRenderer);

    // --- make sure the edge shader can see live flags ---
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _ssboAttached);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, _ssboBoundary);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // uniforms
    GLint uM = glGetUniformLocation(_hexEdgeRenderer,"uModel");
    GLint uV = glGetUniformLocation(_hexEdgeRenderer,"uView");
    GLint uP = glGetUniformLocation(_hexEdgeRenderer,"uProj");
    GLint uScale   = glGetUniformLocation(_hexEdgeRenderer,"uHexScale");
    GLint uHeight  = glGetUniformLocation(_hexEdgeRenderer,"uHeight");
    GLint uEdgeEps = glGetUniformLocation(_hexEdgeRenderer,"uEdgeEps");

    glUniformMatrix4fv(uM,1,GL_FALSE,glm::value_ptr(mat4(1.0f)));
    glUniformMatrix4fv(uV,1,GL_FALSE,glm::value_ptr(_view));
    glUniformMatrix4fv(uP,1,GL_FALSE,glm::value_ptr(_proj));
    glUniform1f(uScale,   _hexRadius);
    glUniform1f(uHeight,  _hexThickness);
    glUniform1f(uEdgeEps, 1e-4f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(0.5f);

    glBindVertexArray(_hexVAO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _hexEdgeEBO);
    glDrawElementsInstanced(GL_LINES, _hexEdgeIndexCount, GL_UNSIGNED_INT, 0, _nSitesFull);

    glBindVertexArray(0);
    glUseProgram(0);
}


void Frost::render() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// renderBounds(mat4(1.0f));

	renderHexes();	

	if (showSites) renderSites();
	if (showEdges) renderHexEdges();
}

///////////////////////////////////////////////////////////////////////
// timestepping functions
///////////////////////////////////////////////////////////////////////

void Frost::seed() {

	int c0 = _nBasal / 2;

	// seeding initial crystal	
    _attached[indexFromQRC( 0, 0,c0)] = 1;
	// _attached[indexFromQRC( 0,-1,c0)] = 1;
	// _attached[indexFromQRC( 1,-1,c0)] = 1;
	// _attached[indexFromQRC( 1, 0,c0)] = 1;
	// _attached[indexFromQRC( 0, 1,c0)] = 1;
	// _attached[indexFromQRC(-1, 1,c0)] = 1;
	// _attached[indexFromQRC(-1, 0,c0)] = 1;

	// seeding a line
	// int width = 40;
	// for (int i = -width; i < width; i++) 
	// 	_attached[indexFromQRC(i,0,c0)] = 1;

	// mark boundary
	findBoundarySites();

	// seed diffusive vapor on A complement
	for (int i = 0; i < _nSitesFull; i++) {
		_diffuseMass[i] = _attached[i] ? 0.0f : _rho;
		_boundaryMass[i] = 0.0f;
	}
}

void Frost::diffuse() {

	// buffers for steps
	vector<REAL> d1(_nSitesFull, 0.0f); // planar
	vector<REAL> d2(_nSitesFull, 0.0f); // basal
	
	// planar diffusion: d -> d`
	for (int i = 0; i < _nSitesFull; ++i) {
        if (_attached[i])      { d1[i] = 0.0f; continue; }
        if (_borderSites[i])   { d1[i] = _diffuseMass[i]; continue; } // skip computing at borders

        const int base = i * N_NBR;
        float sum = _diffuseMass[i]; // include center

		// 0...5 are planar neighbors
        for (int k = 0; k < 6; ++k) {
            const int j = _neighborLUT[base + k];
			sum += _diffuseMass[j];
        }		
        d1[i] = sum * (1.0f / 7.0f);
    }

	// basal diffusion: d' -> d''
	for (int i = 0; i < _nSitesFull; ++i) {
        if (_attached[i])    { d2[i] = 0.0f; continue; }
        if (_borderSites[i]) { d2[i] = d1[i]; continue; } // carry through

        const int base = i * N_NBR;

		// 6...7 are basal neighbors
        float acc = (4.0f / 7.0f) * d1[i];
        for (int k = 6; k <= 7; ++k) {
            const int j = _neighborLUT[base + k];

			acc += (3.0f / 14.0f) * d1[j];
        }
        d2[i] = acc;
    }

	_diffuseMass.swap(d2);
}

void Frost::freeze() {
	for (int i = 0; i < _nSitesFull; i++) {
		if (_attached[i] || _borderSites[i] || !_boundary[i]) continue; // only on border

		float d0 = _diffuseMass[i];
		if (d0 < 0.0f) continue;

		int nT=0, nZ=0, nTcapped, nZcapped;
		const int base = i * N_NBR;

    	// 6 planar neighbors
		for (int k = 0; k < 6; ++k) {
			int j = _neighborLUT[base + k];
			if (j >= 0 && _attached[j]) nT++; 
		}
		// 2 vertical neighbors (cap at 1)
		for (int k = 6; k <= 7; ++k) {
			int j = _neighborLUT[base + k];
			if (j >= 0 && _attached[j]) nZ++;
		}

		nTcapped = max(nT, 3);
		nZcapped = max(nZ, 1);

		float k = _kappa[nZcapped][nTcapped];          
                       
        float take = (1.0f - k) * d0;
        _diffuseMass[i]  = k * d0;
        _boundaryMass[i] += take;
	}
}

void Frost::attach() {
	_newAttachmentsIdx.clear();
    REAL eps = 1e-7f;

    for (int i = 0; i < _nSitesFull; ++i) {
        // Only consider current boundary gas cells in the physical domain
        if (_attached[i] || _borderSites[i] || !_boundary[i])   continue;

        int nT=0, nZ=0, nTcapped, nZcapped;
        const int base = i * N_NBR;

    	// 6 planar neighbors
		for (int k = 0; k < 6; ++k) {
			int j = _neighborLUT[base + k];
			if (j >= 0 && _attached[j]) nT++; 
		}
		// 2 vertical neighbors (cap at 1)
		for (int k = 6; k <= 7; ++k) {
			int j = _neighborLUT[base + k];
			if (j >= 0 && _attached[j]) nZ++;
		}

		nTcapped = max(nT, 3);
		nZcapped = max(nZ, 1);

        const float beta = _beta[nZcapped][nTcapped];
        if (_boundaryMass[i] + eps < beta) continue; // not ready yet

        // attach: site joins A 
        _attached[i] = 1;
        _newAttachmentsIdx.push_back(i);

        // Clean up local fields at the newly attached site
        _boundaryMass[i] = 0.0f;
        _diffuseMass[i]  = 0.0f;
    }
}

void Frost::timestep() {

	// diffusion on A_c
	diffuse();

	// find the boundary sites
	findBoundarySites();

	// freezing on boundary
	freeze();

	// attachment decision
	attach();

	// find boundary sites again
	findBoundarySites();

	// melting on boundary


	// upload new attached and boundary
	// attached flags 
    glBindBuffer(GL_ARRAY_BUFFER, _attachVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, _attached.size() * sizeof(GLint), _attached.data());

    // boundary flags 
    glBindBuffer(GL_ARRAY_BUFFER, _boundaryVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, _boundary.size() * sizeof(GLint), _boundary.data());
	
}

void Frost::initNeighborLUT() {

	_neighborLUT.assign(_nSitesFull * N_NBR, -1);

	// qr directions
	static int dq[6] = {  0,  1,  1,  0, -1, -1};
	static int dr[6] = { -1, -1,  0,  1,  1,  0};

	for (int i3D = 0; i3D < _nSitesFull; i3D++) {
		const int c   = i3D / _nSites;
        const int i2D = i3D - c * _nSites;

        const int q = _axialCoords[i2D].q;
        const int r = _axialCoords[i2D].r;

		// skip border sites
        if (abs(q) == _gridRadius || 
		    abs(r) == _gridRadius || 
		    abs(q + r) == _gridRadius) continue;

		// 6 planar neighbors, dont compute for top and bottom layer
        for (int k = 0; k < 6; k++) 
            _neighborLUT[i3D * N_NBR + k] = indexFromQRC(q + dq[k], r + dr[k], c);
        
		// basal neighbors
        _neighborLUT[i3D * N_NBR + 6] = (c > 0)           ? (i3D - _nSites) : -1; // below
        _neighborLUT[i3D * N_NBR + 7] = (c + 1 < _nBasal) ? (i3D + _nSites) : -1;  
	}	
}

int Frost::numAttachedNeighbors(int i3D) {
	int sum = 0;
    const int base = i3D * N_NBR;
    for (int k = 0; k < N_NBR; ++k) {
        const int j = _neighborLUT[base + k];
        if (j >= 0) sum += _attached[j];
    }
    return sum;
}

void Frost::findBoundarySites() {
	fill(_boundary.begin(), _boundary.end(), 0);

    for (int i = 0; i < _nSitesFull; i++) {
        if (_attached[i]) continue;

        for (int k = 0; k < N_NBR; k++) {
            const int j = _neighborLUT[i * N_NBR + k];

            if (j >= 0 && _attached[j]) { 
				_boundary[i] = 1; 
				break; 
			}
        }
    }
}

