#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef _DEBUG
#include <vld.h> //leak detection 
#endif

#include <cmath>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cfloat>

#ifdef _WIN32
#include <windows.h> //Windows exception handling
#endif

#include "clipper.hpp"          //http://sourceforge.net/projects/polyclipping/ (clipper)

#include <boost/foreach.hpp>    //http://www.boost.org/  
#include <boost/geometry.hpp>   //(boost geometry aka ggl)
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/multi/geometries/multi_polygon.hpp>

//---------------------------------------------------------------------------

using namespace std;
using namespace ClipperLib;
using namespace boost::geometry;

const double INT_SCALE = 1000;

struct Point
{
  double  x;
  double  y;
  Point(double _x = 0.0, double _y = 0.0): x(_x), y(_y) {}; 
};

typedef std::vector< Point > Poly;
typedef std::vector< Poly > Polys;

typedef model::d2::point_xy<double> ggl_point;
typedef model::ring< ggl_point > ggl_ring;
typedef model::polygon< ggl_point > ggl_polygon;
typedef model::multi_polygon< ggl_polygon > ggl_polygons;

enum bool_type {Intersection, Union, Difference, Xor};
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

string ColorToHtml(unsigned clr)
{
  stringstream ss;
  ss << '#' << hex << std::setfill('0') << setw(6) << (clr & 0xFFFFFF);
  return ss.str();
}
//------------------------------------------------------------------------------

float GetAlphaAsFrac(unsigned clr)
{
  return ((float)(clr >> 24) / 255);
}
//------------------------------------------------------------------------------

inline long64 Round(double val)
{
  if ((val < 0)) return (long64)(val - 0.5); else return (long64)(val + 0.5);
}
//------------------------------------------------------------------------------

double Area(Poly& poly)
{
  int highI = poly.size() -1;
  if (highI < 2) return 0;
  double a;
  a = (poly[highI].x + poly[0].x) * (poly[0].y - poly[highI].y);
  for (int i = 1; i <= highI; ++i)
    a += (poly[i - 1].x + poly[i].x) * (poly[i].y - poly[i - 1].y);
  return a / 2;
}
//------------------------------------------------------------------------------

void Ellipse(Poly& p, double cx, double cy, double rx, double ry, int steps = 0)
{ 
  const double pi = 3.1415926535898, tolerance = 0.125;
  double r = (rx + ry)/2;
  if (steps <= 0) steps = int(pi / (acos(1 - tolerance / r)));
  if (steps < 3) steps = 3;
  p.resize(steps);
  double sn = sin(2 * pi / steps);
  double cs = cos(2 * pi / steps);
  Point pt = Point(1.0, 0);
  for (int i = 0; i < steps; i++)
  {
    p[i].x = cx + pt.x * rx;
    p[i].y = cy + pt.y * ry;
    //cross & dot products avoids repeat calls to sin() & cos() 
    pt = Point(pt.x * cs - sn * pt.y, pt.x * sn + pt.y * cs); 
  }
}
//------------------------------------------------------------------------------

void Star(Poly& p, double cx, double cy, double radius1, double radius2, 
  int count = 5, double offset_angle = 0.0)
{ 
  const double pi = 3.1415926535898;
  if (count <= 5) count = 5;
  count *= 2;
  p.resize(count);
  double sn = sin(2 * pi / count);
  double cs = cos(2 * pi / count);
  Point delta(1.0, 0);
  if (offset_angle != 0.0)
  {
    delta.x = cos(offset_angle / count);
    delta.y = sin(offset_angle / count);
  } 
  for (int i = 0; i < count; i++)
  {
    double r = (i % 2 == 0 ? radius1: radius2);
    p[i].x = cx + delta.x * r;
    p[i].y = cy + delta.y * r;
    //cross & dot products faster than repeat calls to sin() & cos() ...
    delta = Point(delta.x * cs - sn * delta.y, delta.x * sn + delta.y * cs); 
  }
}
//------------------------------------------------------------------------------

void MakeRandomPoly(Poly& poly, int width, int height, unsigned vertCnt)
{
  //stress_factor > 1 causes more frequent complex intersections with GPC crashes
  const int stress_factor = 10; 
  //make vertices a multiple of stress_factor ...
  poly.resize(vertCnt);
  int w = width / stress_factor, h = height / stress_factor;
  for (unsigned i = 0; i < vertCnt; ++i)
  {
    poly[i].x = (rand() % w) * stress_factor;
    poly[i].y = (rand() % h) * stress_factor;
  }
}
//---------------------------------------------------------------------------

bool LoadFromWlrFile(char *filename, Polys &pp) {
  FILE *f = fopen(filename, "r");
  if (!f) return false;
  int polyCnt, vertCnt, i, j;
  int X, Y;
  pp.clear();
  if ((fscanf(f, "%d", &i) == 1) &&
    (fscanf(f, "%d", &polyCnt) == 1) && (i == 1) && (polyCnt > 0)){
    pp.resize(polyCnt);
    for (i = 0; i < polyCnt; i++) {
      if (fscanf(f, "%d", &vertCnt) != 1) break;
      pp[i].resize(vertCnt);
      for (j = 0; j < vertCnt; j++){
        fscanf(f, "%d,%d", &X, &Y);
        pp[i][j].x = X; pp[i][j].y = Y;
      }
    }
  }
  fclose(f);
  return true;
}
//---------------------------------------------------------------------------

void LoadClipper(Polygons &p, Polys& polys)
{
  p.resize(polys.size());
  for (size_t i = 0; i < polys.size(); i++)
  {
    p[i].resize(polys[i].size());
    for (size_t j = 0; j < polys[i].size(); j++)
    {
      p[i][j].X = Round(polys[i][j].x *INT_SCALE);
      p[i][j].Y = Round(polys[i][j].y *INT_SCALE);
    };
  }
}
//---------------------------------------------------------------------------

void UnloadClipper(Polys& polys, Polygons &p)
{
  polys.resize(p.size());
  for (size_t i = 0; i < p.size(); i++)
  {
    polys[i].resize(p[i].size());
    for (size_t j = 0; j < p[i].size(); j++)
    {
      polys[i][j].x = (double)p[i][j].X /INT_SCALE;
      polys[i][j].y = (double)p[i][j].Y /INT_SCALE;
    };
  }
}
//---------------------------------------------------------------------------

int CountVertices(Polys& p)
{
  int cnt = 0;
  for (size_t i = 0; i < p.size(); i++)
    for (size_t j = 0; j < p[i].size(); j++)
      cnt++;
  return cnt;
}
//---------------------------------------------------------------------------

double DoClipper(Polys& subj, Polys& clip, Polys& solution, bool_type bt = Intersection)
{
  Polygons clipper_subj, clipper_clip, clipper_solution;
  LoadClipper(clipper_subj, subj);
  LoadClipper(clipper_clip, clip);
  double elapsed = 0;

  ClipType op = ctIntersection;
  switch (bt)
  {
    case Union: op = ctUnion; break;
    case Difference: op = ctDifference; break;
    case Xor: op = ctXor; break;
    default: op = ctIntersection; break;
  }

  clock_t t = clock();
  Clipper cp;
  cp.AddPolygons(clipper_subj, ptSubject);
  cp.AddPolygons(clipper_clip, ptClip);
  if (cp.Execute(op, clipper_solution, pftEvenOdd, pftEvenOdd))
    elapsed = double(clock() - t) *1000 / CLOCKS_PER_SEC;

  UnloadClipper(solution, clipper_solution);
  return elapsed;
}
//---------------------------------------------------------------------------
int MakeShrinkingEllipses(Polys& p, int count, Point& center, const Point& radius, double step)
{
  p.resize(count);
  int result = 0;
  for (int i = 0; i < count; i++)
  {
    if (i*step +1 >= radius.x || i*step +1 >= radius.y)
    {
      p.resize(i);
      break;
    }
    Ellipse(p[i], center.x ,center.y, radius.x - (i*step), radius.y - (i*step), 0);
    result += p[i].size();
    if(i % 2 != 0) reverse(p[i].begin(), p[i].end());
  }
  return result;
}
//---------------------------------------------------------------------------

int MakeShrinkingRects(Polys& p, int count, Point& center, Point& radius, double step)
{
  p.resize(count);
  for(int i = 0; i < count; i++)
  {
    if (i*step +1 >= radius.x || i*step +1 >= radius.y) break;
    p[i].resize(4);
    p[i][0] = Point(center.x - radius.x + (i*step), center.y - radius.y + (i*step));
    p[i][1] = Point(center.x + radius.x - (i*step), center.y - radius.y + (i*step));
    p[i][2] = Point(center.x + radius.x - (i*step), center.y + radius.y - (i*step));
    p[i][3] = Point(center.x - radius.x + (i*step), center.y + radius.y - (i*step));
    if(i % 2 != 0) reverse(p[i].begin(), p[i].end());
  }
  return count * 4;
}
//---------------------------------------------------------------------------

int MakeFanBlades(Polys& p, int blade_cnt, const Point& center, Point& radius)
{
  const int inner_rad = 60;
  blade_cnt *= 2;
  if (blade_cnt < 8) blade_cnt = 8;
  if (radius.x < inner_rad +10) radius.x = inner_rad +10;
  if (radius.y < inner_rad +10) radius.y = inner_rad +10;
  p.resize(1);
  Poly &pg = p.back(), inner, outer;
  Ellipse(outer, center.x,center.y, radius.x, radius.y, blade_cnt);
  Ellipse(inner, center.x,center.y, inner_rad, inner_rad, blade_cnt);
  pg.resize(blade_cnt*2);
  for (int i = 0; i +1 < blade_cnt; i+=2)
  {
    pg[i*2]    = inner[i];
    pg[i*2 +1] = outer[i];
    pg[i*2 +2] = outer[i+1];
    pg[i*2 +3] = inner[i+1];
  }
  return blade_cnt *2;
}
//---------------------------------------------------------------------------

//int MakeFanBlades2(Polys& p, int blade_cnt, Point& center, Point& radius)
//{
//  blade_cnt *= 2;
//  if (blade_cnt < 8) blade_cnt = 8;
//  if (radius.x < 40) radius.x = 40;
//  if (radius.y < 40) radius.y = 40;
//  p.resize(1);
//  Poly &pg = p[0], inner, outer;
//  Ellipse(outer, center.x,center.y, radius.x, radius.y, blade_cnt);
//  pg.resize(blade_cnt*3/2);
//  int j = 0;
//  for (int i = 0; i +1 < blade_cnt; i+=2)
//  {
//    pg[j] = outer[i];
//    pg[j+1] = outer[i+1];
//    pg[j+2] = center;
//    j += 3;
//  }
//  return blade_cnt *3/2;
//}
//---------------------------------------------------------------------------

void StarTest()
{
  Polys subj(1), clip(1), sol;
  double elapsed = 0;
  cout << "\nStar Test:\n";

  Point center1 = Point(310,320);
  Star(subj[0], 325, 325, 300, 150, 250, 0.0);
  Star(clip[0], 325, 325, 300, 150, 250, 0.005);

  cout << "No. vertices in subject & clip polygons: " << CountVertices(subj) + CountVertices(clip) << '\n';
  elapsed = DoClipper(subj, clip, sol, Xor);
  cout << "Clipper Time:  " << elapsed << " msecs\n";
  cout << "Test finished. ('st_classic.svg' file created)\n\n";
}
//---------------------------------------------------------------------------

void ClassicTest()
{
  Polys subj, clip, sol;
  double elapsed = 0;

  cout << "\nClassic Test:\n";
  if (!LoadFromWlrFile("s.wlr", subj) || !LoadFromWlrFile("c.wlr", clip))
  {
    cout << "\nUnable to find or load 's.wlr' or 'c.wlr'.\n";
    cout << "Aborting test.\n";
    return;
  }

  cout << "No. vertices in subject & clip polygons: " << CountVertices(subj) + CountVertices(clip) << '\n';
  elapsed = DoClipper(subj, clip, sol);
  cout << "Clipper Time:  " << elapsed << " msecs\n";
  cout << "Test finished. ('st_classic.svg' file created)\n\n";
}
//---------------------------------------------------------------------------

void EllipseAndFanTest()
{
  Polys subj, clip, sol;
  double elapsed = 0;
  cout << "\nEllipses and Fan Test:\n";

  Point center1 = Point(310,320), center2 = Point(410,350);
  MakeShrinkingEllipses(subj, 80, center1, Point(290, 320), 5);
  Point p = Point(340,300);
  MakeFanBlades(clip, 64, center2, p);

  cout << "No. vertices in subject & clip polygons: " << CountVertices(subj) + CountVertices(clip) << '\n';
  elapsed = DoClipper(subj, clip, sol);
  cout << "Clipper Time:  " << elapsed << " msecs\n";
  cout << "Test finished. ('st_ellipse_fan.svg' file created)\n\n";
}
//---------------------------------------------------------------------------

void EllipseAndRectTest()
{
  Polys subj, clip, sol;
  double elapsed = 0;
  cout << "\nEllipses and Rectangles Test:\n";

  Point center1 = Point(310,320), center2 = Point(410,350);
  MakeShrinkingEllipses(subj, 80, center1, Point(290, 320), 5);
  Point r = Point(340, 300);
  MakeShrinkingRects(clip, 80, center2, r, 5);

  cout << "No. vertices in subject & clip polygons: " << CountVertices(subj) + CountVertices(clip) << '\n';
  elapsed = DoClipper(subj, clip, sol);
  cout << "Clipper Time:  " << elapsed << " msecs\n";
  cout << "Test finished. ('st_ellipse_rect.svg' file created)\n\n";
}
//---------------------------------------------------------------------------

void SelfIntersectTest()
{
  const unsigned VERT_COUNT = 100, LOOP_COUNT = 100;
  Polys subj(1), clip(1), sol;
  cout << "\nSelf-intersect Test:\n";
  cout << "Both subject and clip polygons have " << VERT_COUNT << " vertices.\n";
  cout << "This test is repeated " << LOOP_COUNT << " times using randomly generated coordinates ...\n";
  int errors_Clipper = 0, errors_GPC = 0;
  double elapsed_Clipper = 0, elapsed_GPC = 0, elapsed;
  for (unsigned i = 0; i < LOOP_COUNT; ++i)
  {
    MakeRandomPoly(subj[0], 600, 400, VERT_COUNT);
    MakeRandomPoly(clip[0], 600, 400, VERT_COUNT);


    elapsed = DoClipper(subj, clip, sol);
    if (elapsed == 0) errors_Clipper++;
    else elapsed_Clipper += elapsed;

    if (LOOP_COUNT >= 500 && i % 100 == 0) cout << (LOOP_COUNT - i)/100 << "."; //show something's happening
  }
  if (LOOP_COUNT >= 500) cout << "Done\n";
  cout << "Clipper Time:  " << elapsed_Clipper << " msecs. (Failed " << errors_Clipper << " times)\n";
  cout << "Test finished. ('st_complex.svg' file created)\n\n";
}
//---------------------------------------------------------------------------

#ifdef __BORLANDC__
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc, char* argv[])
#endif
{
  srand ((unsigned)time(NULL));

  ClassicTest();
  EllipseAndFanTest();
  EllipseAndRectTest();
  StarTest();

  SelfIntersectTest();

  return 0;
}
//---------------------------------------------------------------------------
