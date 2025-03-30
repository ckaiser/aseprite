static inline int clamp(int value, int min, int max)
{
  if (value < min) return min;
  if (value > max) return max;
  return value;
}

void algo_line_perfect(int x1, int y1, int x2, int y2, void* data, AlgoPixel proc)
{
  bool yaxis;

  // If the height if the line is bigger than the width, we'll iterate
  // over the y-axis.
  if (ABS(y2 - y1) > ABS(x2 - x1)) {
    std::swap(x1, y1);
    std::swap(x2, y2);
    yaxis = true;
  }
  else
    yaxis = false;

  const int w = ABS(x2 - x1) + 1;
  const int h = ABS(y2 - y1) + 1;
  const int dx = SGN(x2 - x1);
  const int dy = SGN(y2 - y1);

  int e = 0;
  int y = y1;

  // Move x2 one extra pixel to the dx direction so we can use
  // operator!=() instead of operator<(). Here I prefer operator!=()
  // instead of swapping x1 with x2 so the error always start from 0
  // in the origin (x1,y1).
  x2 += dx;

  for (int x = x1; x != x2; x += dx) {
    if (yaxis)
      proc(y, x, data);
    else
      proc(x, y, data);

    // The error advances "h/w" per each "x" step. As we're using a
    // integer value for "e", we use "w" as the unit.
    e += h;
    if (e >= w) {
      y += dy;
      e -= w;
    }
  }
}

// Special version of the perfect line algorithm specially done for
// kLineBrushType so the whole line looks continuous without holes.
//
// TOOD in a future we should convert lines into scanlines and render
//      scanlines instead of drawing the brush on each pixel, that
//      would fix all cases
void algo_line_perfect_with_fix_for_line_brush(int x1,
                                               int y1,
                                               int x2,
                                               int y2,
                                               void* data,
                                               AlgoPixel proc)
{
  bool yaxis;

  if (ABS(y2 - y1) > ABS(x2 - x1)) {
    std::swap(x1, y1);
    std::swap(x2, y2);
    yaxis = true;
  }
  else
    yaxis = false;

  const int w = ABS(x2 - x1) + 1;
  const int h = ABS(y2 - y1) + 1;
  const int dx = SGN(x2 - x1);
  const int dy = SGN(y2 - y1);

  int e = 0;
  int y = y1;

  x2 += dx;

  for (int x = x1; x != x2; x += dx) {
    if (yaxis)
      proc(y, x, data);
    else
      proc(x, y, data);

    e += h;
    if (e >= w) {
      y += dy;
      e -= w;
      if (x + dx != x2) {
        if (yaxis)
          proc(y, x, data);
        else
          proc(x, y, data);
      }
    }
  }
}

// Line code based on Alois Zingl work released under the
// MIT license http://members.chello.at/easyfilter/bresenham.html
void algo_line_continuous(int x0, int y0, int x1, int y1, void* data, AlgoPixel proc)
{
  int dx = ABS(x1 - x0), sx = (x0 < x1 ? 1 : -1);
  int dy = -ABS(y1 - y0), sy = (y0 < y1 ? 1 : -1);
  int err = dx + dy, e2; // error value e_xy

  for (;;) {
    proc(x0, y0, data);
    e2 = 2 * err;
    if (e2 >= dy) { // e_xy+e_x > 0
      if (x0 == x1)
        break;
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) { // e_xy+e_y < 0
      if (y0 == y1)
        break;
      err += dx;
      y0 += sy;
    }
  }
}

// Special version of the continuous line algorithm specially done for
// kLineBrushType so the whole line looks continuous without holes.
void algo_line_continuous_with_fix_for_line_brush(int x0,
                                                  int y0,
                                                  int x1,
                                                  int y1,
                                                  void* data,
                                                  AlgoPixel proc)
{
  int dx = ABS(x1 - x0), sx = (x0 < x1 ? 1 : -1);
  int dy = -ABS(y1 - y0), sy = (y0 < y1 ? 1 : -1);
  int err = dx + dy, e2; // error value e_xy
  bool x_changed;

  for (;;) {
    x_changed = false;

    proc(x0, y0, data);
    e2 = 2 * err;
    if (e2 >= dy) { // e_xy+e_x > 0
      if (x0 == x1)
        break;
      err += dy;
      x0 += sx;
      x_changed = true;
    }
    if (e2 <= dx) { // e_xy+e_y < 0
      if (y0 == y1)
        break;
      err += dx;
      if (x_changed)
        proc(x0, y0, data);
      y0 += sy;
    }
  }
}
