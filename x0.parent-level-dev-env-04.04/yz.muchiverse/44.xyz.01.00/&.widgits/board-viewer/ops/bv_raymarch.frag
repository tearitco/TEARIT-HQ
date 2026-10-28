#version 300 es
precision highp float;
precision highp int;
precision highp usampler3D;
precision highp sampler3D;
precision highp sampler2D;
precision highp sampler2DArray;
precision highp usampler2D;
out vec4 o_col;
uniform vec3  u_eye, u_fwd, u_right, u_up;
uniform float u_focal;
uniform vec2  u_res;
uniform vec3  u_wext;
uniform highp usampler3D u_grid;
uniform sampler2D u_leg;
uniform sampler2DArray u_terr;
uniform vec4  u_lbbox[64];
uniform float u_light;
uniform int   u_nl, u_amb_on;
uniform float u_amb;
uniform vec4  u_lpos[8], u_lcol[8];
uniform vec3  u_sky;
uniform int   u_clear;
uniform float u_fog_start, u_fog_end;
uniform int   u_maker;
uniform int   u_zoff;
uniform int   u_rock;
uniform int   u_cut, u_cuty;
uniform vec3  u_cutp;
uniform float u_cutr, u_cuta, u_cutt, u_cutb;
uniform ivec2 u_mk;
uniform sampler2D u_atlas;
uniform highp usampler2D u_cells;
uniform int   u_under;
uniform sampler2D u_para;
uniform int   u_para_on;
uniform vec2  u_para_size;
uniform vec2  u_para_loop;
uniform int   u_nbox;
uniform float u_wire_edge;
uniform float u_wire_thin;
uniform vec3  u_bmin[128];
uniform vec3  u_bmax[128];
uniform vec4  u_bcol[128];
uniform int   u_bmdl[128];
uniform highp sampler3D u_mdl;
uniform ivec3 u_mdim[80];

bool slab(vec3 ro, vec3 rd, vec3 bn, vec3 bx, out float t, out int face) {
  float tmin = -1e30, tmax = 1e30; face = -1;
  for (int i = 0; i < 3; i++) {
    float o = ro[i], d = rd[i];
    if (abs(d) < 1e-9) { if (o < bn[i] || o > bx[i]) return false; }
    else {
      float t0 = (bn[i]-o)/d, t1 = (bx[i]-o)/d; int f0 = i*2;
      if (t0 > t1) { float tt=t0; t0=t1; t1=tt; f0 = i*2+1; }
      if (t0 > tmin) { tmin = t0; face = f0; }
      if (t1 < tmax) tmax = t1;
      if (tmin > tmax) return false;
    }
  }
  if (tmax < 0.0) return false;
  t = tmin < 0.0 ? 0.0 : tmin;
  return true;
}

bool solid_at(ivec3 c) {
  if (c.x < 0 || c.y < 0 || c.z < 0 || c.x >= int(u_wext.x) || c.y >= int(u_wext.y) || c.z >= int(u_wext.z)) return false;
  uint g = texelFetch(u_grid, ivec3(c.x, c.z, c.y), 0).r;
  return int(texelFetch(u_leg, ivec2(int(g), 0), 0).a * 255.0 + 0.5) > 0;
}

// The see-through face: a cell is blended when it is in a WALL column (the map's own extrusion: the column reaches
// u_cutb levels above the character's feet, so a door, a wall and a tall pillar count and a crate or a table does not),
// at or above the feet, in front of the character toward the camera (half-width u_cutr, out to u_cutt cells, never past
// the eye); or when it is roof, 3 levels above the feet, over the room.
bool is_ghost(ivec3 c) {
  if (u_cut == 0 || c.y < u_cuty) return false;
  vec2 h = vec2(float(c.x) + 0.5 - u_cutp.x, float(c.z) + 0.5 - u_cutp.z);
  vec2 e = u_eye.xz - u_cutp.xz;
  float el = length(e);
  if (el > 0.5) {
    vec2 d = e / el;
    float t = dot(h, d), s = dot(h, vec2(-d.y, d.x));
    if (abs(s) < u_cutr && t > 0.5 && t < min(u_cutt, el + 0.5) && (u_cutb <= 0.0 || solid_at(ivec3(c.x, u_cuty + int(u_cutb), c.z)))) return true;
  }
  return float(c.y) >= float(u_cuty) + 3.0 && abs(h.x) < u_cutr && abs(h.y) < u_cutr;
}

bool on_edge(vec3 hp, vec3 bn, vec3 bx, float e) {
  vec3 q = min(abs(hp - bn), abs(hp - bx));
  return (int(q.x < e) + int(q.y < e) + int(q.z < e)) >= 2;
}

void main() {
  float a = (u_res.x * 0.5 - gl_FragCoord.x) / u_focal;
  float b = (u_res.y * 0.5 - gl_FragCoord.y) / u_focal;
  vec3 rd = normalize(u_fwd + a * u_right + b * u_up);
  vec3 ro = u_eye;

  float bestT = 1e30;
  vec3  col   = u_sky;
  bool  hit = false, self_lit = false;
  int   face = -1;
  vec3  gacc = vec3(0.0);
  float gA = 0.0;

  for (int i = 0; i < u_nbox; i++) {
    float t; int f;
    if (!slab(ro, rd, u_bmin[i], u_bmax[i], t, f) || t >= bestT) continue;
    int raw = u_bmdl[i];
    bool swp = raw >= 128;
    int mdl = swp ? raw - 128 : raw;
    if (mdl < 0) {
      bool wire = (u_bcol[i].a > 0.12 && u_bcol[i].a < 0.4);
      float we = (u_bcol[i].a < 0.2) ? u_wire_thin : u_wire_edge;
      if (wire && !on_edge(ro + rd * t, u_bmin[i], u_bmax[i], we)) continue;
      bestT = t; col = u_bcol[i].rgb; hit = true;
      self_lit = (u_bcol[i].a < 0.5); face = f;
      continue;
    }
    /* raymarch the dense model grid inside the box (fixed sub-cell step) */
    vec3 bsz = max(u_bmax[i] - u_bmin[i], vec3(1e-4));
    ivec3 md0 = u_mdim[mdl];
    ivec3 md = swp ? md0.zyx : md0;
    vec3  cellw = bsz / vec3(md);
    float mn = min(min(cellw.x, cellw.y), cellw.z);
    float stepT = 0.5 * mn / max(length(rd), 1e-4);
    float mt = max(t, 0.0) + stepT * 0.5;
    for (int k = 0; k < 220; k++) {
      vec3 wp = ro + rd * mt;
      vec3 lf = (wp - u_bmin[i]) / bsz;
      if (lf.x < -0.02 || lf.y < -0.02 || lf.z < -0.02 ||
          lf.x > 1.02 || lf.y > 1.02 || lf.z > 1.02) break;
      ivec3 li0 = clamp(ivec3(lf * vec3(md)), ivec3(0), md - 1);
      ivec3 li = swp ? li0.zyx : li0;
      vec4 mv = texelFetch(u_mdl, ivec3(li.x, li.y, mdl * 16 + li.z), 0);
      if (mv.a > 0.5) {
        if (mt < bestT) { bestT = mt; col = mv.rgb; hit = true; self_lit = (u_bcol[i].a < 0.5); face = f; }
        break;
      }
      mt += stepT;
    }
  }

  float bt; int bf;
  if (slab(ro, rd, vec3(-1e-3), u_wext + vec3(1e-3), bt, bf)) {
    float tcur = max(bt, 0.0);
    vec3  p = ro + rd * (tcur + 1e-4);
    ivec3 c = clamp(ivec3(floor(p)), ivec3(0), ivec3(u_wext) - 1);
    ivec3 st = ivec3(sign(rd));
    vec3  tdelta = abs(1.0 / rd);
    vec3  tmax;
    for (int i = 0; i < 3; i++) {
      float nb = (st[i] > 0) ? float(c[i] + 1) : float(c[i]);
      tmax[i] = (abs(rd[i]) < 1e-9) ? 1e30 : (nb - ro[i]) / rd[i];
    }
    int ax = bf / 2;
    for (int it = 0; it < 96; it++) {
      if (u_fog_end > u_fog_start && tcur > u_fog_end) break;
      if (c.x < 0 || c.y < 0 || c.z < 0 ||
          c.x >= int(u_wext.x) || c.y >= int(u_wext.y) || c.z >= int(u_wext.z)) break;
      uint g = texelFetch(u_grid, ivec3(c.x, c.z, c.y), 0).r;
      vec4 L = texelFetch(u_leg, ivec2(int(g), 0), 0);
      int lay = int(L.a * 255.0 + 0.5) - 1;
      if (lay >= 0) {
        bool ghosted = false;
        /* real per-cell slab test for the true hit face + t (the DDA
         * step axis is wrong for the first cell - it's the board-bbox
         * entry face, not this voxel's). Matches the CPU DDA. */
        float ct; int cf;
        if (slab(ro, rd, vec3(c), vec3(c) + 1.0, ct, cf) && ct < bestT) {
          float b0 = bestT; bool h0 = hit, s0 = self_lit; int f0 = face; vec3 c0 = col;
          bestT = ct; hit = true; self_lit = false; face = cf;
          vec3 wp = ro + rd * ct;
          vec3 fr = wp - vec3(c);
          vec2 uv;
          if (cf == 2 || cf == 3)      uv = vec2(fr.x, fr.z);
          else if (cf == 4 || cf == 5) uv = vec2(fr.x, 1.0 - fr.y);
          else                         uv = vec2(fr.z, 1.0 - fr.y);
          vec4 bb = u_lbbox[lay];
          if (bb.z <= bb.x) bb = vec4(0.0, 0.0, 1.0, 1.0);
          vec2 tuv = bb.xy + clamp(uv, 0.0, 0.999) * (bb.zw - bb.xy);
          ivec2 ti = clamp(ivec2(tuv * 16.0), ivec2(0), ivec2(15));
          vec4 tx = texelFetch(u_terr, ivec3(ti, lay), 0);
          col = (tx.a > 0.04) ? tx.rgb : L.rgb;
          if (u_maker != 0) {
            uvec2 sl = texelFetch(u_cells, ivec2(c.x, c.z), 0).rg;
            /* A cell with both a top tile and a different side tile is furniture:
               the top face is the picture, the sides are the extruded edge.
               Walls keep the old split (floor on y=0, wall art above). */
            uint slot = (c.y <= u_zoff) ? sl.x : sl.y;
            if (c.y < u_zoff && u_rock > 0 && cf != 3) slot = uint(u_rock);
            if (sl.x != 0u && sl.y != 0u && sl.x != sl.y) slot = (cf == 3) ? sl.x : sl.y;
            if (u_under != 0 && c.y < u_zoff - 1) { uint us = texelFetch(u_cells, ivec2(c.x, c.z + int(u_wext.z)), 0).r; if (us != 0u) slot = us; }
            ivec2 ap = ivec2(int(slot) % u_mk.x, int(slot) / u_mk.x) * u_mk.y + ivec2(clamp(uv, 0.0, 0.999) * float(u_mk.y));
            vec4 at = texelFetch(u_atlas, ap, 0);
            if ((slot == 0u || at.a <= 0.5) && c.y > u_zoff) {
              slot = sl.x;
              ap = ivec2(int(slot) % u_mk.x, int(slot) / u_mk.x) * u_mk.y + ivec2(clamp(uv, 0.0, 0.999) * float(u_mk.y));
              at = texelFetch(u_atlas, ap, 0);
            }
            col = (slot != 0u && at.a > 0.5) ? at.rgb : vec3(0.10, 0.10, 0.12);
            if (c.y < u_zoff && u_rock <= 0 && cf != 3) col *= vec3(0.62, 0.50, 0.42);
          }
          if (is_ghost(c) && gA < 0.97) {
            vec3 gc = col * (cf != 3 ? 0.75 : 1.0) * vec3(u_amb_on != 0 ? u_amb : u_light);
            if (gA < 0.001) { gacc = u_cuta * gc; gA = u_cuta; }
            bestT = b0; hit = h0; self_lit = s0; face = f0; col = c0; ghosted = true;
          }
        }
        if (!ghosted) break;
      }
      if (tmax.x < tmax.y) { if (tmax.x < tmax.z) ax = 0; else ax = 2; }
      else               { if (tmax.y < tmax.z) ax = 1; else ax = 2; }
      tcur = tmax[ax];
      c[ax]   += st[ax];
      tmax[ax] += tdelta[ax];
    }
  }

  /* Missed rays show the whole parallax picture. A screen-pixel
   * fetch only ever showed two rows, which wrapped as stripes.
   * u and v come from the ray that was already cast. This does not
   * write the camera. */
  if (!hit && u_para_on != 0 && u_clear == 0) {
    vec2 sz = max(u_para_size, vec2(1.0));
    float u = atan(rd.x, rd.z) / 6.2831853 + 0.5;
    float v = clamp(0.5 - rd.y * 0.5, 0.0, 0.999);
    if (u_para_loop.x < 0.5) u = clamp(u, 0.0, 0.999);
    else u = fract(u);
    ivec2 p = ivec2(clamp(vec2(u, v) * sz, vec2(0.0), sz - 1.0));
    col = texelFetch(u_para, p, 0).rgb;
  }
  if (hit && !self_lit) {
    if (face != 3) col *= 0.75;
    vec3 L = vec3(u_amb_on != 0 ? u_amb : u_light);
    if (u_nl > 0) {
      vec3 hp = ro + rd * bestT;
      for (int i = 0; i < u_nl; i++) { float f = clamp(1.0 - distance(hp, u_lpos[i].xyz) / max(u_lpos[i].w, 1e-3), 0.0, 1.0); L += u_lcol[i].rgb * (u_lcol[i].a * f * f); }
    }
    col *= L;
  }
  if (gA > 0.0) col = gacc + (1.0 - gA) * col;
  float fa = (!hit && u_clear != 0) ? 0.0 : 1.0;
  if (gA > 0.0) fa = max(fa, gA);
  if (hit && u_fog_end > u_fog_start) {
    float f = smoothstep(u_fog_start, u_fog_end, bestT);
    if (u_clear != 0) fa = 1.0 - f;
    else col = mix(col, u_sky, f);
  }
  o_col = vec4(clamp(col, 0.0, 1.0), fa);
}
