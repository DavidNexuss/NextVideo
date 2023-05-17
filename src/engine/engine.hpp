#include <glm/glm.hpp>

/* STD CONTAINERS */
#include <vector>
#include <unordered_map>

#define ENGINE_API
#define DEBUG

#ifdef DEBUG
#  include <stdio.h>
#  define ERROR(...) dprintf(2, __VA_ARGS__)
#  define LOG(...)   dprintf(2, __VA_ARGS__)
#  define VERIFY(X, ...)                                             \
    if (!(X)) {                                                      \
      dprintf(2, "%d [ASSERT] ### Assert error " #X ": ", __LINE__); \
      dprintf(2, __VA_ARGS__);                                       \
      fflush(stderr);                                                \
      exit(1);                                                       \
    }

#else
#  define ERROR(...)
#  define LOG(...)
#  define VERIFY(X, ...)
#endif


template <typename T>
bool valid(const std::vector<T>& container, int index) { return index >= 0 && index < container.size(); }

template <typename T>
struct idx_ptr {

  idx_ptr(int _index, std::vector<T>* _container) {
    index     = _index;
    container = _container;
  }

  inline T* operator->() {
    VERIFY(index >= 0 && index < container->size(), "Invalid index pointer\n");
    return &(*container)[index];
  }
  inline operator int() {
    VERIFY(index >= 0 && index < container->size(), "Invalid index pointer\n");
    return index;
  }

  private:
  int             index;
  std::vector<T>* container;
};
/* USER API INTERFACE */
namespace GLEngine {

struct Texture {
  int   width;
  int   height;
  int   channels;
  bool  useNearest;
  bool  useMipmapping;
  void* data;
};

static const int MESH_FORMAT_SIZE[] = {
  8 /* POSITION NORMAL UV */
};

static int MESH_FORMAT_DEFAULT            = 0;
static int MESH_FORMAT_POSITION_NORMAL_UV = 0;
static int MESH_FORMAT_LAST               = 1;

enum MeshType { CUSTOM };

struct Material {
  //PBR PIPELINE
  glm::vec3 albedo;
  glm::vec3 emission;
  float     roughness;
  float     metallic;
  float     fresnel;

  int albedoTexture;
  int roughnessTexture;
  int metallicTexture;
  int normalTexture;
  int emissionTexture;

  // OLD PIPELINE

  glm::vec3 kd;
  glm::vec3 ka;
  glm::vec3 ks;
  float     shinness;

  int specularTexture;
};

struct Mesh {

  struct {
    float*        vertexBuffer;
    unsigned int* indexBuffer;
    int           numVertices;
    int           numIndices;
    int           meshFormat;
  } tCustom;

  MeshType type;
};

struct Object {
  int mesh;
  int material;

  std::vector<int> meshLOD;
};

struct ObjectInstanceGroup {
  int                    object;
  std::vector<glm::mat4> transforms;
};

struct Stage {
  std::vector<Object>              objects;
  std::vector<ObjectInstanceGroup> instances;

  int       skyTexture;
  glm::vec3 camPos;
  glm::vec3 camDir;

  Stage() {
    skyTexture = -1;
  }

  inline idx_ptr<Object> addObject() {
    objects.emplace_back();
    return idx_ptr<Object>(objects.size() - 1, &objects);
  }

  inline idx_ptr<ObjectInstanceGroup> addObjectGroup() {
    instances.emplace_back();
    return idx_ptr<ObjectInstanceGroup>(instances.size() - 1, &instances);
  }
};

struct Scene {
  std::vector<Texture>  textures;
  std::vector<Mesh>     meshes;
  std::vector<Stage>    stages;
  std::vector<Material> materials;
  int                   _currentStage;

  Scene() {
    addStage();
    _currentStage = 0;
  }

  int addTexture(const char* path);

  inline idx_ptr<Material> addMaterial() {
    materials.emplace_back();
    return idx_ptr<Material>(materials.size() - 1, &materials);
  }
  inline idx_ptr<Material> addStandardMaterial() {
    return addMaterial();
  }
  inline idx_ptr<Material> addOldMaterial() {
    return addMaterial();
  }

  inline idx_ptr<Stage> addStage() {
    stages.emplace_back();
    return idx_ptr<Stage>(stages.size() - 1, &stages);
  }

  inline idx_ptr<Mesh> addMesh() {
    meshes.emplace_back();
    return idx_ptr<Mesh>(meshes.size() - 1, &meshes);
  }

  Stage* currentStage() {
    VERIFY(_currentStage < stages.size(), "Invalid current stage\n");
    return &stages[_currentStage];
  }

  void setCurrentStage(int index) { _currentStage = index; }
};

typedef struct _ {
  int width                   = 640;
  int height                  = 480;
  int flag_useDefferedShading = 0;
  int flag_bloom              = 0;
  int flag_hdr                = 0;
  int gauss_passes            = 1;
} RendererDesc;


struct RendererInput {
  int*  keyboard;
  float x;
  float y;
};

struct IRenderer {
  virtual void          render(Scene* scene) = 0;
  virtual void          upload(Scene* scene) = 0;
  virtual int           pollEvents()         = 0;
  virtual RendererInput input()              = 0;
  ~IRenderer();
};

ENGINE_API Scene*     sceneCreate();
ENGINE_API IRenderer* rendererCreate(RendererDesc desc);
} // namespace GLEngine
