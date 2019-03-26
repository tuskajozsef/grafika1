//=============================================================================================
// Bezier and Lagrange curve editor
//=============================================================================================
#include "framework.h"

// vertex shader in GLSL
const char * vertexSource = R"(
	#version 330
    precision highp float;

	uniform mat4 MVP;			// Model-View-Projection matrix in row-major format

	layout(location = 0) in vec2 vertexPosition;	// Attrib Array 0

	void main() {
		gl_Position = vec4(vertexPosition.x, vertexPosition.y, 0, 1) * MVP; 		// transform to clipping space
	}
)";

// fragment shader in GLSL
const char * fragmentSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;
	out vec4 fragmentColor;		// output that goes to the raster memory as told by glBindFragDataLocation

	void main() {
		fragmentColor = vec4(color, 1); // extend RGB to RGBA
	}
)";

// 2D camera
struct Camera {
	float wCx, wCy;	// center in world coordinates
	float wWx, wWy;	// width and height in world coordinates
public:
	Camera() {
		Animate(0);
	}

	mat4 V() { // view matrix: translates the center to the origin
		return mat4(1,    0, 0, 0,
			        0,    1, 0, 0,
			        0,    0, 1, 0,
			     -wCx, -wCy, 0, 1);
	}

	mat4 P() { // projection matrix: scales it to be a square of edge length 2
		return mat4(2/wWx,    0, 0, 0,
			        0,    2/wWy, 0, 0,
			        0,        0, 1, 0,
			        0,        0, 0, 1);
	}

	mat4 Vinv() { // inverse view matrix
		return mat4(1,     0, 0, 0,
				    0,     1, 0, 0,
			        0,     0, 1, 0,
			        wCx, wCy, 0, 1);
	}

	mat4 Pinv() { // inverse projection matrix
		return mat4(wWx/2, 0,    0, 0,
			           0, wWy/2, 0, 0,
			           0,  0,    1, 0,
			           0,  0,    0, 1);
	}

	void Animate(float t) {
		wCx = 0; // 10 * cosf(t);
		wCy = 0;
		wWx = 20;
		wWy = 20;
	}
};


Camera camera;	// 2D camera
bool animate = false;
float tCurrent = 0;	// current clock in sec
GPUProgram gpuProgram; // vertex and fragment shaders
const int nTesselatedVertices = 100;

class Curve {
	unsigned int vaoCurve, vboCurve;        
	unsigned int vaoCtrlPoints, vboCtrlPoints;        
	unsigned int vaoConvexHull, vboConvexHull;        
	unsigned int vaoAnimatedObject, vboAnimatedObject;
protected:
	std::vector<vec4> wCtrlPoints;		// coordinates of control points
public:
	Curve() {
		// Curve
		glGenVertexArrays(1, &vaoCurve);
		glBindVertexArray(vaoCurve);

		glGenBuffers(1, &vboCurve); // Generate 1 vertex buffer object
		glBindBuffer(GL_ARRAY_BUFFER, vboCurve);
		// Enable the vertex attribute arrays
		glEnableVertexAttribArray(0);  // attribute array 0
		// Map attribute array 0 to the vertex data of the interleaved vbo
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), NULL); // attribute array, components/attribute, component type, normalize?, stride, offset

		// Control Points
		glGenVertexArrays(1, &vaoCtrlPoints);
		glBindVertexArray(vaoCtrlPoints);

		glGenBuffers(1, &vboCtrlPoints); // Generate 1 vertex buffer object
		glBindBuffer(GL_ARRAY_BUFFER, vboCtrlPoints);
		// Enable the vertex attribute arrays
		glEnableVertexAttribArray(0);  // attribute array 0
		// Map attribute array 0 to the vertex data of the interleaved vbo
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), NULL); // attribute array, components/attribute, component type, normalize?, stride, offset

		// Convex Hull
		glGenVertexArrays(1, &vaoConvexHull);
		glBindVertexArray(vaoConvexHull);

		glGenBuffers(1, &vboConvexHull); // Generate 1 vertex buffer object
		glBindBuffer(GL_ARRAY_BUFFER, vboConvexHull);
		// Enable the vertex attribute arrays
		glEnableVertexAttribArray(0);  // attribute array 0
		// Map attribute array 0 to the vertex data of the interleaved vbo
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), NULL); // attribute array, components/attribute, component type, normalize?, stride, offset

		// Animated Object
		glGenVertexArrays(1, &vaoAnimatedObject);
		glBindVertexArray(vaoAnimatedObject);

		glGenBuffers(1, &vboAnimatedObject); // Generate 1 vertex buffer object
		glBindBuffer(GL_ARRAY_BUFFER, vboAnimatedObject);
		// Enable the vertex attribute arrays
		glEnableVertexAttribArray(0);  // attribute array 0
		// Map attribute array 0 to the vertex data of the interleaved vbo
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), NULL); // attribute array, components/attribute, component type, normalize?, stride, offset

	}

	virtual vec4 r(float t) { return wCtrlPoints[0]; }
	virtual float tStart() { return 0; }
	virtual float tEnd() { return 1; }

	virtual void AddControlPoint(float cX, float cY) {
		vec4 wVertex = vec4(cX, cY, 0, 1) * camera.Pinv() * camera.Vinv();
		wCtrlPoints.push_back(wVertex);
	}

	// Returns the selected control point or -1
	int PickControlPoint(float cX, float cY) {
		vec4 wVertex = vec4(cX, cY, 0, 1) * camera.Pinv() * camera.Vinv();
		for (unsigned int p = 0; p < wCtrlPoints.size(); p++) {
			if (dot(wCtrlPoints[p] - wVertex, wCtrlPoints[p] - wVertex) < 0.1) return p;
		}
		return -1;
	}

	void MoveControlPoint(int p, float cX, float cY) {
		vec4 wVertex = vec4(cX, cY, 0, 1) * camera.Pinv() * camera.Vinv();
		wCtrlPoints[p] = wVertex;
	}

	void Draw() {
		mat4 VPTransform = camera.V() * camera.P();

		VPTransform.SetUniform(gpuProgram.getId(), "MVP");

		int colorLocation = glGetUniformLocation(gpuProgram.getId(), "color");

		if (wCtrlPoints.size() > 2) {	// draw convex hull
			std::vector<vec4> triangleVertex;
			for (unsigned int i = 0; i < wCtrlPoints.size(); i++) {
				for (unsigned int j = i + 1; j < wCtrlPoints.size(); j++) {
					for (unsigned int k = j + 1; k < wCtrlPoints.size(); k++) {
						triangleVertex.push_back(wCtrlPoints[i]);
						triangleVertex.push_back(wCtrlPoints[j]);
						triangleVertex.push_back(wCtrlPoints[k]);
					}
				}
			}
			glBindVertexArray(vaoConvexHull);
			glBindBuffer(GL_ARRAY_BUFFER, vboConvexHull);
			glBufferData(GL_ARRAY_BUFFER, triangleVertex.size() * 4 * sizeof(float), &triangleVertex[0], GL_DYNAMIC_DRAW);
			if (colorLocation >= 0) glUniform3f(colorLocation, 0, 0.5f, 0.5f);
			glDrawArrays(GL_TRIANGLES, 0, triangleVertex.size());
		}

		if (wCtrlPoints.size() > 0) {	// draw control points
			glBindVertexArray(vaoCtrlPoints);
			glBindBuffer(GL_ARRAY_BUFFER, vboCtrlPoints);
			glBufferData(GL_ARRAY_BUFFER, wCtrlPoints.size() * 4 * sizeof(float), &wCtrlPoints[0], GL_DYNAMIC_DRAW);
			if (colorLocation >= 0) glUniform3f(colorLocation, 1, 0, 0);
			glPointSize(10.0f);
			glDrawArrays(GL_POINTS, 0, wCtrlPoints.size());
		}

		if (wCtrlPoints.size() >= 2) {	// draw curve
			std::vector<float> vertexData;
			for (int i = 0; i < nTesselatedVertices; i++) {	// Tessellate
				float tNormalized = (float)i / (nTesselatedVertices - 1);
				float t = tStart() + (tEnd() - tStart()) * tNormalized;
				vec4 wVertex = r(t);
				vertexData.push_back(wVertex.x);
				vertexData.push_back(wVertex.y);
			}
			// copy data to the GPU
			glBindVertexArray(vaoCurve);
			glBindBuffer(GL_ARRAY_BUFFER, vboCurve);
			glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), &vertexData[0], GL_DYNAMIC_DRAW);
			if (colorLocation >= 0) glUniform3f(colorLocation, 1, 1, 0);
			glDrawArrays(GL_LINE_STRIP, 0, nTesselatedVertices);

			if (animate) {
				// animation on curve
				float t = tCurrent;
				while (t > tEnd()) t -= tEnd();
				vec4 currentLocation = r(t);
				// copy data to the GPU
				glBindVertexArray(vaoAnimatedObject);
				glBindBuffer(GL_ARRAY_BUFFER, vboAnimatedObject);
				glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(float), &currentLocation, GL_DYNAMIC_DRAW);
				if (colorLocation >= 0) glUniform3f(colorLocation, 1, 1, 1);
				glPointSize(20.0f);
				glDrawArrays(GL_POINTS, 0, 1);	// draw 1 point
			}
		}
	}
};

// Bezier using Bernstein polynomials
class BezierCurve : public Curve {
	float B(int i, float t) {
		int n = wCtrlPoints.size() - 1; // n deg polynomial = n+1 pts!
		float choose = 1;
		for (int j = 1; j <= i; j++) choose *= (float)(n - j + 1) / j;
		return choose * pow(t, i) * pow(1 - t, n - i);
	}
public:
	virtual vec4 r(float t) {
		vec4 wPoint = vec4(0, 0, 0, 0);
		for (unsigned int n = 0; n < wCtrlPoints.size(); n++) wPoint += wCtrlPoints[n] * B(n, t);
		return wPoint;
	}
};

// Lagrange curve
class LagrangeCurve : public Curve {
	std::vector<float> ts;  // knots
	float L(int i, float t) {
		float Li = 1.0f;
		for (unsigned int j = 0; j < wCtrlPoints.size(); j++)
			if (j != i) Li *= (t - ts[j]) / (ts[i] - ts[j]);
		return Li;
	}
public:
	void AddControlPoint(float cX, float cY) {
		ts.push_back((float)wCtrlPoints.size());
		Curve::AddControlPoint(cX, cY);
	}
	float tStart() { return ts[0]; }
	float tEnd() { return ts[wCtrlPoints.size() - 1]; }

	virtual vec4 r(float t) {
		vec4 wPoint = vec4(0, 0, 0, 0);
		for (unsigned int n = 0; n < wCtrlPoints.size(); n++) wPoint += wCtrlPoints[n] * L(n, t);
		return wPoint;
	}
};

// The virtual world: collection of two objects
Curve * curve;

// popup menu event handler
void processMenuEvents(int option) {
	delete curve;
	switch (option) {
	case 0: curve = new Curve();
		break;
	case 1: curve = new LagrangeCurve();
		break;
	case 2: curve = new BezierCurve();
		break;
	}
	glutPostRedisplay();
}

// Initialization, create an OpenGL context
void onInitialization() {
	// create the menu and tell glut that "processMenuEvents" will handle the events: Do not use it in your homework, because it is prohitibed by the portal
	int menu = glutCreateMenu(processMenuEvents);
	//add entries to our menu
	glutAddMenuEntry("Empty", 0);
	glutAddMenuEntry("Lagrange", 1);
	glutAddMenuEntry("Bezier", 2);
	// attach the menu to the right button
	glutAttachMenu(GLUT_MIDDLE_BUTTON);

	glViewport(0, 0, windowWidth, windowHeight);
	glLineWidth(2.0f);

	curve = new Curve();

	// create program for the GPU
	gpuProgram.Create(vertexSource, fragmentSource, "outColor");
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0, 0, 0, 0);							// background color 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen

	curve->Draw();
	glutSwapBuffers();									// exchange the two buffers
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	animate = !animate;			// toggle animation
	glutPostRedisplay();        // redraw
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {

}

int pickedControlPoint = -1;

// Mouse click event
void onMouse(int button, int state, int pX, int pY) {
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {  // GLUT_LEFT_BUTTON / GLUT_RIGHT_BUTTON and GLUT_DOWN / GLUT_UP
		float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
		float cY = 1.0f - 2.0f * pY / windowHeight;
		curve->AddControlPoint(cX, cY);
		glutPostRedisplay();     // redraw
	}
	if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {  // GLUT_LEFT_BUTTON / GLUT_RIGHT_BUTTON and GLUT_DOWN / GLUT_UP
		float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
		float cY = 1.0f - 2.0f * pY / windowHeight;
		pickedControlPoint = curve->PickControlPoint(cX, cY);
		glutPostRedisplay();     // redraw
	}
	if (button == GLUT_RIGHT_BUTTON && state == GLUT_UP) {  // GLUT_LEFT_BUTTON / GLUT_RIGHT_BUTTON and GLUT_DOWN / GLUT_UP
		pickedControlPoint = -1;
	}
}

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {
	float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
	float cY = 1.0f - 2.0f * pY / windowHeight;
	if (pickedControlPoint >= 0) curve->MoveControlPoint(pickedControlPoint, cX, cY);
}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
	long time = glutGet(GLUT_ELAPSED_TIME); // elapsed time since the start of the program
	tCurrent = time / 1000.0f;				// convert msec to sec
	glutPostRedisplay();					// redraw the scene
}
