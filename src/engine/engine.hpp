#include <glm/glm.hpp>
#include <string>

#define DEBUG
/* STD CONTAINERS */
#include "core.hpp"

/* USER API INTERFACE */
namespace NextVideo {

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

struct Texture {
  int   width;
  int   height;
  int   channels;
  bool  useNearest;
  bool  mipmapDisable;
  void* data;
};

static const int MESH_FORMAT_SIZE[] = {
  8 /* POSITION NORMAL UV */
};

static int MESH_FORMAT_DEFAULT            = 0;
static int MESH_FORMAT_POSITION_NORMAL_UV = 0;
static int MESH_FORMAT_LAST               = 1;

enum MeshType { CUSTOM };

struct Program {
  std::string vertexShader;
  std::string fragmentShader;
};

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

  glm::vec2 uvScale  = glm::vec2(1.0f);
  glm::vec2 uvOffset = glm::vec2(0.0f);
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

  bool program_special;
  int  program;
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

enum LightType {
  POINT,
  DIRECTIONAL,
  HEMI
};

struct Light {
  glm::vec3 direction;
  glm::vec3 position;
  float     radius;
  LightType type;
};
struct SunLight {
};
struct Stage {
  std::vector<Object>              objects;
  std::vector<ObjectInstanceGroup> instances;
  std::vector<Light>               lights;
  std::vector<Program>             programs;

  SunLight sunLight;

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


struct SurfaceInput {
  int*  keyboard;
  float x;
  float y;
};

struct SurfaceExtensions {
  const char** names;
  unsigned int count;
};

struct SurfaceDesc {
  int  width  = 800;
  int  height = 600;
  bool online = true;
};

struct ISurface {
  virtual void*             native()        = 0;
  virtual SurfaceExtensions getExtensions() = 0;
  virtual SurfaceInput      getInput()      = 0;
  virtual int               update()        = 0;
  virtual ~ISurface() {}


  virtual inline int   getWidth() const  = 0;
  virtual inline int   getHeight() const = 0;
  virtual inline bool  resized() const   = 0;
  virtual inline float ra() const { return getWidth() / float(getHeight()); }

  inline SurfaceDesc& getDesc() { return desc; }
  inline bool         isOnline() { return desc.online; }

  protected:
  SurfaceDesc desc;
};


typedef struct _RendererDesc {

  // The renderer implementation may or may not obey this flags
  int       deferred_enable         = 0;
  int       hdr_enable              = 0;
  float     bloom_radius            = 1.5f;
  int       bloom_sampling          = 2;
  bool      bloom_enable            = 0;
  bool      shadowmapping_enable    = 0;
  int       shadowmapping_width     = 1024;
  int       shadowmapping_height    = 1024;
  bool      ssao_enable             = 0;
  bool      parallaxmapping_enable  = 0;
  bool      texture_mipmap_enable   = 1;
  bool      depth_enable            = 1;
  bool      backface_culling_enable = 1;
  bool      cull_back_face          = 1;
  ISurface* surface                 = nullptr;
} RendererDesc;

struct IRenderer {
  virtual void render(Scene* scene) = 0;
  virtual void upload(Scene* scene) = 0;
  virtual ~IRenderer() {}

  inline RendererDesc& desc() { return _desc; }

  protected:
  RendererDesc _desc;
};

struct RendererBackendDefaults {
  bool glfw_noApi = false;
};

ENGINE_API RendererBackendDefaults rendererDefaults();
ENGINE_API Scene*                  sceneCreate();
ENGINE_API IRenderer*              rendererCreate(RendererDesc desc);
ENGINE_API ISurface*               surfaceCreate(SurfaceDesc desc);
} // namespace NextVideo
