//=============================================================================================
// Path animation with derivative calculation provided by the Clifford algebra
//=============================================================================================
#include "framework.h"

// vertex shader in GLSL
const char * const vertexSource = R"(
	#version 330
    precision highp float;

    uniform vec2 point, tangent;

	layout(location = 0) in vec2 vertexPosition;	// Attrib Array 0
	const float scale = 0.1f;

	void main() {
		vec2 normal = vec2(-tangent.y, tangent.x);
		vec2 p = (vertexPosition.x * tangent + vertexPosition.y * normal + point) * scale; 
		gl_Position = vec4(p.x, p.y, 0, 1); 		// transform to clipping space
	}
)";

// fragment shader in GLSL
const char * const fragmentSource = R"(
	#version 330
    precision highp float;

	uniform vec4 color;			// uniform color
	out vec4 fragmentColor;		// output that goes to the raster memory as told by glBindFragDataLocation

	void main() {
		fragmentColor = color; 
	}
)";

GPUProgram gpuProgram; // vertex and fragment shaders

struct Clifford {
	float f, d;
	Clifford(float f0 = 0, float d0 = 0) { f = f0, d = d0; }
	Clifford operator+(Clifford r) { return Clifford(f + r.f, d + r.d); }
	Clifford operator-(Clifford r) { return Clifford(f - r.f, d - r.d); }
	Clifford operator*(Clifford r) { return Clifford(f * r.f, f * r.d + d * r.f); }
	Clifford operator/(Clifford r) {
		float l = r.f * r.f;
		return (*this) * Clifford(r.f / l, -r.d / l);
	}
};

Clifford T(float t) { return Clifford(t, 1); }
Clifford Sin(Clifford g) { return Clifford(sin(g.f), cos(g.f) * g.d); }
Clifford Cos(Clifford g) { return Clifford(cos(g.f), -sin(g.f) * g.d); }
Clifford Tan(Clifford g) { return Sin(g) / Cos(g); }
Clifford Log(Clifford g) { return Clifford(logf(g.f), 1 / g.f * g.d); }
Clifford Exp(Clifford g) { return Clifford(expf(g.f), expf(g.f) * g.d); }
Clifford Pow(Clifford g, float n) { return Clifford(powf(g.f, n), n * powf(g.f, n - 1) * g.d); }

class Object {
	unsigned int vao;	// vertex array object id
	int nPoints;
	vec4 color;
public:
	Object(std::vector<vec2>& points, vec4 color0) {
		color = color0;
		nPoints = points.size();

		glGenVertexArrays(1, &vao);	// create 1 vertex array object
		glBindVertexArray(vao);		// make it active
		unsigned int vbo;		// vertex buffer objects
		glGenBuffers(1, &vbo);	// Generate 1 vertex buffer objects
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		// Map Attribute Array 0 to the current bound vertex buffer (vbo[0])

		glBufferData(GL_ARRAY_BUFFER,      // copy to the GPU
			points.size() * sizeof(vec2), // number of the vbo in bytes
			&points[0],		   // address of the data array on the CPU
			GL_STATIC_DRAW);	   // copy to GPU

		glEnableVertexAttribArray(0);
		// Data organization of Attribute Array 0 
		glVertexAttribPointer(0,			// Attribute Array 0
			2, GL_FLOAT,  // components/attribute, component type
			GL_FALSE,		// not in fixed point format, do not normalized
			0, NULL);     // stride and offset: it is tightly packed
	}

	void Draw(vec2 point, vec2 tangent) {
		int location = glGetUniformLocation(gpuProgram.getId(), "point");
		if (location >= 0) glUniform2f(location, point.x, point.y);

		location = glGetUniformLocation(gpuProgram.getId(), "tangent");
		if (location >= 0) glUniform2f(location, tangent.x, tangent.y);

		location = glGetUniformLocation(gpuProgram.getId(), "color");
		if (location >= 0) glUniform4f(location, color.x, color.y, color.z, color.w);

		glBindVertexArray(vao);	// make the vao and its vbos active playing the role of the data source
		glDrawArrays(GL_LINE_LOOP, 0, nPoints);	// draw a single triangle with vertices defined in vao
	}
};

// The virtual world: collection of two objects
Object * vehicle;
Object * path;

// Path of the object and the derivative
void Path(float t, Clifford& x, Clifford& y) {
	x = Sin(T(t)) * (Sin(T(t)) + 3) * 4 / (Tan(Cos(T(t))) + 2);
	y = (Cos(Sin(T(t)) * 8 + 1) * 12 + 2) / (Pow(Sin(T(t)) * Sin(T(t)), 3) + 2);
}

// Initialization, create an OpenGL context
void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);
	glLineWidth(3);

	std::vector<vec2> points;
	points.push_back(vec2(-1, -1));
	points.push_back(vec2(1, 0));
	points.push_back(vec2(-1, 1));
	points.push_back(vec2(0, 0));

	vec4 color = vec4(1, 1, 0, 1);
	vehicle = new Object(points, color);

	std::vector<vec2> pathPoints;
	for (float t = 0; t < 2.0f * M_PI; t += 0.01f) {
		Clifford x, y;
		Path(t, x, y);
		pathPoints.push_back(vec2(x.f, y.f));
	}
	color = vec4(1, 1, 1, 1);
	path = new Object(pathPoints, color);

	// create program for the GPU
	gpuProgram.Create(vertexSource, fragmentSource, "fragmentColor");
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0, 0, 0, 0);							// background color 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen
	path->Draw(vec2(0, 0), vec2(1, 0));

	long time = glutGet(GLUT_ELAPSED_TIME); // elapsed time since the start of the program
	float sec = time / 1000.0f;				// convert msec to sec
	Clifford x, y;
	Path(sec / 2, x, y);
	float tangentLength = sqrtf(x.d * x.d + y.d * y.d);
	vec2 tangent(x.d / tangentLength, y.d / tangentLength);
	vehicle->Draw(vec2(x.f, y.f), tangent);
	glutSwapBuffers();									// exchange the two buffers
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	if (key == 'd') glutPostRedisplay();         // if d, invalidate display, i.e. redraw
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {

}

// Mouse click event
void onMouse(int button, int state, int pX, int pY) {
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {  // GLUT_LEFT_BUTTON / GLUT_RIGHT_BUTTON and GLUT_DOWN / GLUT_UP
	}
}

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {
}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
	glutPostRedisplay();					// redraw the scene
}