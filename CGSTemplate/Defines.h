#pragma once
#include <iostream>
#include <vector>
#define RASTER_WIDTH 800
#define RASTER_HEIGHT 600
#define NUM_PIXELS (RASTER_HEIGHT * RASTER_WIDTH)
#define SWAP_BGRA_TO_ARGB(color) ( ((color & 0xFF000000) >> 24) | ((color & 0x00FF0000) >> 8) | ((color & 0x0000FF00) << 8) | ((color & 0x000000FF) << 24) )
unsigned int SCREEN_ARRAY[NUM_PIXELS];
float DEPTH_ARRAY[NUM_PIXELS];

struct vec3
{
	union
	{
		struct
		{
			float F[3];
		};
		struct
		{
			float x, y, z;
		};
	};
};

struct vec4
{
	union
	{
		struct
		{
			float F[4];
		};
		struct
		{
			float x, y, z, w;
		};
	};
};

struct vertex
{
	vec4 pos;
	vec3 pos2;
	unsigned int color;
	float u;
	float v;

	vertex() {};
	vertex(vec4 pos, unsigned int color, float u = 0.0f, float v = 0.0f)
	{
		this->pos = pos;
		this->color = color;
		this->u = u;
		this->v = v;
	}
};


struct matrix3x3
{
	union
	{
		struct
		{
			float m[3][3];
		};
		struct
		{
			float xx;
			float xy;
			float xz;
			float yx;
			float yy;
			float yz;
			float zx;
			float zy;
			float zz;
			float wx;
			float wy;
			float wz;
		};
		struct
		{
			vec4 axisX, axisY, axisZ;
		};
	};
};

struct matrix4x4
{
	union
	{
		struct
		{
			float m[4][4];
		};
		struct
		{
			float xx;
			float xy;
			float xz;
			float xw;
			float yx;
			float yy;
			float yz;
			float yw;
			float zx;
			float zy;
			float zz;
			float zw;
			float wx;
			float wy;
			float wz;
			float ww;
		};
		struct
		{
			vec4 axisX, axisY, axisZ, axisW;
		};
	};
};

struct Pixel
{
	int x, y;
	unsigned int color;
};

struct Triangle
{
	float b, y, a;
};
