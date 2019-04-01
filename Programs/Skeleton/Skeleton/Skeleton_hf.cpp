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

// verte shader in GLSL: It is a Raw string (C++11) since it contains new line characters
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
		wCx = 10 * cosf(t);
		wCy = 0;
		wWx = 20;
		wWy = 20;
	}
};


Camera camera;	// 2D camera
bool animate = false;
GPUProgram gpuProgram; // vertex and fragment shaders
const int nTesselatedVertices = 1000;

// The virtual world: collection of two objects

class CatmullRom {
	float tension;
	vector<vec4> wCtrlPoints;
	unsigned int vaoCurve, vboCurve;
	bool animateable = false;

	vec4 color = vec4(1, 1, 1, 1);

	vec2 Hermite(float y0, float v0, float x0,
		float y1, float v1, float x1,
		float x)
	{

		float a3 = (y0 - y1) / ((x1 - x0)*(x1 - x0)*(x1 - x0)) * 2 + (v1 + v0) / ((x1 - x0)*(x1 - x0));
		float a2 = (y1 - y0) / ((x1 - x0)*(x1 - x0)) * 3 - (v1 + v0 * 2) / (x1 - x0);
		float a1 = v0;
		float a0 = y0;


		return vec2(a3 * (x - x0)*(x - x0)*(x - x0) +
			a2 * (x - x0)*(x - x0) +
			a1 * (x - x0) + a0,
			3 * a3*(x - x0)*(x - x0) + 2 * a2*(x - x0) + a1);
	}

public:
	CatmullRom(float tension) { this->tension = tension; }

	void Create() {
		// Curve
		glGenVertexArrays(1, &vaoCurve);
		glBindVertexArray(vaoCurve);

		glGenBuffers(1, &vboCurve); // Generate 1 vertex buffer object
		glBindBuffer(GL_ARRAY_BUFFER, vboCurve);
		// Enable the vertex attribute arrays
		glEnableVertexAttribArray(0);  // attribute array 0
		// Map attribute array 0 to the vertex data of the interleaved vbo
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), NULL); // attribute array, components/attribute, component type, normalize?, stride, offset
	}

	void Anim() {
		animateable = !animateable;
	}
	void SetColor(vec4 col) {
		color = col;
	}

	float xStart() { return wCtrlPoints[0].x; }
	float xEnd() { return wCtrlPoints[wCtrlPoints.size() - 1].x; }

	void AddControlPoint(float cX, float cY) {
		vec4 wVertex = vec4(cX, cY, 0, 1) * camera.Pinv() * camera.Vinv();
		int i = 0;

		if (wCtrlPoints.size() == 0) 
			wCtrlPoints.push_back(wVertex);

		else if (wCtrlPoints.size() == 1) {

			if (wCtrlPoints[0].x < wVertex.x)
				wCtrlPoints.push_back(wVertex);
			
			else 
				wCtrlPoints.insert(wCtrlPoints.begin(), wVertex);
		}

		else {

				for (i = 0; i < (signed int)wCtrlPoints.size() - 1 && wVertex.x >= wCtrlPoints[i].x; i++) {
					if (wCtrlPoints[i].x == wVertex.x) return;
				}

				if (i == wCtrlPoints.size() - 1 && wVertex.x > wCtrlPoints[i].x) {
					wCtrlPoints.push_back(wVertex);
				}

				else {
					wCtrlPoints.insert(wCtrlPoints.begin() + i, wVertex);
				}
			}
		}
	
	vec2 y(float x) {
		float v0, v1;

		
		for (signed int i = 0; i < wCtrlPoints.size() - 1; i++) {
			if (wCtrlPoints[i].x <= x && x <= wCtrlPoints[i + 1].x) {

				if (wCtrlPoints.size() == 2) {
					v0 = 0;
					v1 = 0;
				}

				else {
					if (i == 0) {
						v0 = 0;
						v1 = ((wCtrlPoints[i + 2].y - wCtrlPoints[i + 1].y) / (wCtrlPoints[i + 2].x - wCtrlPoints[i + 1].x) + (wCtrlPoints[i + 1].y - wCtrlPoints[i].y) / (wCtrlPoints[i + 1].x - wCtrlPoints[i].x))*(1.0f-tension);
						//printf("v0: %f\n", v0);
					}

					else if (i == wCtrlPoints.size() - 2) {
						v0 = ((wCtrlPoints[i + 1].y - wCtrlPoints[i].y) / (wCtrlPoints[i + 1].x - wCtrlPoints[i].x) + (wCtrlPoints[i].y - wCtrlPoints[i - 1].y) / (wCtrlPoints[i].x - wCtrlPoints[i - 1].x))*(1.0f - tension);
						v1 = 0;
						//printf("v1: %f\n", v1);
					}

					else {

						v0 = ((wCtrlPoints[i + 1].y - wCtrlPoints[i].y) / (wCtrlPoints[i + 1].x - wCtrlPoints[i].x) + (wCtrlPoints[i].y - wCtrlPoints[i - 1].y) / (wCtrlPoints[i].x - wCtrlPoints[i - 1].x))*(1.0f - tension);
						v1 = ((wCtrlPoints[i + 2].y - wCtrlPoints[i + 1].y) / (wCtrlPoints[i + 2].x - wCtrlPoints[i + 1].x) + (wCtrlPoints[i + 1].y - wCtrlPoints[i].y) / (wCtrlPoints[i + 1].x - wCtrlPoints[i].x))*(1.0f - tension);
						//printf("v0: %f\n", v0);
						//printf("v1: %f\n", v1);

					}
				}

				return Hermite(wCtrlPoints[i].y, v0, wCtrlPoints[i].x, wCtrlPoints[i + 1].y, v1, wCtrlPoints[i + 1].x, x);
			}
		}

	}

	void Draw() {
		mat4 VPTransform = camera.V() * camera.P();

		VPTransform.SetUniform(gpuProgram.getId(), "MVP");

		int colorLocation = glGetUniformLocation(gpuProgram.getId(), "color");

		if (wCtrlPoints.size() >= 2) {	// draw curve
			vector<float> vertexData;
			for (int i = 0; i < nTesselatedVertices; i++) {	// Tessellate
				float xNormalized = (float)i / (nTesselatedVertices - 1);
				float x = xStart() + (xEnd() - xStart()) * xNormalized;
				float y1 = y(x).x;

				if (i == 0) {
					vertexData.push_back(0);
					vertexData.push_back(-10.0f);
				}

				vertexData.push_back(x);
				vertexData.push_back(y1);

				vertexData.push_back(x);
				vertexData.push_back(-10.0f);

				if (i == nTesselatedVertices - 1) {
					vertexData.push_back(x);
					vertexData.push_back(-10.0f);
				}
			}

			// copy data to the GPU
			glBindVertexArray(vaoCurve);
			glBindBuffer(GL_ARRAY_BUFFER, vboCurve);
			glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), &vertexData[0], GL_STATIC_DRAW);

			colorLocation = glGetUniformLocation(gpuProgram.getId(), "color");
			if (colorLocation >= 0) glUniform4f(colorLocation, color.x, color.y, color.z, color.w);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, vertexData.size());
		}
	}
};

CatmullRom * curve;
CatmullRom * background;


class Cyclist {
private:
	unsigned int vaoCyclist, vboCyclist;
	unsigned int vboHuman;
	float r= 1;
	float v = 1;
	vec2 wTranslate = vec2(-10.0f, 0.0f);
	vec2 position = vec2(0, 0);
	float sina;
	float fi=0;
	float ds;

public:
	void Create() {
		glGenVertexArrays(1, &vaoCyclist);
		glBindVertexArray(vaoCyclist);
		// Curve
		glGenBuffers(1, &vboCyclist); // Generate 1 vertex buffer object
		glBindBuffer(GL_ARRAY_BUFFER, vboCyclist);
		glGenBuffers(1, &vboHuman); // Generate 1 vertex buffer object
		// Enable the vertex attribute arrays
		glEnableVertexAttribArray(0);  // attribute array 0
		// Map attribute array 0 to the vertex data of the interleaved vbo
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2* sizeof(float), NULL); // attribute array, components/attribute, component type, normalize?, stride, offset
	}

	float GetSpeed() {
		return v;
	}
	
	vec2 GetPosition() {
		return position;
	}

	void SetPosition(float Dt) {
		static bool dir = true;

		float dY = curve->y(position.x).y;
		float Tnormy = 1 / sqrtf(1 + (dY*dY));
		float Tnormx = -1.0f * dY / sqrtf(1 + (dY*dY));

		wTranslate.x = Tnormx + position.x;
		wTranslate.y = Tnormy + position.y;

		ds = sqrtf(1 + dY * dY);

		float Vlength = sqrtf(1+dY*dY);

		sina = dY / Vlength;
		dir ? v = 2.0f - 1.0f * sina : v = 2.0f + 1.0f * sina;

		float ds = Dt * v;
		float dx = ds / sqrtf(1 + dY * dY);
		
		if (dir) {
			position.x += dx;
			fi += ds;
			position.y = curve->y(position.x).x;
			if ((position.x + dx) > curve->xEnd()) 
				dir = !dir;
		}
		
		else {
			position.x -= dx;
			fi+= ds;
			position.y = curve->y(position.x).x;
			if ((position.x - dx) < curve->xStart())
				dir = !dir;
		}

	}
	mat4 MrotateTranslate() {

		mat4 Mrotate(cosf(fi), sinf(fi), 0, 0,
			-sinf(fi), cosf(fi), 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);

		mat4 Mtranslate(1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 0, 0,
			wTranslate.x, wTranslate.y, 0, 1);

		return Mrotate * Mtranslate ;
	}

	mat4 MTranslate() {

		mat4 Mtranslate(1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 0, 0,
			wTranslate.x, wTranslate.y, 0, 1);

		return Mtranslate;
	}

	void Draw() {
		
		mat4 MVPTransform = MrotateTranslate() * camera.V() * camera.P();

		MVPTransform.SetUniform(gpuProgram.getId(), "MVP");

		int colorLocation = glGetUniformLocation(gpuProgram.getId(), "color");

		vector<float> circleData;
		vector<float> humanData;
		vector<vec2> pedalCircle;
		vector<vec2> seatCircle;

		for (int i = 0; i < 180; i++) {
			vec2 cord = vec2((float) ( cosf(M_PI * i * 2 / 180.0)), (float) (sinf(M_PI * i * 2 / 180.0)));

			if (i == 0) {
				circleData.push_back((float)cord.x);
				circleData.push_back((float)cord.y);
			}

			else if (i == 179)
			{
				circleData.push_back((float)circleData[0]);
				circleData.push_back((float)circleData[1]);
			}

			else {
				circleData.push_back((float)cord.x);
				circleData.push_back((float)cord.y);
				circleData.push_back((float)cord.x);
				circleData.push_back((float)cord.y);
			}
		}

			vec2 cord;
			cord = vec2(cosf(M_PI * 0 / 4.0f), sinf(M_PI *  0 / 4 ));
			circleData.push_back((float)cord.x);
			circleData.push_back((float)cord.y);
			cord = vec2(cosf(M_PI * 4 / 4.0f), sinf(M_PI * 4 / 4 ));
			circleData.push_back((float)cord.x);
			circleData.push_back((float)cord.y);
			cord = vec2(cosf(M_PI * 1 / 4.0f ), sinf(M_PI * 1 / 4 ));
			circleData.push_back((float)cord.x);
			circleData.push_back((float)cord.y);
			cord = vec2(cosf(M_PI * 5 / 4.0f ), sinf(M_PI * 5 / 4 ));
			circleData.push_back((float)cord.x);
			circleData.push_back((float)cord.y);
			cord = vec2(cosf(M_PI * 2 / 4.0f ), sinf(M_PI * 2 / 4 ));
			circleData.push_back((float)cord.x);
			circleData.push_back((float)cord.y);
			cord = vec2(cosf(M_PI * 6 / 4.0f), sinf(M_PI * 6 / 4));
			circleData.push_back((float)cord.x);
			circleData.push_back((float)cord.y);
			cord = vec2(cosf(M_PI * 3 / 4.0f ), sinf(M_PI * 3 / 4 ));
			circleData.push_back((float)cord.x);
			circleData.push_back((float)cord.y);
			cord = vec2(cosf(M_PI * 7 / 4.0 ), sinf(M_PI * 7 / 4 ));
			circleData.push_back((float)cord.x);
			circleData.push_back((float)cord.y);
			cord = vec2(cosf(M_PI * 4 / 4.0f ), sinf(M_PI * 4 / 4));
			circleData.push_back((float)cord.x);
			circleData.push_back((float)cord.y);
			cord = vec2(cosf(M_PI * 8 / 4.0f ), sinf(M_PI * 8 / 4 ));
			circleData.push_back((float)cord.x);
			circleData.push_back((float)cord.y);

			cord = vec2(cosf(M_PI * 2 / 4.0), sinf(M_PI * 2 / 4)); // rúd alja
			humanData.push_back((float)cord.x);
			humanData.push_back((float)cord.y);

			vec2 seat = vec2(cosf(M_PI * 2 / 4.0), sinf(M_PI * 2 / 4)+0.5);
			humanData.push_back((float)seat.x);
			humanData.push_back((float)seat.y); //rúd vége

			cord = vec2(cosf(M_PI * 2 / 4.0f) - 0.5, sinf(M_PI * 2 / 4.0f)+ 0.5); //ülés bal oldala
			humanData.push_back((float)cord.x);
			humanData.push_back((float)cord.y);

			cord = vec2(cosf(M_PI * 2 / 4.0f) + 0.5, sinf(M_PI * 2 / 4.0f) + 0.5); //ülés jobb oldala
			humanData.push_back((float)cord.x);
			humanData.push_back((float)cord.y);

			vec2 seatEdge = vec2(cosf(M_PI * 2 / 4.0), sinf(M_PI * 2 / 4)+ 0.5); //ülés közepe
			humanData.push_back((float)seatEdge.x);
			humanData.push_back((float)seatEdge.y);

			vec2 pedal = vec2(cosf(fi) * 0.5f , sinf(fi) *0.5f);
				
			//printf("pedal: %f, %f    seat: %f, %f", pedal.x, pedal.y, seatEdge.x, seatEdge.y);

			for (int i = 0; i < 100; i++) {
				pedalCircle.push_back(vec2(cosf(M_PI *4* i / 100.0f) *1.2  + pedal.x, sinf(M_PI *4* i / 100.0f)*1.2 + pedal.y));
				seatCircle.push_back(vec2(cosf(M_PI * 4* i / 100.0f)  + seatEdge.x, sinf(M_PI * 4* i / 100.0f) + seatEdge.y));
			}

			vec2 intersect;
			for (int i = 0; i < pedalCircle.size()-1 ; i++) {
				printf("%f\n", fi);
				if (abs((pedalCircle[i].x - seatEdge.x)*(pedalCircle[i].x - seatEdge.x) + (pedalCircle[i].y - seatEdge.y) * (pedalCircle[i].y - seatEdge.y) - 1.44) < 0.01)
					printf(">>>>>>>>>>>MEGVAN: %f\n", abs((pedalCircle[i].x - seatEdge.x)*(pedalCircle[i].x - seatEdge.x) + (pedalCircle[i].y - seatEdge.y)*(pedalCircle[i].y - seatEdge.y)-1));
					intersect = vec2(pedalCircle[i].x, pedalCircle[i].y);
					printf("X: %f, Y: %F\n", intersect.x, intersect.y);
			}

			//printf("%f, %f\n", intersect.x, intersect.y);
			humanData.push_back(intersect.x);
			humanData.push_back(intersect.y);

			//humanData.push_back(intersect.x);
			//humanData.push_back(intersect.y);

			//humanData.push_back((float)pedal.x);
			//humanData.push_back((float)pedal.y);


		// copy data to the GPU
		/*glBindVertexArray(vaoCyclist);
		glBindBuffer(GL_ARRAY_BUFFER, vboCyclist);
		glBufferData(GL_ARRAY_BUFFER, pedalCircle.size() * sizeof(float), &pedalCircle[0], GL_DYNAMIC_DRAW);
		if (colorLocation >= 0) glUniform4f(colorLocation, 0.72f, 0.16f , 0.0f, 1.0f);
		glDrawArrays(GL_LINES, 0, pedalCircle.size());

		MVPTransform = MTranslate() * camera.V() * camera.P();
		MVPTransform.SetUniform(gpuProgram.getId(), "MVP");

		glBindVertexArray(vaoCyclist);
		glBindBuffer(GL_ARRAY_BUFFER, vboCyclist);
		glBufferData(GL_ARRAY_BUFFER, seatCircle.size() * 2* sizeof(float), &seatCircle[0], GL_DYNAMIC_DRAW);
		if (colorLocation >= 0) glUniform4f(colorLocation, 0.72f, 0.16f, 0.0f, 1.0f);
		glDrawArrays(GL_LINES, 0, seatCircle.size());*/

		glBindVertexArray(vaoCyclist);
		glBindBuffer(GL_ARRAY_BUFFER, vboCyclist);
		glBufferData(GL_ARRAY_BUFFER, circleData.size() * sizeof(float), &circleData[0], GL_DYNAMIC_DRAW);
		if (colorLocation >= 0) glUniform4f(colorLocation, 0.72f, 0.16f, 0.0f, 1.0f);
		glDrawArrays(GL_LINES, 0, circleData.size());

		/*glBindVertexArray(vaoCyclist);
		glBindBuffer(GL_ARRAY_BUFFER, vboCyclist);
		glBufferData(GL_ARRAY_BUFFER, humanData.size() * sizeof(float), &seatCircle[0], GL_STATIC_DRAW);
		if (colorLocation >= 0) glUniform4f(colorLocation, 0.72f, 0.16f, 0.0f, 1.0f);
		glDrawArrays(GL_LINES, 0, humanData.size());*/
	}

};

Cyclist* cyclist;

	// Initialization, create an OpenGL context
	void onInitialization() {

		glViewport(0, 0, windowWidth, windowHeight);
		glLineWidth(2.0f);

	
		curve = new CatmullRom(-0.6f);
		background = new CatmullRom(0.3f);
		cyclist = new Cyclist();

		curve->Create();
		background->Create();
		cyclist->Create();

		// create program for the GPU
		gpuProgram.Create(vertexSource, fragmentSource, "outColor");

		background->SetColor(vec4(0.45f, 0.45f, 0.45f, 1));
		curve->SetColor(vec4(0.45f, 0.68f, 0.125f, 1));

		//kezdõpontok elkészítése

		float cX = 2.0f * 0 / windowWidth - 1;	// flip y axis
		float cY = 1.0f - 2.0f * 400 / windowHeight;
		curve->AddControlPoint(cX, cY);

		cX = 2.0f * 600 / windowWidth - 1;	// flip y axis
		curve->AddControlPoint(cX, cY);

		//háttér
		cX = 2.0f * 0 / windowWidth - 1;	// flip y axis
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
	}

	// Window has become invalid: Redraw
	void onDisplay() {
		glClearColor(0.63f, 0.80f, 0.92f, 1.0f);							// background color 
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen

		background->Draw();
		curve->Draw();
		cyclist->Draw();

		glutSwapBuffers();
								// exchange the two buffers
	}

	// Key of ASCII code pressed
	void onKeyboard(unsigned char key, int pX, int pY) {
		glutPostRedisplay();        // redraw
	}

	// Key of ASCII code released
	void onKeyboardUp(unsigned char key, int pX, int pY) {

	}

	// Mouse click event
	void onMouse(int button, int state, int pX, int pY) {
		if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {  // GLUT_LEFT_BUTTON / GLUT_RIGHT_BUTTON and GLUT_DOWN / GLUT_UP
			float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
			float cY = 1.0f - 2.0f * pY / windowHeight;
			curve->AddControlPoint(cX, cY);
			glutPostRedisplay();     // redraw
		}
	}

	// Move mouse with key pressed
	void onMouseMotion(int pX, int pY) {
		float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
		float cY = 1.0f - 2.0f * pY / windowHeight;

	}

	static void IdleFunc() {
		static float tend = 0;
		const float dt = 0.01;
		float tstart = tend;
		tend = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
		for (float t = tstart; t < tend; t += dt) {
			float Dt = fmin(dt, tend - t);
			cyclist->SetPosition(Dt);
		}
	}

	// Idle event indicating that some time elapsed: do animation here
	void onIdle() {
		IdleFunc();
		glutPostRedisplay();
	}
	
	
