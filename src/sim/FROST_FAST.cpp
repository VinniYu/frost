#include "FROST.h"
#include "./src/util/PPM.h"


///////////////////////////////////////////////////////////////////////`
// GPU FUNCTIONS
///////////////////////////////////////////////////////////////////////


void Frost::initGPU() {
    uploadNeighborTable();
    uploadMasksAndMass();
    uploadBoundary();

    createPrograms();


}

void Frost::createPrograms() {
    _progDiffusePlanar = createComputeProgram("./src/compute/diffusePlanar.glsl");
    _progDiffuseBasal = createComputeProgram("./src/compute/diffuseBasal.glsl");
    _progFindBoundary = createComputeProgram("./src/compute/findBoundary.glsl");
    _progFreeze = createComputeProgram("./src/compute/freeze.glsl");   
    _progAttach = createComputeProgram("./src/compute/attach.glsl");
    _progMelt = createComputeProgram("./src/compute/melt.glsl");   
}

///////////////////////////////////////////////////////////////////////
// upload functions
///////////////////////////////////////////////////////////////////////


void Frost::uploadNeighborTable() {
    
    // create the buffer
    if (_ssboNeighbors == 0) {
        glGenBuffers(1, &_ssboNeighbors);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboNeighbors);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 sizeof(int) * _neighborLUT.size(),
                 _neighborLUT.data(),
                 GL_STATIC_DRAW);

    // binded to 0
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboNeighbors);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void Frost::uploadMasksAndMass() {

    // massA - upload diffuseMass
    if (_ssboMassA == 0) glGenBuffers(1, &_ssboMassA);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboMassA);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 sizeof(float) * _diffuseMass.size(),
                 _diffuseMass.data(),
                 GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ssboMassA);

    // massB - starts empty to write into
    if (_ssboMassB == 0) glGenBuffers(1, &_ssboMassB);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboMassB);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 sizeof(float) * _nSitesFull,
                 nullptr,
                 GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssboMassB);

    // boundary mass
    if (_ssboBoundaryMass == 0) glGenBuffers(1, &_ssboBoundaryMass);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboBoundaryMass);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                    sizeof(float) * _nSitesFull,
                    _boundaryMass.data(),  
                    GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, _ssboBoundaryMass);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // attached
    if (_ssboAttached == 0) glGenBuffers(1, &_ssboAttached);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboAttached);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 sizeof(GLuint) * _nSitesFull,
                 _attached.data(),
                 GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _ssboAttached);

    // border
    if (_ssboBorder == 0) glGenBuffers(1, &_ssboBorder);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboBorder);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 sizeof(GLuint) * _nSitesFull,
                 _borderSites.data(),
                 GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, _ssboBorder);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

}

void Frost::uploadBoundary() {
    glGenBuffers(1, &_ssboBoundary);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboBoundary);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                    sizeof(GLuint) * _nSitesFull,
                    nullptr,               
                    GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, _ssboBoundary);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
}

void Frost::setKappa(GLuint prog) {
    GLint locKappa1 = glGetUniformLocation(prog, "kappa1");
    GLint locKappa2 = glGetUniformLocation(prog, "kappa2");

    glUniform1fv(locKappa1, 4, &_kappa[0][0]); // nZcapped = 1
    glUniform1fv(locKappa2, 4, &_kappa[1][0]); // nZcapped = 2
}

void Frost::setBeta(GLuint prog) {
    GLint locBeta1 = glGetUniformLocation(prog, "beta1");
    GLint locBeta2 = glGetUniformLocation(prog, "beta2");

    glUniform1fv(locBeta1, 4, &_beta[0][0]); // nZcapped = 1
    glUniform1fv(locBeta2, 4, &_beta[1][0]); // nZcapped = 2
}

void Frost::setMu(GLuint prog) {
    GLint locMu1 = glGetUniformLocation(prog, "mu1");
    GLint locMu2 = glGetUniformLocation(prog, "mu2");

    glUniform1fv(locMu1, 4, &_mu[0][0]);
    glUniform1fv(locMu2, 4, &_mu[1][0]);
}

///////////////////////////////////////////////////////////////////////`
// gpu timestepping functions
///////////////////////////////////////////////////////////////////////


void Frost::gpuDiffuse() {
    
    // planar diffuse
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboNeighbors);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ssboMassA);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssboMassB);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _ssboAttached);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, _ssboBorder);

    glUseProgram(_progDiffusePlanar);
    glUniform1ui(glGetUniformLocation(_progDiffusePlanar, "nSitesFull"), (GLuint)_nSitesFull);

    const GLuint WG = 128;
    GLuint groups = (_nSitesFull + WG - 1) / WG;
    glDispatchCompute(groups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    glUseProgram(0);

    // ping pong
    swap(_ssboMassA, _ssboMassB);

    // basal diffuse
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboNeighbors);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ssboMassA);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssboMassB);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _ssboAttached);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, _ssboBorder);

    glUseProgram(_progDiffuseBasal);
    glUniform1ui(glGetUniformLocation(_progDiffuseBasal, "nSitesFull"), (GLuint)_nSitesFull);
    glDispatchCompute(groups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    glUseProgram(0);

    // ping pong
    swap(_ssboMassA, _ssboMassB);
}

void Frost::gpuFindBoundary() {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboNeighbors);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _ssboAttached);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, _ssboBorder);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, _ssboBoundary);

    glUseProgram(_progFindBoundary);
    glUniform1ui(glGetUniformLocation(_progFindBoundary, "nSitesFull"),
                 (GLuint)_nSitesFull);

    const GLuint WG = 128;
    GLuint groups = (_nSitesFull + WG - 1) / WG;
    glDispatchCompute(groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    glUseProgram(0);
}

void Frost::gpuFreeze() {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboNeighbors);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ssboMassA);        // read
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssboMassB);        // write
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _ssboAttached);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, _ssboBorder);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, _ssboBoundary);     // from findBoundaryGPU
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, _ssboBoundaryMass); 

    glUseProgram(_progFreeze);
    glUniform1ui(glGetUniformLocation(_progFreeze, "nSitesFull"), (GLuint)_nSitesFull);
    setKappa(_progFreeze);

    const GLuint WG = 128;
    GLuint groups = (_nSitesFull + WG - 1) / WG;
    glDispatchCompute(groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    glUseProgram(0);

    // ping-pong mass for next stage
    swap(_ssboMassA, _ssboMassB);
}

void Frost::gpuAttach() {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboNeighbors);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ssboMassA);       // diffuseMass (read/write)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssboMassB);       // diffuseMass (read/write)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _ssboAttached);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, _ssboBorder);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, _ssboBoundary);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, _ssboBoundaryMass);

    glUseProgram(_progAttach);
    glUniform1ui(glGetUniformLocation(_progAttach, "nSitesFull"), (GLuint)_nSitesFull);
    glUniform1f(glGetUniformLocation(_progAttach, "eps"), 1e-7f);
    setBeta(_progAttach);

    const GLuint WG = 128;
    GLuint groups = (_nSitesFull + WG - 1) / WG;
    glDispatchCompute(groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    glUseProgram(0);

	swap(_ssboMassA, _ssboMassB);
}

void Frost::gpuMelt() {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboNeighbors);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ssboMassA);        // read vapor
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssboMassB);        // write vapor
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _ssboAttached);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, _ssboBorder);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, _ssboBoundary);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, _ssboBoundaryMass); // read/write boundary layer

    glUseProgram(_progMelt);
    glUniform1ui(glGetUniformLocation(_progMelt, "nSitesFull"), (GLuint)_nSitesFull);
    setMu(_progMelt);

    const GLuint WG = 128;
    GLuint groups = (_nSitesFull + WG - 1) / WG;
    glDispatchCompute(groups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    glUseProgram(0);

    // ping-pong for next stage
    swap(_ssboMassA, _ssboMassB);
}


void Frost::gpuTimestep() {

    // diffusion on A_c
	gpuDiffuse();

	// find the boundary sites
	gpuFindBoundary();

	// freezing on boundary
	gpuFreeze();

	// attachment decision
	gpuAttach();

	// find boundary sites again
	gpuFindBoundary();
    
    // gpuMelt();          

    // gpuFindBoundary();
}

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


void Frost::createHeightMap() {
    // read ssbo attached back to the CPU for processing
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    glFinish(); // block this function from running until all GL work is done SUPER HELPFUL

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboAttached);

    GLint64 byteSize = 0;
    glGetBufferParameteri64v(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &byteSize);

    // copy ssbo data
    vector<GLuint> attached32(_nSitesFull, 0);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, byteSize, attached32.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    vector<uint8_t> attached(_nSitesFull, 0);
    for (size_t i = 0; i < attached32.size(); i++) 
        attached[i] = (attached32[i] != 0) ? 1u : 0u;
    

    // STEP 2: find top layer of basal planes
    vector<int> topBasal(_nSites, -1);
    for (int base = 0; base < _nSites; base++) {
        for (int c = _nBasal - 1; c>=0; c--) {
            const size_t gid = static_cast<size_t>(c) * static_cast<size_t>(_nSites) + static_cast<size_t>(base);
            if (attached[gid]) {
                topBasal[base] = c;
                break; // highest layer found for this column
            }
        }
    }

    // STEP 3: super sample the heights
    const float xMin = -_xBound, xMax = _xBound;
    const float zMin = -_zBound, zMax =  _zBound;

    // pixel sizes in world space
    const float px = (xMax - xMin) / static_cast<float>(_ppmW);
    const float pz = (zMax - zMin) / static_cast<float>(_ppmH);

    // supersample offsets
    int nSample = 3;
    vector<pair<float, float>> sampleOffsets;
    for (int y = 0; y < nSample; y++) {
        for (int x = 0; x < nSample; x++) {
            const float offx = (static_cast<float>(x) + 0.5f) / static_cast<float>(nSample) - 0.5f;
            const float offz = (static_cast<float>(y) + 0.5f) / static_cast<float>(nSample) - 0.5f;
            sampleOffsets.emplace_back(offx, offz);
        }
    }

    const float size      = _hexRadius;
    const float invSize   = 1.0f / size;
    const float A = (sqrt(3.0f) / 3.0f) * invSize; 
    const float B = (1.0f / 3.0f)       * invSize; 
    const float C = (2.0f / 3.0f)       * invSize; 
    const float yMin = (-0.5f * static_cast<float>(_nBasal - 1)) * _hexThickness;
    const float yMax = (+0.5f * static_cast<float>(_nBasal - 1)) * _hexThickness;
    
    // raster pass
    vector<float> height(_ppmH * _ppmW, yMin);

    for (int j = 0; j < _ppmH; ++j) {
        const float zc = zMin + (static_cast<float>(j) + 0.5f) * pz;

        for (int i = 0; i < _ppmW; ++i) {
            const float xc = xMin + (static_cast<float>(i) + 0.5f) * px;

            float hPixel = yMin;  // start at background (top surface uses max)

            // Supersample taps
            for (const auto& off : sampleOffsets) {
                const float x = xc + off.first  * px;
                const float z = zc + off.second * pz;

                // q_f = B * zp - A * xp
                // r_f = -C * zp
                const float qf = B * z - A * x;
                const float rf = -C * z;

                float xc_f = qf;
                float zc_f = rf;
                float yc_f = -xc_f - zc_f;

                int X = static_cast<int>(round(xc_f));
                int Z = static_cast<int>(round(zc_f));
                int Y = static_cast<int>(round(yc_f));

                float dxr = fabs(static_cast<float>(X) - xc_f);
                float dyr = fabs(static_cast<float>(Y) - yc_f);
                float dzr = fabs(static_cast<float>(Z) - zc_f);

                if (dxr > dyr && dxr > dzr) X = -Y - Z;
                else if (dyr > dzr) Y = -X - Z;
                else Z = -X - Y;
                
                if (!validQR(X, Z)) {
                    continue; // tap lands outside hex disc: background
                }

                const int i2D = indexFromQR(X, Z);
                const int cTop = topBasal[i2D];
                if (cTop < 0) continue; // no attached cell in this column
                
                // convert layer index to world y
                const float yTop = (static_cast<float>(cTop) - 0.5f * static_cast<float>(_nBasal - 1)) * _hexThickness;

                if (yTop > hPixel) hPixel = yTop;
            }

            // write out pixel height
            height[j * _ppmW + i] = hPixel;
        }
    }
    
    // write to a ppm file
    const float norm = yMax - yMin;
    writeMapPNG(norm, _ppmW, _ppmH, height, yMin, "./media/heightMap.png");
}

void Frost::createDensityMap() {
    // read ssbo attached back to the CPU for processing
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    glFinish(); // block this function from running until all GL work is done SUPER HELPFUL

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboAttached);

    GLint64 byteSize = 0;
    glGetBufferParameteri64v(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &byteSize);

    // copy ssbo data
    vector<GLuint> attached32(_nSitesFull, 0);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, byteSize, attached32.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    vector<uint8_t> attached(_nSitesFull, 0);
    for (size_t i = 0; i < attached32.size(); i++) 
        attached[i] = (attached32[i] != 0) ? 1u : 0u;

    // 2) Build per-planar column counts: colCount[i2D] ∈ [0, _nBasal]
    std::vector<int> colCount(_nSites, 0);
    for (int i2D = 0; i2D < _nSites; ++i2D) {
        int count = 0;
        // scan all basal layers for that planar column
        for (int c = 0; c < _nBasal; ++c) {
            const size_t gid = static_cast<size_t>(c) * static_cast<size_t>(_nSites)
                             + static_cast<size_t>(i2D);
            count += attached[gid] ? 1 : 0;
        }
        colCount[i2D] = count;
    }

    // 3) Supersample setup
    const float xMin = -_xBound, xMax = _xBound;
    const float zMin = -_zBound, zMax =  _zBound;

    const float px = (xMax - xMin) / static_cast<float>(_ppmW);
    const float pz = (zMax - zMin) / static_cast<float>(_ppmH);

    // 3×3 taps (match your height map)
    const int nSample = 3;
    std::vector<std::pair<float,float>> sampleOffsets;
    sampleOffsets.reserve(nSample * nSample);
    for (int sy = 0; sy < nSample; ++sy) {
        for (int sx = 0; sx < nSample; ++sx) {
            const float offx = (static_cast<float>(sx) + 0.5f) / static_cast<float>(nSample) - 0.5f;
            const float offz = (static_cast<float>(sy) + 0.5f) / static_cast<float>(nSample) - 0.5f;
            sampleOffsets.emplace_back(offx, offz);
        }
    }

    // Inverse axial constants (use same convention you used in createHeightMap)
    const float size    = _hexRadius;
    const float invSize = 1.0f / size;
    const float A = (std::sqrt(3.0f) / 3.0f) * invSize;
    const float B = (1.0f / 3.0f)            * invSize;
    const float C = (2.0f / 3.0f)            * invSize;

    // Density/thickness range: [0, _nBasal * _hexThickness]
    const float yMin = 0.0f;
    const float yMax = static_cast<float>(_nBasal) * _hexThickness;

    // 4) raster pass: average thickness over supersample taps
    std::vector<float> outVal(static_cast<size_t>(_ppmW) * static_cast<size_t>(_ppmH), 0.0f);

    for (int j = 0; j < _ppmH; ++j) {
        const float zc = zMin + (static_cast<float>(j) + 0.5f) * pz;

        for (int i = 0; i < _ppmW; ++i) {
            const float xc = xMin + (static_cast<float>(i) + 0.5f) * px;

            float sumVal = 0.0f;
            int   validT = 0;

            for (const auto& off : sampleOffsets) {
                const float x = xc + off.first  * px;
                const float z = zc + off.second * pz;

                // q_f = B * z - A * x
                // r_f = -C * z
                const float qf = B * z - A * x;
                const float rf = -C * z;

                // cube rounding
                float xc_f = qf;
                float zc_f = rf;
                float yc_f = -xc_f - zc_f;

                int X = static_cast<int>(round(xc_f));
                int Z = static_cast<int>(round(zc_f));
                int Y = static_cast<int>(round(yc_f));

                const float dxr = fabs(static_cast<float>(X) - xc_f);
                const float dyr = fabs(static_cast<float>(Y) - yc_f);
                const float dzr = fabs(static_cast<float>(Z) - zc_f);

                if (dxr > dyr && dxr > dzr) X = -Y - Z;
                else if (dyr > dzr)         Y = -X - Z;
                else                        Z = -X - Y;

                if (!validQR(X, Z)) continue;

                const int i2D = indexFromQR(X, Z);
                if (i2D < 0) continue;

                // thickness for this tap = (#attached in column) * layer thickness
                const int   count  = colCount[i2D];                 // 0.._nBasal
                const float thick  = static_cast<float>(count) * _hexThickness;

                sumVal += thick;
                ++validT;
            }

            // mean over taps (area average); if no valid taps, leave as 0
            const float pixelVal = (validT > 0) ? (sumVal / static_cast<float>(validT)) : 0.0f;
            outVal[static_cast<size_t>(j) * static_cast<size_t>(_ppmW) + static_cast<size_t>(i)] = pixelVal;
        }
    }

    const float norm = yMax - yMin; 
    for (auto &v : outVal)
        v *= 10.0f;
    writeMapPNG(norm, _ppmW, _ppmH, outVal, yMin, "./media/densityMap.png");

}

