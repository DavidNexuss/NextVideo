#include "../engine/engine.hpp"

using namespace GLEngine;
void initScene(Scene* scene) {
  scene->addTexture("assets/envMap.jpg");
  scene->currentStage()->skyTexture = 0;
}

int main() {

  RendererDesc desc;
  desc.width                   = 800;
  desc.height                  = 600;
  desc.flag_hdr                = 0;
  desc.flag_bloom              = 0;
  desc.flag_useDefferedShading = 0;
  IRenderer* renderer          = rendererCreate(desc);

  Scene* scene = sceneCreate();
  initScene(scene);
  renderer->upload(scene);
  do {
    renderer->render(scene);
  } while (renderer->pollEvents());
}
