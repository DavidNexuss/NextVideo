#include "../engine/engine.hpp"
#include "../include/implot.h"
#include "../engine/gl.hpp"
using namespace NextVideo;

#define C                  299792458.0
#define LIGHT_DECAY_FACTOR 1.0e-5
#define INTEGRATION_ENABLED
#define INTEGRATION_STEPS 4
#define GAMMA_CORRECTED   1.0
#define A_WAVE            5000e-10
#define A_SEPARATION      0.01e-3
#define A_L               = 200.0e-3
#define ZOOM              1e-4
#define TIME_ZOOM         (1e-6 / C)

int NCOUNT = 5;
#define N NCOUNT
bool  LIGHT_DECAY_ENABLED  = false;
float LIGHT_DECAY_EXPONENT = 0.00002;
float plotting_distance    = 500e-3;

#define B_SEPARATION 0.001e-3
#define C_SEPARATION 0.001e-3
/* CPU BACKEND */
using namespace glm;

#define LAMBDA 5000e-10

float lightValue(vec2 st) {
  return pow(0.1, LIGHT_DECAY_EXPONENT) / sqrt(st.x * st.x + st.y * st.y);
}

float light(vec2 st, float t) {
  float l = length(vec3(st.x, st.y, 0));
  float k = 2.0 * M_PI / LAMBDA;
  float f = C / LAMBDA;
  float w = f * 2.0 * M_PI;

  float value = (sin(l * k - t * w) * 0.5 + 0.5);
  if (LIGHT_DECAY_ENABLED) return value * lightValue(st);
  return value;
}


float net(vec2 st, float off, float t, float separation) {
  float result = 0.0;
  separation   = 10.0 * separation / float(N);
  float offset = -float(N) * separation * 0.5 + off;
  for (int i = 0; i < N; i++) {
    result += light(st + vec2(0, offset), t);
    offset += separation;
  }
  return result / float(N);
}

float experimentA(vec2 st, float t) {
  return light(st + vec2(0.0, -A_SEPARATION * 0.5), t) * 0.5 + light(st + vec2(0.0, A_SEPARATION * 0.5), t) * 0.5;
}
float experimentB(vec2 st, float t) {
  return net(st, 0.0, t, B_SEPARATION);
}

typedef float (*experiment_t)(vec2 st, float t);

#define C_SEPARATION 0.001e-3

float experimentC(vec2 st, float t) {
  return net(st, 0.0, t, C_SEPARATION);
}

float experimentD(vec2 st, float t) {
  float o = 0.1e-3;
  return net(st, -o / 2.0, t, C_SEPARATION) * 0.5 + net(st, o / 2.0, t, C_SEPARATION) * 0.5;
}

float integrate(vec2 st, float tP, experiment_t func) {

  float result = 0.0;
  float f      = C / A_WAVE;
  float w      = f * 2.0 * M_PI;
  float L      = float(INTEGRATION_STEPS);
  float dt     = 2.0 * M_PI / (L * w);
  float t      = 0.0;
  for (int i = 0; i < INTEGRATION_STEPS; i++) {
    float partial = func(st, t + tP);
    result += partial * partial;
    t += dt;
  }
  return result / L;
}

struct PlotResult {
  std::vector<float> y;
  std::vector<float> x;
};
PlotResult plot(experiment_t func) {
  float x     = plotting_distance;
  float dy    = 10.0e-7;
  int   count = 4000;

  float      current = -dy * count / 2;
  PlotResult res;
  for (int i = 0; i < count; i++) {
    res.y.push_back(integrate(glm::vec2(x, current), 0.0, func));
    res.x.push_back(current);
    current += dy;
  }
  return res;
}


/* GL CODE */
/* GL CALLBACKS*/
void messageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
  fprintf(stdout, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n", (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);
}

GLuint program;
GLuint iTime;
GLuint iZoom;
GLuint iResolution;
GLuint iIntegrationMode;
GLuint iDecayMode;
GLuint iDecayExponent;
GLuint iExperimentSelector;
GLuint iDistance;
GLuint iN;

// Uniforms
float uDistance   = 0.0;
float uZoom       = 1.0;
float uTime       = 0.0;
int   uExperiment = 0;
bool  uIntegration;

bool experimentPractica = true;

experiment_t currentExperiment() {
  if (uExperiment == 0) return experimentA;
  if (uExperiment == 1) return experimentB;
  if (uExperiment == 2) return experimentC;
  if (uExperiment >= 3) return experimentD;
}

void init() {
  program             = glUtilLoadProgram("assets/filter.vs", "assets/fdm.glsl");
  iTime               = glGetUniformLocation(program, "iTime");
  iZoom               = glGetUniformLocation(program, "iZoom");
  iResolution         = glGetUniformLocation(program, "iResolution");
  iIntegrationMode    = glGetUniformLocation(program, "iIntegrationMode");
  iDecayMode          = glGetUniformLocation(program, "iDecayMode");
  iDecayExponent      = glGetUniformLocation(program, "iDecayExponent");
  iExperimentSelector = glGetUniformLocation(program, "iExperimentSelector");
  iN                  = glGetUniformLocation(program, "N");
  iDistance           = glGetUniformLocation(program, "iDistance");
}

NextVideo::ISurface* surface;


void uiRender() {
  if (ImGui::Begin("Simulation parameters")) {
    ImGui::Text("Simulation types");
    ImGui::Checkbox("Lab Experiments", &experimentPractica);

    ImGui::Separator();
    ImGui::Text("Simulation parameters");
    ImGui::Checkbox("Use light decay", &LIGHT_DECAY_ENABLED);
    ImGui::Checkbox("Integration", &uIntegration);
    ImGui::SliderFloat("Light decay exponent", &LIGHT_DECAY_EXPONENT, 1.0, 10.0);
    ImGui::SliderFloat("Zoom", &uZoom, 0.01, 10.0);
    ImGui::SliderFloat("Time", &uTime, 0.01, 10.0);
    ImGui::SliderInt("N", &NCOUNT, 2, 50);
    ImGui::InputFloat("Distance", &uDistance);
    ImGui::End();
  }


  if (experimentPractica && ImGui::Begin("FDM LAB Paramaters")) {
    ImGui::Text("FDB Lab experiments tweak values");
    ImGui::SliderInt("Experiment", &uExperiment, 0, 3);
    ImGui::End();

    static bool showPlot;
    ImGui::Checkbox("Show integration plot", &showPlot);

    if (showPlot) {
      ImGui::SliderFloat("Screen distance", &plotting_distance, 0.0, 200.0e-4);
      static bool currentPlot = 0;
      auto        data        = plot(currentExperiment());

      static bool normalizeData = false;

      ImGui::Checkbox("Normalize data", &normalizeData);

      if (normalizeData) {
        float minVal = 10e50;
        float maxVal = -10e50;

        for (int i = 0; i < data.y.size(); i++) {
          maxVal = std::max(data.y[i], maxVal);
          minVal = std::min(data.y[i], minVal);
        }

        ImGui::Text("Min value %f\n", minVal);
        ImGui::Text("Max value %f\n", maxVal);


        for (int i = 0; i < data.y.size(); i++) {
          data.y[i] = (data.y[i] - minVal) / (maxVal - minVal);
        }
      }

      if (ImPlot::BeginPlot("My Plot", "Distancia en X", "Intensitat llum", ImVec2(1500, 600))) {
        ImPlot::PlotLine("Integration", data.x.data(), data.y.data(), data.x.size());
        ImPlot::EndPlot();
      }
    }
  }
}
void render() {
  glViewport(0, 0, surface->getWidth(), surface->getHeight());
  glUseProgram(program);
  glUniform1f(iTime, uTime);
  glUniform1f(iZoom, uZoom);
  glUniform2f(iResolution, surface->getWidth(), surface->getHeight());
  glUniform1i(iIntegrationMode, uIntegration);
  glUniform1i(iDecayMode, LIGHT_DECAY_ENABLED);
  glUniform1f(iDecayExponent, LIGHT_DECAY_EXPONENT);
  glUniform1i(iExperimentSelector, uExperiment);
  glUniform1f(iDistance, uDistance);
  glUniform1i(iN, NCOUNT);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

/* MAIN CODE */
int main() {
  SurfaceDesc desc;
  desc.width  = 1920;
  desc.height = 1080;
  desc.online = true;
  surface     = NextVideo::surfaceCreate(desc);

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  init();
  ImPlot::CreateContext();
  do {
    if (surface->getWidth() > 0 && surface->getHeight() > 0) {
      glBindVertexArray(vao);
      render();
      glBindVertexArray(0);
      surface->beginUI();
      uiRender();
      surface->endUI();
    }
  } while (surface->update());
}
