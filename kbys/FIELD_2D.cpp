#include "FIELD_2D.h"
#include <cassert>

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D::FIELD_2D(const int &rows, const int &cols) : _xRes(rows), _yRes(cols)
{
  _totalCells = _xRes * _yRes;
  _data = new float[_totalCells];

  for (int x = 0; x < _totalCells; x++)
    _data[x] = 0.0;
}

FIELD_2D::FIELD_2D(const FIELD_2D &m) : _xRes(m.xRes()), _yRes(m.yRes())
{
  _totalCells = _xRes * _yRes;
  _data = new float[_totalCells];

  for (int x = 0; x < _totalCells; x++)
    _data[x] = m[x];
}

FIELD_2D::FIELD_2D() : _xRes(0), _yRes(0), _totalCells(0), _data(NULL)
{
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D::~FIELD_2D()
{
  delete[] _data;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
void FIELD_2D::clear()
{
  for (int x = 0; x < _totalCells; x++)
    _data[x] = 0.0;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
void FIELD_2D::write(string filename) const
{
  FILE *file;
  file = fopen(filename.c_str(), "wb");
  if (file == NULL) {
    cout << __FILE__ << " " << __FUNCTION__ << " " << __LINE__ << " : " << endl;
    cout << " FIELD_2D write failed! " << endl;
    cout << " Could not open file " << filename.c_str() << endl;
    exit(0);
  }

  // write dimensions
  fwrite((void *)&_xRes, sizeof(int), 1, file);
  fwrite((void *)&_yRes, sizeof(int), 1, file);

  // always write out as a double
  if (sizeof(float) != sizeof(double)) {
    double *dataDouble = new double[_totalCells];
    for (int x = 0; x < _totalCells; x++)
      dataDouble[x] = _data[x];

    fwrite((void *)dataDouble, sizeof(double), _totalCells, file);
    delete[] dataDouble;
    fclose(file);
  } else
    fwrite((void *)_data, sizeof(float), _totalCells, file);
  fclose(file);
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
void FIELD_2D::read(string filename)
{
  FILE *file;
  file = fopen(filename.c_str(), "rb");
  if (file == NULL) {
    cout << __FILE__ << " " << __FUNCTION__ << " " << __LINE__ << " : " << endl;
    cout << " FIELD_2D read failed! " << endl;
    cout << " Could not open file " << filename.c_str() << endl;
    exit(0);
  }

  // read dimensions
  fread((void *)&_xRes, sizeof(int), 1, file);
  fread((void *)&_yRes, sizeof(int), 1, file);
  _totalCells = _xRes * _yRes;
  if (_data)
    delete[] _data;
  _data = new float[_totalCells];

  // always read in as a double
  if (sizeof(float) != sizeof(double)) {
    double *dataDouble = new double[_totalCells];
    fread((void *)dataDouble, sizeof(double), _totalCells, file);

    for (int x = 0; x < _totalCells; x++)
      _data[x] = dataDouble[x];

    delete[] dataDouble;
  } else
    fread((void *)_data, sizeof(float), _totalCells, file);
  fclose(file);
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
void FIELD_2D::writePPM(string filename)
{
  FILE *fp;
  unsigned char *pixels = new unsigned char[3 * _totalCells];

  for (int x = 0; x < _totalCells; x++) {
    pixels[3 * x] = 255 * _data[x];
    pixels[3 * x + 1] = 255 * _data[x];
    pixels[3 * x + 2] = 255 * _data[x];
  }

  fp = fopen(filename.c_str(), "wb");
  fprintf(fp, "P6\n%d %d\n255\n", _xRes, _yRes);
  fwrite(pixels, 1, _totalCells * 3, fp);
  fclose(fp);
  delete[] pixels;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
void FIELD_2D::writeMatlab(string filename, string variableName) const
{
  FILE *file;
  file = fopen(filename.c_str(), "w");
  fprintf(file, "%s = [", variableName.c_str());
  for (int y = 0; y < _yRes; y++) {
    for (int x = 0; x < _xRes; x++)
      fprintf(file, "%f ", (*this)(x, y));
    fprintf(file, "; ");
  }
  fprintf(file, "];\n");

  fclose(file);
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
void FIELD_2D::normalize()
{
  float maxFound = 0.0;
  float minFound = _data[0];
  for (int x = 0; x < _totalCells; x++) {
    maxFound = (_data[x] > maxFound) ? _data[x] : maxFound;
    minFound = (_data[x] < minFound) ? _data[x] : minFound;
  }

  float range = 1.0 / (maxFound - minFound);
  for (int x = 0; x < _totalCells; x++)
    _data[x] = (_data[x] - minFound) * range;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D &FIELD_2D::abs()
{
  for (int x = 0; x < _totalCells; x++)
    _data[x] = fabs(_data[x]);

  return *this;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
void FIELD_2D::resizeAndWipe(int xRes, int yRes)
{
  if (_xRes == xRes && _yRes == yRes) {
    clear();
    return;
  }

  if (_data)
    delete[] _data;

  _xRes = xRes;
  _yRes = yRes;
  _totalCells = _xRes * _yRes;

  _data = new float[_xRes * _yRes];
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D &FIELD_2D::operator=(const float &alpha)
{
  for (int x = 0; x < _totalCells; x++)
    _data[x] = alpha;

  return *this;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D &FIELD_2D::operator*=(const float &alpha)
{
  for (int x = 0; x < _totalCells; x++)
    _data[x] *= alpha;

  return *this;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D &FIELD_2D::operator/=(const float &alpha)
{
  for (int x = 0; x < _totalCells; x++)
    _data[x] /= alpha;

  return *this;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D &FIELD_2D::operator+=(const float &alpha)
{
  for (int x = 0; x < _totalCells; x++)
    _data[x] += alpha;

  return *this;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D &FIELD_2D::operator-=(const FIELD_2D &input)
{
  assert(input.xRes() == _xRes);
  assert(input.yRes() == _yRes);
  for (int x = 0; x < _totalCells; x++)
    _data[x] -= input[x];

  return *this;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D &FIELD_2D::operator+=(const FIELD_2D &input)
{
  assert(input.xRes() == _xRes);
  assert(input.yRes() == _yRes);
  for (int x = 0; x < _totalCells; x++)
    _data[x] += input[x];

  return *this;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D &FIELD_2D::operator*=(const FIELD_2D &input)
{
  assert(input.xRes() == _xRes);
  assert(input.yRes() == _yRes);

  for (int x = 0; x < _totalCells; x++)
    _data[x] *= input[x];

  return *this;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D &FIELD_2D::operator/=(const FIELD_2D &input)
{
  assert(input.xRes() == _xRes);
  assert(input.yRes() == _yRes);

  for (int x = 0; x < _totalCells; x++)
    if (fabs(input[x]) > 1e-6)
      _data[x] /= input[x];
    else
      _data[x] = 0;

  return *this;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D operator*(const FIELD_2D &A, const float alpha)
{
  FIELD_2D final(A);
  final *= alpha;
  return final;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D operator/(const FIELD_2D &A, const float alpha)
{
  FIELD_2D final(A);
  final /= alpha;
  return final;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D operator+(const FIELD_2D &A, const FIELD_2D &B)
{
  FIELD_2D final(A);
  final += B;
  return final;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D operator-(const FIELD_2D &A, const FIELD_2D &B)
{
  FIELD_2D final(A);
  final -= B;
  return final;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D operator+(const FIELD_2D &A, const float alpha)
{
  FIELD_2D final(A);
  final += alpha;
  return final;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D operator*(const float alpha, const FIELD_2D &A)
{
  return A * alpha;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D operator+(const float alpha, const FIELD_2D &A)
{
  return A + alpha;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
FIELD_2D &FIELD_2D::operator=(const FIELD_2D &A)
{
  resizeAndWipe(A.xRes(), A.yRes());

  for (int x = 0; x < _totalCells; x++)
    _data[x] = A[x];

  return *this;
}

///////////////////////////////////////////////////////////////////////
// sum of all entries
///////////////////////////////////////////////////////////////////////
float FIELD_2D::sum()
{
  float total = 0;
  for (int x = 0; x < _totalCells; x++)
    total += _data[x];

  return total;
}

///////////////////////////////////////////////////////////////////////
// take the log
///////////////////////////////////////////////////////////////////////
void FIELD_2D::log(float base)
{
  float scale = 1.0 / std::log(base);
  for (int x = 0; x < _totalCells; x++)
    _data[x] = std::log(_data[x]) * scale;
}

///////////////////////////////////////////////////////////////////////
// get the min of the field
///////////////////////////////////////////////////////////////////////
float FIELD_2D::min()
{
  assert(_xRes > 0);
  assert(_yRes > 0);
  float final = _data[0];

  for (int i = 0; i < _xRes * _yRes; i++)
    final = (_data[i] < final) ? _data[i] : final;

  return final;
}

///////////////////////////////////////////////////////////////////////
// get the max of the field
///////////////////////////////////////////////////////////////////////
float FIELD_2D::max()
{
  assert(_xRes > 0);
  assert(_yRes > 0);
  float final = _data[0];

  for (int i = 0; i < _xRes * _yRes; i++)
    final = (_data[i] > final) ? _data[i] : final;

  return final;
}

///////////////////////////////////////////////////////////////////////
// set to a checkboard for debugging
///////////////////////////////////////////////////////////////////////
void FIELD_2D::setToCheckerboard(int xChecks, int yChecks)
{
  for (int x = 0; x < _xRes; x++)
    for (int y = 0; y < _yRes; y++) {
      int xMod = (x / (_xRes / xChecks)) % 2;
      int yMod = (y / (_yRes / yChecks)) % 2;

      if ((xMod && yMod) || (!xMod && !yMod))
        (*this)(x, y) = 1;
    }
}
