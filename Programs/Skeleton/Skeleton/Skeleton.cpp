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

const char * const vertexSource = R"(
	#version 330				// Shader 3.3
	precision highp float;		// normal floats, makes no difference on desktop computers
	uniform mat4 MVP;			// uniform variable, the Model-View-Projection transformation matrix
 
	layout(location = 0) in vec2 vp;	// Varying input: vp = vertex position is expected in attrib array 0
	layout(location = 1) in vec2 vertexUV; //Attrib array 1 
 
	out vec2 texCoord;
    void main() {
		gl_Position = vec4(vp.x, vp.y, 0, 1) * MVP;		// transform vp from modeling space to normalized device space
		texCoord=vertexUV;
	}
)";

const char * const fragmentSource = R"(
	#version 330			// Shader 3.3
	precision highp float;	// normal floats, makes no difference on desktop computers
	
	uniform vec4 color;		// uniform variable, the color of the primitive
	out vec4 outColor;		// computed color of the current pixel
	
	uniform sampler2D textureUnit;
	uniform int isGPUProcedural;
 
	in vec2 texCoord;
 
	void main() {
		if(isGPUProcedural == 0){
			outColor = color;	// computed color is the color of the primitive
		}
 
		else{
				outColor = texture(textureUnit, texCoord);
			}
	}
)";

struct Camera {
	bool follow = false;
	float wCx, wCy;
	float wWx, wWy;
public:
	Camera() : wCx(0), wCy(0), wWx(40), wWy(40) {}

	mat4 Vnofollow() {
		return mat4(1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);
	}

	mat4 Pnofollow() {
		return mat4((float)2 / 40, 0, 0, 0,
			0, (float)2 / 40, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);
	}

	mat4 V() {
		return mat4(1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			-wCx, -wCy, 0, 1);
	}

	mat4 P() {
		return mat4(2 / wWx, 0, 0, 0,
			0, 2 / wWy, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);
	}

	mat4 Vinv() {
		return mat4(1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			wCx, wCy, 0, 1);
	}

	mat4 Pinv() {
		return mat4(wWx / 2, 0, 0, 0,
			0, wWy / 2, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);
	}

	void Zoom() { wWx /= 2; wWy /= 2; }
	void Follow(vec2 pos) {
		wCx = pos.x;
		wCy = pos.y;
	}
};


Camera camera;
GPUProgram gpuProgram;
const int nTesselatedVertices = 1000;
bool isGPUProcedural = true;


class CatmullRom {
	float tension;
	vector<vec4> wCtrlPoints;
	unsigned int vaoCurve, vboCurve;

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
		glGenVertexArrays(1, &vaoCurve);
		glBindVertexArray(vaoCurve);

		glGenBuffers(1, &vboCurve);
		glBindBuffer(GL_ARRAY_BUFFER, vboCurve);

		glEnableVertexAttribArray(0);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
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

			for (i = 0; i < wCtrlPoints.size() - 1 && wVertex.x >= wCtrlPoints[i].x; i++) {
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


		for (int i = 0; i < wCtrlPoints.size() - 1; i++) {
			if (wCtrlPoints[i].x <= x && x <= wCtrlPoints[i + 1].x) {

				if (wCtrlPoints.size() == 2) {
					v0 = 0;
					v1 = 0;
				}

				else {
					if (i == 0) {
						v0 = 0;
						v1 = ((wCtrlPoints[i + 2].y - wCtrlPoints[i + 1].y) / (wCtrlPoints[i + 2].x - wCtrlPoints[i + 1].x) + (wCtrlPoints[i + 1].y - wCtrlPoints[i].y) / (wCtrlPoints[i + 1].x - wCtrlPoints[i].x))*(1.0f - tension);
					}

					else if (i == wCtrlPoints.size() - 2) {
						v0 = ((wCtrlPoints[i + 1].y - wCtrlPoints[i].y) / (wCtrlPoints[i + 1].x - wCtrlPoints[i].x) + (wCtrlPoints[i].y - wCtrlPoints[i - 1].y) / (wCtrlPoints[i].x - wCtrlPoints[i - 1].x))*(1.0f - tension);
						v1 = 0;
					}

					else {

						v0 = ((wCtrlPoints[i + 1].y - wCtrlPoints[i].y) / (wCtrlPoints[i + 1].x - wCtrlPoints[i].x) + (wCtrlPoints[i].y - wCtrlPoints[i - 1].y) / (wCtrlPoints[i].x - wCtrlPoints[i - 1].x))*(1.0f - tension);
						v1 = ((wCtrlPoints[i + 2].y - wCtrlPoints[i + 1].y) / (wCtrlPoints[i + 2].x - wCtrlPoints[i + 1].x) + (wCtrlPoints[i + 1].y - wCtrlPoints[i].y) / (wCtrlPoints[i + 1].x - wCtrlPoints[i].x))*(1.0f - tension);

					}
				}

				vec2 result = Hermite(wCtrlPoints[i].y, v0, wCtrlPoints[i].x, wCtrlPoints[i + 1].y, v1, wCtrlPoints[i + 1].x, x);
				return result;
			}
		}
		return 0;
	}

	bool Under(float x1, float y1) {
		float Y = y(x1).x;
		return y1 < Y;
	}

	void Draw() {

		int location = glGetUniformLocation(gpuProgram.getId(), "isGPUProcedural");
		if (location >= 0) glUniform1i(location, isGPUProcedural);
		else printf("isGPUProcedural cannot be set\n");

		mat4 VPTransform = camera.V() * camera.P();

		VPTransform.SetUniform(gpuProgram.getId(), "MVP");

		int colorLocation = glGetUniformLocation(gpuProgram.getId(), "color");

		if (wCtrlPoints.size() >= 2) {
			vector<float> vertexData;
			for (int i = 0; i < nTesselatedVertices; i++) {
				float xNormalized = (float)i / (nTesselatedVertices - 1);
				float x = xStart() + (xEnd() - xStart()) * xNormalized;
				float y1 = y(x).x;

				vertexData.push_back(x);
				vertexData.push_back(y1);

				vertexData.push_back(x);
				vertexData.push_back(-100.0f);
			}

			glBindVertexArray(vaoCurve);
			glBindBuffer(GL_ARRAY_BUFFER, vboCurve);
			glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), &vertexData[0], GL_STATIC_DRAW);

			colorLocation = glGetUniformLocation(gpuProgram.getId(), "color");
			if (colorLocation >= 0) glUniform4f(colorLocation, color.x, color.y, color.z, color.w);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, vertexData.size() / 2);
		}
	}
};

CatmullRom * curve;
CatmullRom * background;


class Cyclist {
private:
	unsigned int vaoCyclist, vboCyclist;
	unsigned int vboHuman;
	float r = 1;
	float v = 0;
	vec2 wTranslate = vec2(-10.0f, 0.0f);
	vec2 position = vec2(0, 0);
	float sina, fi = 0, ds;
	float cordX, cordY;
	vector<float> circleData;
	vector<float> humanData;
	float dir = 1.0f;

public:
	Cyclist() {
		CalculateCircle();
	}
	void Create() {
		glGenVertexArrays(1, &vaoCyclist);
		glBindVertexArray(vaoCyclist);

		glGenBuffers(1, &vboCyclist);
		glBindBuffer(GL_ARRAY_BUFFER, vboCyclist);

		glEnableVertexAttribArray(0);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	vec2 GetPosition() {
		return position;
	}

	void SetPosition(float Dt) {

		float dY = curve->y(position.x).y;
		float Tnormy = 1 / sqrtf(1 + (dY*dY));
		float Tnormx = -1.0f * dY / sqrtf(1 + (dY*dY));

		wTranslate.x = Tnormx + position.x;
		wTranslate.y = Tnormy + position.y;

		ds = sqrtf(1 + dY * dY);

		float Vlength = sqrtf(1 + dY * dY);

		sina = dY / Vlength;
		dir == 1 ? v = (900.0f - 900.0f * sina) / 1.16f : v = (900.0f + 900.0f * sina) / 1.16f;

		float ds = Dt * v;
		float dx = ds / sqrtf(1 + dY * dY);

		if (dir == 1.0) {
			position.x += dx;
			fi -= ds;
			position.y = curve->y(position.x).x;
			if ((position.x + dx) > curve->xEnd() - 10)
				dir = -1.0f;

		}

		else {
			position.x -= dx;
			fi += ds;
			position.y = curve->y(position.x).x;
			if ((position.x - dx) < curve->xStart() + 10)
				dir = 1.0f;
		}

	}

	void CalculateCircle() {
		humanData.clear();
		circleData.clear();

		for (int i = 0; i < 180; i++) {
			cordX = cosf((float)M_PI * i * 2 / 180.0f);
			cordY = sinf((float)M_PI * i * 2 / 180.0f);

			if (i == 0) {
				circleData.push_back(cordX);
				circleData.push_back(cordY);
			}

			else if (i == 179)
			{
				circleData.push_back(cosf((float)M_PI * i * 2 / 180.0f));
				circleData.push_back(sinf((float)M_PI * i * 2 / 180.0f));

				circleData.push_back(cosf((float)M_PI * i * 2 / 180.0f));
				circleData.push_back(sinf((float)M_PI * i * 2 / 180.0f));

				circleData.push_back(cosf((float)M_PI * 0 * 2 / 180.0f));
				circleData.push_back(sinf((float)M_PI * 0 * 2 / 180.0f));
			}

			else {
				circleData.push_back(cordX);
				circleData.push_back(cordY);
				circleData.push_back(cordX);
				circleData.push_back(cordY);
			}
		}

		cordX = cosf((float)M_PI * 0.0f / 4.0f);
		cordY = sinf((float)M_PI *  0.0f / 4.0f);
		circleData.push_back(cordX);
		circleData.push_back((float)cordY);

		cordX = cosf((float)M_PI * 4.0f / 4.0f);
		cordY = sinf((float)M_PI * 4.0f / 4.0f);
		circleData.push_back(cordX);
		circleData.push_back(cordY);

		cordX = cosf((float)M_PI * 1.0f / 4.0f);
		cordY = sinf((float)M_PI * 1.0f / 4.0f);
		circleData.push_back(cordX);
		circleData.push_back(cordY);

		cordX = cosf((float)M_PI * 5.0f / 4.0f);
		cordY = sinf((float)M_PI * 5.0f / 4.0f);
		circleData.push_back(cordX);
		circleData.push_back(cordY);

		cordX = cosf((float)M_PI * 2 / 4.0f);
		cordY = sinf((float)M_PI * 2 / 4.0f);
		circleData.push_back(cordX);
		circleData.push_back(cordY);

		cordX = cosf((float)M_PI * 6 / 4.0f);
		cordY = sinf((float)M_PI * 6 / 4.0f);
		circleData.push_back(cordX);
		circleData.push_back(cordY);

		cordX = cosf((float)M_PI * 3 / 4.0f);
		cordY = sinf((float)M_PI * 3 / 4.0f);
		circleData.push_back(cordX);
		circleData.push_back(cordY);

		cordX = cosf((float)M_PI * 7 / 4.0f);
		cordY = sinf((float)M_PI * 7 / 4.0f);
		circleData.push_back(cordX);
		circleData.push_back(cordY);

		cordX = cosf((float)M_PI * 4 / 4.0f);
		cordY = sinf((float)M_PI * 4 / 4);
		circleData.push_back(cordX);
		circleData.push_back(cordY);

		cordX = cosf((float)M_PI * 8.0f / 4.0f);
		cordY = sinf((float)M_PI * 8.0f / 4.0f);
		circleData.push_back((float)cordX);
		circleData.push_back(cordY);

		cordX = cosf((float)M_PI * 2 / 4.0);
		cordY = sinf((float)M_PI * 2 / 4);
		humanData.push_back((float)cordX);
		humanData.push_back((float)cordY);

		vec2 seat = vec2(cosf((float)M_PI * 2.0f / 4.0f), sinf((float)M_PI * 2.0f / 4.0f) + 0.5f);
		humanData.push_back((float)seat.x);
		humanData.push_back((float)seat.y);


		cordX = cosf((float)M_PI * 2.0f / 4.0f) - 0.5f;
		cordY = sinf((float)(float)M_PI * 2.0f / 4.0f) + 0.5f;
		humanData.push_back((float)cordX);
		humanData.push_back((float)cordY);

		cordX = cosf((float)M_PI * 2.0f / 4.0f) + 0.5f;
		cordY = sinf((float)M_PI * 2.0f / 4.0f) + 0.5f;
		humanData.push_back((float)cordX);
		humanData.push_back((float)cordY);

		vec2 seatEdge = vec2(cosf((float)M_PI * 2.0f / 4.0f), sinf((float)M_PI * 2.0f / 4.0f) + 0.5f);
		humanData.push_back((float)seatEdge.x);
		humanData.push_back((float)seatEdge.y);

		vec2 pedal = vec2(cosf(fi) * 0.5f, sinf(fi) *0.5f);
		vec2 pedal2 = vec2(cosf(fi + (float)M_PI) * 0.5f, (float)sinf(fi + (float)M_PI) *0.5f);
		vector<vec2> pedal2Circle;

		vector<vec2> pedalCircle;
		vector<vec2> seatCircle;

		for (int i = 0; i < 100; i++) {
			pedalCircle.push_back(vec2(cosf((float)M_PI - dir * i / 50.0f*(float)M_PI) * 1.2f + pedal.x, sinf((float)M_PI - dir * i / 50.0f *(float)M_PI)*1.2f + pedal.y));
			pedal2Circle.push_back(vec2(cosf((float)M_PI - dir * i / 50.0f*(float)M_PI) * 1.2f + pedal2.x, sinf((float)M_PI - dir * i / 50.0f *(float)M_PI)*1.2f + pedal2.y));
			seatCircle.push_back(vec2(cosf((float)M_PI - dir * i / 50.0f*(float)M_PI), sinf((float)M_PI - dir * i / 50.0f * (float)M_PI)));
		}

		float intersectX = 0, intersectY = 0;
		float intersectX2 = 0, intersectY2 = 0;

		for (int i = 0; i < (signed int)pedalCircle.size() - 1; i++) {
			if (fabs((pedalCircle[i].x)*(pedalCircle[i].x) + (pedalCircle[i].y - seatEdge.y) * (pedalCircle[i].y - seatEdge.y) - 1.44) < 0.1f) {
				intersectX = pedalCircle[i].x;
				intersectY = pedalCircle[i].y;
			}

			else if (fabs((pedal2Circle[i].x)*(pedal2Circle[i].x) + (pedal2Circle[i].y - seatEdge.y) * (pedal2Circle[i].y - seatEdge.y) - 1.44) < 0.2f) {
				intersectX2 = pedal2Circle[i].x;
				intersectY2 = pedal2Circle[i].y;
			}
		}
		humanData.push_back(intersectX);
		humanData.push_back(intersectY);

		humanData.push_back(intersectX);
		humanData.push_back(intersectY);

		humanData.push_back((float)pedal.x);
		humanData.push_back((float)pedal.y);

		humanData.push_back((float)seatEdge.x);
		humanData.push_back((float)seatEdge.y);

		humanData.push_back(intersectX2);
		humanData.push_back(intersectY2);

		humanData.push_back(intersectX2);
		humanData.push_back(intersectY2);

		humanData.push_back((float)pedal2.x);
		humanData.push_back((float)pedal2.y);

		humanData.push_back(seatEdge.x);
		humanData.push_back(seatEdge.y);

		humanData.push_back(seatEdge.x);
		humanData.push_back(seatEdge.y + 1.5f);

		humanData.push_back(seatEdge.x - 0.7f);
		humanData.push_back(seatEdge.y + 0.8f);

		humanData.push_back(seatEdge.x + 0.7f);
		humanData.push_back(seatEdge.y + 1.0f);


		for (int i = 0; i < 100; i++) {
			if (i == 0) {
				humanData.push_back(cosf((float)M_PI - i / 50.0f*(float)M_PI) * 0.5f + seatEdge.x);
				humanData.push_back(sinf((float)M_PI - i / 50.0f *(float)M_PI)*0.5f + seatEdge.y + 2.0f);
			}

			else if (i == 99) {
				humanData.push_back(cosf((float)M_PI - i / 50.0f*(float)M_PI) * 0.5f + seatEdge.x);
				humanData.push_back(sinf((float)M_PI - i / 50.0f * (float)M_PI)*0.5f + seatEdge.y + 2.0f);
				humanData.push_back(cosf((float)M_PI - i / 50.0f*(float)M_PI) * 0.5f + seatEdge.x);
				humanData.push_back(sinf((float)M_PI - i / 50.0f *(float)M_PI)*0.5f + seatEdge.y + 2.0f);

				humanData.push_back(cosf((float)M_PI - 0 / 50.0f*(float)M_PI) * 0.5f + seatEdge.x);
				humanData.push_back(sinf((float)M_PI - 0 / 50.0f *(float)M_PI)*0.5f + seatEdge.y + 2.0f);
			}

			else {
				humanData.push_back(cosf((float)M_PI - i / 50.0f*(float)M_PI) * 0.5f + seatEdge.x);
				humanData.push_back(sinf((float)M_PI - i / 50.0f *(float)M_PI)*0.5f + seatEdge.y + 2.0f);
				humanData.push_back(cosf((float)M_PI - i / 50.0f*(float)M_PI)*0.5f + seatEdge.x);
				humanData.push_back(sinf((float)M_PI - i / 50.0f * (float)M_PI) *0.5f + seatEdge.y + 2.0f);
			}
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

		return Mrotate * Mtranslate;
	}

	mat4 MTranslate() {

		mat4 Mtranslate(1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 0, 0,
			wTranslate.x, wTranslate.y, 0, 1);

		return Mtranslate;
	}

	void Draw() {

		int location = glGetUniformLocation(gpuProgram.getId(), "isGPUProcedural");
		if (location >= 0) glUniform1i(location, isGPUProcedural);
		else printf("isGPUProcedural cannot be set\n");

		mat4 MVPTransform = MrotateTranslate() * camera.V() * camera.P();

		MVPTransform.SetUniform(gpuProgram.getId(), "MVP");

		int colorLocation = glGetUniformLocation(gpuProgram.getId(), "color");

		glBindVertexArray(vaoCyclist);
		glBindBuffer(GL_ARRAY_BUFFER, vboCyclist);
		glBufferData(GL_ARRAY_BUFFER, circleData.size() * sizeof(float), &circleData[0], GL_STATIC_DRAW);
		if (colorLocation >= 0) glUniform4f(colorLocation, 0.72f, 0.16f, 0.0f, 1.0f);
		glDrawArrays(GL_LINES, 0, circleData.size() / 2);

		MVPTransform = MTranslate() * camera.V() * camera.P();
		MVPTransform.SetUniform(gpuProgram.getId(), "MVP");

		glBindBuffer(GL_ARRAY_BUFFER, vboCyclist);
		glBufferData(GL_ARRAY_BUFFER, humanData.size() * sizeof(float), &humanData[0], GL_STATIC_DRAW);
		if (colorLocation >= 0) glUniform4f(colorLocation, 0.72f, 0.16f, 0.0f, 1.0f);
		glDrawArrays(GL_LINES, 0, humanData.size() / 2);
	}

};

Cyclist* cyclist;


std::vector<vec4> BackgroundTexture(int& width, int& height) {
	std::vector<vec4> image(width*width);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			float wX = (float)x / (width / camera.wWx) - camera.wWx / 2;
			float wY = (float)y / (height / camera.wWy) - camera.wWy / 2;
			if (background->Under(wX, wY))
				image[y * width + x] = vec4(0.45f, 0.45f, 0.45f, 1.0f);

			else
				image[y * width + x] = vec4(0.63f, 0.80f, 0.92f, 1.0f);
		}
	}
	return image;
}

class TexturedQuad {
	unsigned int vao, vbo[2], textureId;
	vec2 vertices[4], uvs[4];

	Texture * pTexture;

public:
	TexturedQuad() {
		vertices[0] = vec2(-20, -20); uvs[0] = vec2(0, 0);
		vertices[1] = vec2(20, -20);  uvs[1] = vec2(1, 0);
		vertices[2] = vec2(20, 20);   uvs[2] = vec2(1, 1);
		vertices[3] = vec2(-20, 20);  uvs[3] = vec2(0, 1);
	}
	void Create() {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glGenBuffers(2, vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);

		int width = 600;
		int height = 600;
		std::vector<vec4> image = BackgroundTexture(width, height);

		pTexture = new Texture(width, height, image);
	}

	void Draw() {
		glBindVertexArray(vao);

		mat4 MVPTransform = camera.Vnofollow() * camera.Pnofollow();

		MVPTransform.SetUniform(gpuProgram.getId(), "MVP");

		int location = glGetUniformLocation(gpuProgram.getId(), "isGPUProcedural");
		if (location >= 0) glUniform1i(location, isGPUProcedural);
		else printf("isGPUProcedural cannot be set\n");

		pTexture->SetUniform(gpuProgram.getId(), "textureUnit");

		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}
};

TexturedQuad quad;

void onInitialization() {

	glViewport(0, 0, windowWidth, windowHeight);
	glLineWidth(2.0f);


	curve = new CatmullRom(-0.6f);
	background = new CatmullRom(0.3f);
	cyclist = new Cyclist();

	gpuProgram.Create(vertexSource, fragmentSource, "outColor");
	curve->SetColor(vec4(0.45f, 0.68f, 0.125f, 1));

	float cX = 2.0f * 0 / windowWidth - 1;
	float cY = 1.0f - 2.0f * 400 / windowHeight;
	curve->AddControlPoint(cX, cY);

	cX = 2.0f * 600 / windowWidth - 1;
	curve->AddControlPoint(cX, cY);

	cX = -1.5f;
	curve->AddControlPoint(cX, cY);

	cX = 1.5f;
	curve->AddControlPoint(cX, cY);

	cX = 2.0f * 0 / windowWidth - 1;
	cY = 1.0f - 2.0f * 150 / windowHeight;
	background->AddControlPoint(cX, cY);

	cX = 2.0f * 100 / windowWidth - 1;
	cY = 1.0f - 2.0f * 200 / windowHeight;
	background->AddControlPoint(cX, cY);

	cX = 2.0f * 200 / windowWidth - 1;
	cY = 1.0f - 2.0f * 80 / windowHeight;
	background->AddControlPoint(cX, cY);

	cX = 2.0f * 300 / windowWidth - 1;
	cY = 1.0f - 2.0f * 30 / windowHeight;
	background->AddControlPoint(cX, cY);

	cX = 2.0f * 400 / windowWidth - 1;
	cY = 1.0f - 2.0f * 180 / windowHeight;
	background->AddControlPoint(cX, cY);

	cX = 2.0f * 500 / windowWidth - 1;
	cY = 1.0f - 2.0f * 210 / windowHeight;
	background->AddControlPoint(cX, cY);

	cX = 2.0f * 600 / windowWidth - 1;
	cY = 1.0f - 2.0f * 100 / windowHeight;
	background->AddControlPoint(cX, cY);

	quad.Create();
	curve->Create();
	background->Create();
	cyclist->Create();

}

void onDisplay() {
	glClearColor(0.63f, 0.80f, 0.92f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	isGPUProcedural = true;
	quad.Draw();
	isGPUProcedural = false;
	curve->Draw();
	cyclist->Draw();
	glutSwapBuffers();

}

void onKeyboard(unsigned char key, int pX, int pY) {
	switch (key) {
	case ' ':
		camera.Zoom();
		camera.follow = true;
		break;
	}
	glutPostRedisplay();
}

void onKeyboardUp(unsigned char key, int pX, int pY) {

}

void onMouse(int button, int state, int pX, int pY) {
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
		float cX = 2.0f * pX / windowWidth - 1;
		float cY = 1.0f - 2.0f * pY / windowHeight;
		curve->AddControlPoint(cX, cY);
		glutPostRedisplay();
	}
}

void onMouseMotion(int pX, int pY) {

}

static void IdleFunc() {
	static float tend = 0.0f;
	const float dt = 0.01f;
	float tstart = tend;
	tend = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
	for (float t = tstart; t < tend; t += dt) {
		float Dt = fmin(dt, tend - t) / 100.0f;
		cyclist->SetPosition(Dt);
		cyclist->CalculateCircle();
		if (camera.follow)
			camera.Follow(cyclist->GetPosition());
	}
}

void onIdle() {
	IdleFunc();
	glutPostRedisplay();
}