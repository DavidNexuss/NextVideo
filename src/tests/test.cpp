#include "../engine/engine.hpp"

using namespace GLEngine;
void initScene(Scene* scene) {
  scene->addTexture("assets/envMap.jpg");
  scene->addMaterial()->albedoTexture =
    scene->addTexture("assets/checker.png");

  auto plane         = scene->currentStage()->addObject();
  auto planeInstance = scene->currentStage()->addObjectGroup();

  plane->material       = 0;
  plane->mesh           = 0;
  planeInstance->object = plane;
  planeInstance->transforms.push_back(glm::mat4(1.0f));

  scene->currentStage()->skyTexture = 0;
  scene->currentStage()->camPos     = glm::vec3(0, 1, 0);
  scene->currentStage()->camDir     = glm::vec3(0, -1, 0);
}

void updateScene(Scene* scene, RendererInput input) {
  glm::vec3 velocity = glm::vec3(0, 0, 0);
  velocity.x += input.keyboard['a'] - input.keyboard['d'];
  velocity.y += input.keyboard['w'] - input.keyboard['s'];
  velocity.z += input.keyboard['w'] - input.keyboard['s'];

  scene->currentStage()->camPos += velocity;
  scene->currentStage()->camDir.x = cos(input.x);
  scene->currentStage()->camDir.z = sin(input.x);
  scene->currentStage()->camDir.y = cos(input.y);
}

int main() {

  RendererDesc desc;
  desc.width                   = 800;
  desc.height                  = 600;
  desc.flag_hdr                = 1;
  desc.flag_bloom              = 1;
  desc.flag_useDefferedShading = 0;
  IRenderer* renderer          = rendererCreate(desc);

  Scene* scene = sceneCreate();
  initScene(scene);
  renderer->upload(scene);
  do {
    renderer->render(scene);
    updateScene(scene, renderer->input());
  } while (renderer->pollEvents());
}
