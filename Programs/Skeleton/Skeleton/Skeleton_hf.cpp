//=============================================================================================
// Mintaprogram: Zöld háromszög. Ervenyes 2018. osztol.
//
// A beadott program csak ebben a fajlban lehet, a fajl 1 byte-os ASCII karaktereket tartalmazhat, BOM kihuzando.
// Tilos:
// - mast "beincludolni", illetve mas konyvtarat hasznalni
// - faljmuveleteket vegezni a printf-et kiveve
// - Mashonnan atvett programresszleteket forrasmegjeloles nelkul felhasznalni es
// - felesleges programsorokat a beadott programban hagyni!!!!!!! 
// - felesleges kommenteket a beadott programba irni a forrasmegjelolest kommentjeit kiveve
// ---------------------------------------------------------------------------------------------
// A feladatot ANSI C++ nyelvu forditoprogrammal ellenorizzuk, a Visual Studio-hoz kepesti elteresekrol
// es a leggyakoribb hibakrol (pl. ideiglenes objektumot nem lehet referencia tipusnak ertekul adni)
// a hazibeado portal ad egy osszefoglalot.
// ---------------------------------------------------------------------------------------------
// A feladatmegoldasokban csak olyan OpenGL fuggvenyek hasznalhatok, amelyek az oran a feladatkiadasig elhangzottak 
// A keretben nem szereplo GLUT fuggvenyek tiltottak.
//
// NYILATKOZAT
// ---------------------------------------------------------------------------------------------
// Nev    : Tuska Jozsef Csongor
// Neptun : LAU37R
// ---------------------------------------------------------------------------------------------
// ezennel kijelentem, hogy a feladatot magam keszitettem, es ha barmilyen segitseget igenybe vettem vagy
// mas szellemi termeket felhasznaltam, akkor a forrast es az atvett reszt kommentekben egyertelmuen jeloltem.
// A forrasmegjeloles kotelme vonatkozik az eloadas foliakat es a targy oktatoi, illetve a
// grafhazi doktor tanacsait kiveve barmilyen csatornan (szoban, irasban, Interneten, stb.) erkezo minden egyeb
// informaciora (keplet, program, algoritmus, stb.). Kijelentem, hogy a forrasmegjelolessel atvett reszeket is ertem,
// azok helyessegere matematikai bizonyitast tudok adni. Tisztaban vagyok azzal, hogy az atvett reszek nem szamitanak
// a sajat kontribucioba, igy a feladat elfogadasarol a tobbi resz mennyisege es minosege alapjan szuletik dontes.
// Tudomasul veszem, hogy a forrasmegjeloles kotelmenek megsertese eseten a hazifeladatra adhato pontokat
// negativ elojellel szamoljak el es ezzel parhuzamosan eljaras is indul velem szemben.
//=============================================================================================
using namespace std;

#include "framework.h"

// vertex shader in GLSL: It is a Raw string (C++11) since it contains new line characters
const char * const vertexSource = R"(
	#version 330				// Shader 3.3
	precision highp float;		// normal floats, makes no difference on desktop computers

	uniform mat4 MVP;			// uniform variable, the Model-View-Projection transformation matrix
	layout(location = 0) in vec2 vp;	// Varying input: vp = vertex position is expected in attrib array 0

	void main() {
		gl_Position = vec4(vp.x, vp.y, 0, 1) * MVP;		// transform vp from modeling space to normalized device space
	}
)";

// fragment shader in GLSL
const char * const fragmentSource = R"(
	#version 330			// Shader 3.3
	precision highp float;	// normal floats, makes no difference on desktop computers
	
	uniform vec4 color;		// uniform variable, the color of the primitive
	out vec4 outColor;		// computed color of the current pixel

	void main() {
		outColor = color;	// computed color is the color of the primitive
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
		return mat4(1, 0, 0, 0,
			        0, 1, 0, 0,
			        0, 0, 1, 0,
			     -wCx, -wCy, 0, 1);
	}

	mat4 P() { // projection matrix: scales it to be a square of edge length 2
		return mat4(2 / wWx, 0, 0, 0,
			        0, 2 / wWy, 0, 0,
			        0, 0, 1, 0,
			           0, 0, 0, 1);
	}

	mat4 Vinv() { // inverse view matrix
		return mat4(1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			wCx, wCy, 0, 1);
	}

	mat4 Pinv() { // inverse projection matrix
		return mat4(wWx / 2, 0, 0, 0,
			0, wWy / 2, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);
	}

	void Animate(float t) {
		wCx = 1000 * cosf(t);
		wCy = 0;
		wWx = 20;
		wWy = 20;
	}
};


Camera camera;	// 2D camera
bool animate = false;
float tCurrent = 0;	// current clock in sec
GPUProgram gpuProgram; // vertex and fragment shaders
const int nTesselatedVertices = 1000;

class Circle {
	void drawCircle(float r, float x, float y) {
		float i = 0.0f;

		glBegin(GL_TRIANGLE_FAN);

		glVertex2f(x, y); // Center
		for (i = 0.0f; i <= 360; i++)
			glVertex2f(r*cos(M_PI * i / 180.0) + x, r*sin(M_PI * i / 180.0) + y);

		glEnd();
	}
};

class Curve {
	bool animateable = false;
	unsigned int vaoCurve, vboCurve;
	unsigned int vaoCtrlPoints, vboCtrlPoints;
	unsigned int vaoAnimatedObject, vboAnimatedObject;
	unsigned int vbo;

	vec4 color=vec4(1,1,1,1);
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

	void Anim() {
		animateable = !animateable;
	}

	void SetColor(vec4 col) {
		color = col;
	}

	virtual vec4 r(float t) { return wCtrlPoints[0]; }
	virtual float tStart() { return 0; }
	virtual float tEnd() { return 1; }

	virtual void AddControlPoint(float cX, float cY) {
		vec4 wVertex = vec4(cX, cY, 0, 1) * camera.Pinv() * camera.Vinv();
		int i = 0;

		if (wCtrlPoints.size() == 0) 
			wCtrlPoints.push_back(wVertex);

		else {
			if (wCtrlPoints.size() == 1) {
				if (wCtrlPoints[0].x < wVertex.x) 
						wCtrlPoints.push_back(wVertex);

				else 
					wCtrlPoints.insert(wCtrlPoints.begin(), wVertex);
			}

			else {
				for (i = 0; i < (signed int) wCtrlPoints.size() - 1 && wVertex.x > wCtrlPoints[i].x; i++) {}

				if (i == wCtrlPoints.size()-1 && wVertex.x > wCtrlPoints[i].x)
					wCtrlPoints.push_back(wVertex);
				else
					wCtrlPoints.insert(wCtrlPoints.begin() + i, wVertex);
			}
		}
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

		if (wCtrlPoints.size() > 0) {	// draw control points
			glBindVertexArray(vaoCtrlPoints);
			glBindBuffer(GL_ARRAY_BUFFER, vboCtrlPoints);
			glBufferData(GL_ARRAY_BUFFER, wCtrlPoints.size() * 4 * sizeof(float), &wCtrlPoints[0], GL_DYNAMIC_DRAW);

			if (colorLocation >= 0) glUniform4f(colorLocation, color.x, color.y, color.z, color.w);
			glPointSize(5.0f);
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

			colorLocation = glGetUniformLocation(gpuProgram.getId(), "color");
			if (colorLocation >= 0) glUniform4f(colorLocation, color.x, color.y, color.z, color.w);

			glDrawArrays(GL_LINE_STRIP, 0, nTesselatedVertices);

			if (animate&&animateable) {
				// animation on curve
				float t = tCurrent;
				while (t > tEnd()) t -= tEnd();
				vec4 currentLocation = r(t);
				// copy data to the GPU
				glBindVertexArray(vaoAnimatedObject);
				glBindBuffer(GL_ARRAY_BUFFER, vboAnimatedObject);
				glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(float), &currentLocation, GL_DYNAMIC_DRAW);
				if (colorLocation >= 0) glUniform4f(colorLocation, color.x, color.y, color.z, color.w);

				//glDrawArrays(GL_)
				
			}
		}
	}
};

class CatmullRom: public Curve {
	std::vector<float>  ts;	// parameter (knot) values
	std::vector<vec2> points;

	vec4 Hermite(vec4 p0, vec4 v0, float t0, 
				 vec4 p1, vec4 v1, float t1,
					float t) {
		vec4 a3 = (p0 - p1)/((t1-t0)*(t1-t0)*(t1 - t0))* 2 + (v1 + v0)/((t1-t0)*(t1 - t0));
		vec4 a2 = (p1 - p0)/((t1 - t0)*(t1 - t0)) * 3 - (v1 + v0 * 2)/(t1-t0);
		
		return a3*(t-t0)*(t-t0)*(t-t0) +
			    a2*(t-t0)*(t-t0) + 
			         v0 * (t-t0)+ 
			              p0;
	}

public:
	
	void AddControlPoint(float cX, float cY) {

		ts.push_back((float)wCtrlPoints.size());
		Curve::AddControlPoint(cX, cY);
	}
	float tStart() {return ts[0]; }
	float tEnd() { return ts[wCtrlPoints.size()-1]; }

	virtual vec4 r(float t) {
		vec4 v0, v1;
		//0.5 * (1-t)
		for (int i = 0; i < (signed int) wCtrlPoints.size() - 1; i++){
			if (i <= t && t <= i+1) {
				if (i == 0) {
					v0 = 0;
				}

				else {
					v0 = ((wCtrlPoints[i+1] - wCtrlPoints[i])
					+ (wCtrlPoints[i] - wCtrlPoints[i - 1])) * 0.5f;
				}

				if (i == wCtrlPoints.size() - 2) {
					v1 = 0;
				}

				else 
					v1 = ((wCtrlPoints[i +  2] - wCtrlPoints[i + 1])
						+ (wCtrlPoints[i + 1] - wCtrlPoints[i])) * 0.5f;
				return Hermite(wCtrlPoints[i], v0, i, wCtrlPoints[i + 1], v1, i+1, t);
			}
		}

	}
};

// The virtual world: collection of two objects
Curve * curve;
Curve * background;

// popup menu event handler
void processMenuEvents(int option) {
	delete curve;
	switch (option) {
	case 0: curve = new CatmullRom();
		break;
	}
	glutPostRedisplay();
}

// Initialization, create an OpenGL context
void onInitialization() {
	// create the menu and tell glut that "processMenuEvents" will handle the events: Do not use it in your homework, because it is prohitibed by the portal
	int menu = glutCreateMenu(processMenuEvents);
	//add entries to our menu
	glutAddMenuEntry("CatmullRom", 0);
	// attach the menu to the right button
	//glutAttachMenu(GLUT_RIGHT_BUTTON);

	glViewport(0, 0, windowWidth, windowHeight);
	glLineWidth(2.0f);

	curve = new CatmullRom();
	background = new CatmullRom();

	background->SetColor(vec4(0.65, 0.33, 0, 1));

	//kezdõpontok elkészítése
	//föld
	float cX = 2.0f * 0 / windowWidth - 1;	// flip y axis
	float cY = 1.0f - 2.0f * 400 / windowHeight;
	curve->AddControlPoint(cX, cY);
	cX = 2.0f * 600 / windowWidth - 1;	// flip y axis
	curve->AddControlPoint(cX, cY);

	//háttér
	 cX = 2.0f * 0/ windowWidth - 1;	// flip y axis
	 cY = 1.0f - 2.0f * 150 / windowHeight;
	 background->AddControlPoint(cX, cY);

	 cX = 2.0f * 100 / windowWidth - 1;	// flip y axis
	 cY = 1.0f - 2.0f * 200 / windowHeight;
	 background->AddControlPoint(cX, cY);

	 cX = 2.0f * 200 / windowWidth - 1;	// flip y axis
	 cY = 1.0f - 2.0f * 80 / windowHeight;
	 background->AddControlPoint(cX, cY);

	 cX = 2.0f * 300 / windowWidth - 1;	// flip y axis
	 cY = 1.0f - 2.0f * 30 / windowHeight;
	 background->AddControlPoint(cX, cY);

	 cX = 2.0f * 400 / windowWidth - 1;	// flip y axis
	 cY = 1.0f - 2.0f * 180 / windowHeight;
	 background->AddControlPoint(cX, cY);

	 cX = 2.0f * 500 / windowWidth - 1;	// flip y axis
	 cY = 1.0f - 2.0f * 210 / windowHeight;
	 background->AddControlPoint(cX, cY);

	 cX = 2.0f * 600 / windowWidth - 1;	// flip y axis
	 cY = 1.0f - 2.0f * 100 / windowHeight;
	 background->AddControlPoint(cX, cY);

	// create program for the GPU
	gpuProgram.Create(vertexSource, fragmentSource, "outColor");
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0.25, 0.750, 1, 0);							// background color 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen

	curve->Draw();
	background->Draw();
	glutSwapBuffers();									// exchange the two buffers
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	animate = !animate;			// toggle animation
	curve->Anim();			
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