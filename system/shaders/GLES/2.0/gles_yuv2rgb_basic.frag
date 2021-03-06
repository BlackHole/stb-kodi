/*
 *      Copyright (C) 2010-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#version 100

precision mediump float;

uniform sampler2D m_sampY;
uniform sampler2D m_sampU;
uniform sampler2D m_sampV;
varying vec2 m_cordY;
varying vec2 m_cordU;
varying vec2 m_cordV;
uniform vec2 m_step;
uniform mat4 m_yuvmat;
uniform mat3 m_primMat;
uniform float m_gammaDstInv;
uniform float m_gammaSrc;
uniform float m_toneP1;
uniform vec3 m_coefsDst;
uniform float m_alpha;

void main()
{
  vec4 rgb;
  vec4 yuv;

#if defined(XBMC_YV12) || defined(XBMC_NV12)

  yuv = vec4(texture2D(m_sampY, m_cordY).r,
             texture2D(m_sampU, m_cordU).g,
             texture2D(m_sampV, m_cordV).a,
             1.0);

#elif defined(XBMC_NV12_RRG)

  yuv = vec4(texture2D(m_sampY, m_cordY).r,
             texture2D(m_sampU, m_cordU).r,
             texture2D(m_sampV, m_cordV).g,
             1.0);

#endif

  rgb = m_yuvmat * yuv;
  rgb.a = m_alpha;

#if defined(XBMC_COL_CONVERSION)
  vec4 tmp;
  vec4 tmp2;
  tmp.rgb = max(vec3(0), rgb.rgb);
  float a = 0.1854;
  float b = 0.8516;
  float c = -0.0357;
  float ai = 0.06981272;
  float bi = -0.2543179;
  float ci = 1.18681715;
#if defined(XBMC_COL_GAMMA_2_4)
  a = 0.3856;
  b = 0.6641;
  c = -0.2034;
  ai = 0.14334285;
  bi = -0.50023754;
  ci = 1.36175809;
#endif
  tmp2.rgb = tmp.rgb * tmp.rgb;
  rgb.rgb = tmp2.rgb * tmp.rgb * a + tmp2.rgb * b + c * tmp.rgb;
  rgb.rgb = max(vec3(0), m_primMat * rgb.rgb);
  rgb.rgb = ai * rgb.rgb*rgb.rgb + bi * rgb.rgb + ci * sqrt(rgb.rgb);

#if defined(XBMC_TONE_MAPPING)
  float luma = dot(rgb.rgb, m_coefsDst);
  rgb.rgb *= tonemap(luma) / luma;
#endif

#endif

  gl_FragColor = rgb;
}

