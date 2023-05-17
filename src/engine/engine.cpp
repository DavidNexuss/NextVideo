#include "engine.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define VERIFY_FRAMEBUFFER                                                               \
  do {                                                                                   \
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);                            \
    VERIFY(status == GL_FRAMEBUFFER_COMPLETE, "Framebuffer not complete! %d\n", status); \
  } while (0)

#include <stb/stb_image.h>

#include <glm/ext.hpp>
#include <stdio.h>
#define MAX_OBJECTS 512
#include <GL/glew.h>
// Include
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace GLEngine {



int Scene::addTexture(const char* path) {
  Texture text;
  text.data = stbi_load(path, &text.width, &text.height, &text.channels, 3);
  VERIFY(text.data != nullptr, "[IO] Error trying to load texture %s\n", path);
  textures.push_back(text);
  return textures.size() - 1;
}

/* LIENAR */

namespace lin {
ENGINE_API GLfloat* id() {
  static GLfloat id[] = {
    1, 0, 0, 0, /*x */
    0, 1, 0, 0, /*y */
    0, 0, 1, 0, /*z*/
    0, 0, 0, 1, /*w*/
  };
  return id;
}

ENGINE_API glm::mat4 rotate(glm::vec3 axis, float amount) {
  return glm::rotate(glm::mat4(1.0f), amount, axis);
}
ENGINE_API glm::mat4 scale(glm::vec3 scale) {
  return glm::scale(glm::mat4(1.0f), scale);
}

ENGINE_API glm::mat4 scale(float scale) {
  return glm::scale(glm::mat4(1.0f), glm::vec3(scale));
}

ENGINE_API glm::mat4 translate(float x, float y, float z) {
  return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
}

ENGINE_API float* meshTransformPlaneScreen() {
  static glm::mat4 plane = rotate(glm::vec3(1, 0, 0), -M_PI / 2.0f) * scale(2.0) * translate(-0.5, 0, -0.5);
  return &plane[0][0];
}

ENGINE_API float* viewMatrix(glm::vec3& camPos, glm::vec3& camDir) {
  static glm::mat4 view;
  view = glm::lookAt(camPos, camPos + camDir, glm::vec3(1, 0, 0));
  return &view[0][0];
}
ENGINE_API float* projMatrix(float ra) {
  static glm::mat4 proj;
  proj = glm::perspective(float(M_PI) / 2.0f, ra, 0.5f, 2000.0f);
  return &proj[0][0];
}
} // namespace lin

/* SCENE LOADING */

ENGINE_API Scene* sceneCreate() {

  Scene* scene = new Scene;
  auto   plain = scene->addMesh();

  static float plainMesh[] = {
    0, 0, 0, /* POSITION*/ 0, 1, 0, /* NORMAL */ 0, 0, /* UV */
    0, 0, 1, /* POSITION*/ 0, 1, 0, /* NORMAL */ 0, 1, /* UV */
    1, 0, 1, /* POSITION*/ 0, 1, 0, /* NORMAL */ 1, 1, /* UV */
    1, 0, 0, /* POSITION*/ 0, 1, 0, /* NORMAL */ 1, 0, /* UV */
  };

  static unsigned int plainMeshEBO[] = {0, 1, 2, 2, 3, 0};

  plain->type                 = CUSTOM;
  plain->tCustom.meshFormat   = 0;
  plain->tCustom.numVertices  = 4;
  plain->tCustom.numIndices   = 6;
  plain->tCustom.vertexBuffer = plainMesh;
  plain->tCustom.indexBuffer  = plainMeshEBO;
  return scene;
}
namespace SceneLoader {

struct LoaderCache {
  std::unordered_map<int, int>         meshCache;
  std::unordered_map<std::string, int> textureCache;
  std::unordered_map<int, int>         materialCache;

  void clear() {
    meshCache.clear();
    textureCache.clear();
    materialCache.clear();
  }
};

LoaderCache    cache;
ENGINE_API int processTexture(const std::string& path, Scene* tracerScene) {
  auto it = cache.textureCache.find(path);
  if (it != cache.textureCache.end()) return it->second;
  int texture = tracerScene->addTexture(path.c_str());

  LOG("[LOADER] Texture created %d\n", texture);
  return cache.textureCache[path] = texture;
}
ENGINE_API int processMaterial(int materialIdx, const aiScene* scene, Scene* tracerScene) {
  auto it = cache.materialCache.find(materialIdx);
  if (it != cache.materialCache.end()) return it->second;

  aiMaterial* mat = scene->mMaterials[materialIdx];


  auto traceMaterialPtr = tracerScene->addMaterial();

  mat->Get(AI_MATKEY_COLOR_DIFFUSE, traceMaterialPtr->kd);
  mat->Get(AI_MATKEY_COLOR_AMBIENT, traceMaterialPtr->ka);
  mat->Get(AI_MATKEY_COLOR_SPECULAR, traceMaterialPtr->ks);

  static aiTextureType types[] = {aiTextureType_DIFFUSE, aiTextureType_SPECULAR};
  //TODO: Magic number
  for (int type = 0; type < 2; type++) {
    for (int i = 0; i < mat->GetTextureCount(types[type]); i++) {
      aiString path;
      mat->GetTexture(types[type], i, &path);
      switch (type) {
        case 0: traceMaterialPtr->albedoTexture = processTexture(path.C_Str(), tracerScene); break;
        case 1: traceMaterialPtr->specularTexture = processTexture(path.C_Str(), tracerScene); break;
      }
    }
  }

  LOG("[LOADER] Material created %d\n", (int)traceMaterialPtr);
  return cache.materialCache[materialIdx] = (int)traceMaterialPtr;
}
ENGINE_API int processMesh(int meshIdx, const aiScene* scene, Scene* tracerScene) {

  auto it = cache.meshCache.find(meshIdx);
  if (it != cache.meshCache.end()) return it->second;

  aiMesh* mesh = scene->mMeshes[meshIdx];

  int vbo_count = mesh->mNumVertices;

  float* vbo_data = (float*)malloc(MESH_FORMAT_SIZE[MESH_FORMAT_DEFAULT] * sizeof(float) * vbo_count);

  int t = 0;
  for (int i = 0; i < vbo_count; i++) {
    vbo_data[t++] = mesh->mVertices[i].x;
    vbo_data[t++] = mesh->mVertices[i].y;
    vbo_data[t++] = mesh->mVertices[i].z;

    vbo_data[t++] = mesh->mNormals[i].x;
    vbo_data[t++] = mesh->mNormals[i].y;
    vbo_data[t++] = mesh->mNormals[i].z;

    if (mesh->mTextureCoords[0] != 0) {
      vbo_data[t++] = mesh->mTextureCoords[0][i].x;
      vbo_data[t++] = mesh->mTextureCoords[0][i].y;
    } else {
      vbo_data[t++] = 0;
      vbo_data[t++] = 0;
    }
  }
  if (t != (vbo_count * MESH_FORMAT_SIZE[MESH_FORMAT_DEFAULT])) {
    LOG("VBO Buffer overflow\n");
    exit(1);
  }

  int ebo_count = 0;
  for (int i = 0; i < mesh->mNumFaces; i++) { ebo_count += mesh->mFaces[i].mNumIndices; }

  unsigned int* ebo_data = (unsigned int*)malloc(sizeof(unsigned int) * ebo_count);

  t = 0;
  for (int i = 0; i < mesh->mNumFaces; i++) {
    for (int j = 0; j < mesh->mFaces[i].mNumIndices; j++) {
      ebo_data[t++] = mesh->mFaces[i].mIndices[j];
    }
  }

  if (t != (ebo_count)) {
    LOG("EBO Buffer overflow\n");
    exit(1);
  }

  auto ptrMeshTracer = tracerScene->addMesh();

  ptrMeshTracer->type                 = CUSTOM;
  ptrMeshTracer->tCustom.numIndices   = ebo_count;
  ptrMeshTracer->tCustom.numVertices  = vbo_count;
  ptrMeshTracer->tCustom.vertexBuffer = vbo_data;
  ptrMeshTracer->tCustom.indexBuffer  = ebo_data;

  LOG("[LOADER] Mesh created %d\n", (int)ptrMeshTracer);
  return cache.meshCache[meshIdx] = (int)ptrMeshTracer;
}


ENGINE_API glm::mat4 toMat(aiMatrix4x4& n) {
  glm::mat4 mat;
  mat[0] = glm::vec4(n.a1, n.a2, n.a3, n.a4);
  mat[1] = glm::vec4(n.b1, n.b2, n.b3, n.b4);
  mat[2] = glm::vec4(n.c1, n.c2, n.c3, n.c4);
  mat[3] = glm::vec4(n.d1, n.d2, n.d3, n.d4);
  return mat;
}

ENGINE_API void processNode(aiNode* node, const aiScene* scene, Scene* tracerScene, aiMatrix4x4 parentTransform) {

  aiMatrix4x4 nodeTransform = parentTransform * node->mTransformation;

  Stage* stage = tracerScene->currentStage();
  for (unsigned int i = 0; i < node->mNumMeshes; i++) {
    int meshTracer    = processMesh(node->mMeshes[i], scene, tracerScene);
    int materialTrace = processMaterial(scene->mMeshes[node->mMeshes[i]]->mMaterialIndex, scene, tracerScene);

    aiVector3t<float> pos, scale, rot;
    node->mTransformation.Decompose(scale, rot, pos);

    auto obj = stage->addObject();

    obj->mesh     = meshTracer;
    obj->material = materialTrace;

    auto instanceTracer = stage->addObjectGroup();

    instanceTracer->object = obj;
    instanceTracer->transforms.push_back(lin::translate(pos.x, pos.y, pos.z));

    LOG("[LOADER] Object created %d\n", (int)obj);
  }
  // then do the same for each of its children
  for (unsigned int i = 0; i < node->mNumChildren; i++) {
    processNode(node->mChildren[i], scene, tracerScene, nodeTransform);
  }
}

ENGINE_API int _sceneLoadOBJ(const char* path, Scene* scene) {
  cache.clear();
  Assimp::Importer importer;
  const aiScene*   scn = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
  if (!scn || scn->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scn->mRootNode) {
    LOG("[LOADER] Error processing mesh\n");
    return 1;
  }
  aiMatrix4x4 id;
  id.a1 = 1;
  id.b2 = 1;
  id.c3 = 1;
  id.d4 = 1;
  processNode(scn->mRootNode, scn, scene, id);
  LOG("[LOADER] Mesh processing successful\n");
  return 0;
}
} // namespace SceneLoader


/* UTIL FUNCTIONS */

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
ENGINE_API const char* readFile(const char* path) {
  struct stat _stat;
  stat(path, &_stat);

  if (_stat.st_size <= 0)
    return 0;

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return 0;
  }

  char* buffer          = (char*)malloc(_stat.st_size + 1);
  buffer[_stat.st_size] = 0;

  int current = 0;
  int size    = _stat.st_size;
  int step    = 0;

  while ((step = read(fd, &buffer[current], size - current))) {
    current += step;
  }

  return buffer;
}

struct Window {
  void* windowPtr;
  int   width;
  int   height;
  bool  needsUpdate;
  float x;
  float y;
  float ra;
  int   keyboard[512];
};

/* GL CALLBACKS*/
ENGINE_API void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
  fprintf(stdout,
          "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
          (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity,
          message);
}

/* GLFW CALLBACKS */
ENGINE_API void window_size_callback(GLFWwindow* window, int width, int height) {
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {

  auto* ptr = (Window*)glfwGetWindowUserPointer(window);

  glViewport(0, 0, width, height);
  ptr->width  = width;
  ptr->height = height;

  ptr->ra          = ptr->width / (float)(ptr->height);
  ptr->needsUpdate = 1;
}
ENGINE_API void cursor_position_callback(GLFWwindow* window, double x, double y) {
  auto* ptr = (Window*)glfwGetWindowUserPointer(window);
  ptr->x    = x / (float)ptr->width;
  ptr->y    = y / (float)ptr->height;

  ptr->x -= 0.5;
  ptr->y -= 0.5;

  ptr->x *= 2;
  ptr->y *= 2;
}
ENGINE_API void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {}
ENGINE_API void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  auto* ptr          = (Window*)glfwGetWindowUserPointer(window);
  ptr->keyboard[key] = (action == GLFW_PRESS) || (action == GLFW_REPEAT);
}

ENGINE_API Window* windowCreate(int width, int height) {

  if (glfwInit() != GLFW_TRUE) {
    ERROR("Failed to start GLFW .\n");
    return 0;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow* window =
    glfwCreateWindow(width, height, "PathTracing", NULL, NULL);

  if (window == NULL) {
    ERROR("Failed to open GLFW window. width: %d height: %d\n", width, height);
    return 0;
  }

  glfwMakeContextCurrent(window);
  if (glewInit() != GLEW_OK) {
    ERROR("Failed to initialize glew");
    return 0;
  }

  glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
  glfwSetWindowSizeCallback(window, window_size_callback);
  glfwSetCursorPosCallback(window, cursor_position_callback);
  glfwSetScrollCallback(window, scroll_callback);
  glfwSetKeyCallback(window, key_callback);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

  LOG("Window created with %d %d\n", width, height);

  Window* windowInstance      = new Window;
  windowInstance->windowPtr   = window;
  windowInstance->needsUpdate = true;
  glfwSetWindowUserPointer(window, windowInstance);
  return windowInstance;
}

ENGINE_API void windowDestroy(Window* window) {}


/* GL_UTIL_FUNCTIONS */

ENGINE_API void glUtilRenderScreenQuad() {
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
}

ENGINE_API void glUtilsSetVertexAttribs(int index) {

  int stride = MESH_FORMAT_SIZE[index] * sizeof(float);

  if (index == 0) {
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
  }
}

ENGINE_API void glUtilRenderQuad(GLuint vbo, GLuint ebo, GLuint worldMat, GLuint viewMat, GLuint projMat) {
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glUtilsSetVertexAttribs(0);
  glUniformMatrix4fv(worldMat, 1, 0, lin::meshTransformPlaneScreen());
  glUniformMatrix4fv(viewMat, 1, 0, lin::id());
  glUniformMatrix4fv(projMat, 1, 0, lin::id());
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
}

ENGINE_API GLuint glUtilLoadProgram(const char* vs, const char* fs) {

  char errorBuffer[2048];

  GLuint VertexShaderID   = glCreateShader(GL_VERTEX_SHADER);
  GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

  GLint Result = GL_FALSE;
  int   InfoLogLength;

  const char* VertexSourcePointer   = readFile(vs);
  const char* FragmentSourcePointer = readFile(fs);

  if (VertexSourcePointer == 0) {
    ERROR("Error reading vertex shader %s \n", vs);
    return -1;
  }

  if (FragmentSourcePointer == 0) {
    ERROR("Error reading fragment shader %s \n", fs);
    return -1;
  }

  glShaderSource(VertexShaderID, 1, &VertexSourcePointer, NULL);
  glCompileShader(VertexShaderID);

  // Check Vertex Shader
  glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);

  if (InfoLogLength > 0) {
    glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, errorBuffer);
    ERROR("Error loading vertex shader %s : \n%s\n", vs, errorBuffer);
    free((void*)VertexSourcePointer);
    free((void*)FragmentSourcePointer);
    return 0;
  }

  glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer, NULL);
  glCompileShader(FragmentShaderID);

  // Check Fragment Shader
  glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);

  if (InfoLogLength > 0) {
    glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, errorBuffer);
    ERROR("Error loading fragment shader %s : \n%s\n", fs, errorBuffer);
    free((void*)VertexSourcePointer);
    free((void*)FragmentSourcePointer);
    return 0;
  }

  GLuint ProgramID = glCreateProgram();
  glAttachShader(ProgramID, VertexShaderID);
  glAttachShader(ProgramID, FragmentShaderID);
  glLinkProgram(ProgramID);

  // Check the program
  glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
  glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);

  if (InfoLogLength > 0) {
    glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, errorBuffer);
    free((void*)VertexSourcePointer);
    free((void*)FragmentSourcePointer);
    return 0;
  }

  glDetachShader(ProgramID, VertexShaderID);
  glDetachShader(ProgramID, FragmentShaderID);

  glDeleteShader(VertexShaderID);
  glDeleteShader(FragmentShaderID);

  free((void*)VertexSourcePointer);
  free((void*)FragmentSourcePointer);

  LOG("[RENDERER] Program loaded successfully %s\n", fs);
  return ProgramID;
}

/* Renderer begin */
/* Engine Default Variables */
static int PARAM_BLOOM_CHAIN_LENGTH = 5;

static int BUFF_START_USER       = 0;
static int BUFF_PLAIN            = 0;
static int TEXT_STD              = 0;
static int TEXT_IBL              = 1;
static int TEXT_ATTACHMENT_COLOR = 2;
static int TEXT_ATTACHMENT_BLOOM = 3;
static int TEXT_GAUSS_RESULT0    = 4;
static int TEXT_GAUSS_RESULT02   = 5;
static int TEXT_GAUSS_RESULT1    = 6;
static int TEXT_GAUSS_RESULT12   = 7;
static int TEXT_GAUSS_RESULT2    = 8;
static int TEXT_GAUSS_RESULT22   = 9;
static int TEXT_GAUSS_RESULT3    = 10;
static int TEXT_GAUSS_RESULT32   = 11;
static int TEXT_BLOOM_START      = 12;
static int TEXT_BLOOM_END        = TEXT_BLOOM_START + PARAM_BLOOM_CHAIN_LENGTH;
static int TEXT_END              = TEXT_BLOOM_END;
static int TEXT_START_USER       = TEXT_END + 1;
static int FBO_HDR_PASS          = 0;
static int FBO_GAUSS_PASS_PING   = 1;
static int FBO_GAUSS_PASS_PONG   = 2;
static int FBO_BLOOM             = 3;
static int FBO_START_USER        = 4;
static int RBO_HDR_PASS_DEPTH    = 0;

#define UNIFORMLIST_HDR(o)        o(u_color) o(u_bloom)
#define UNIFORMLIST_FILTER(o)     o(u_input)
#define UNIFORMLIST_GAUSS(o)      o(u_input) o(u_horizontal)
#define UNIFORMLIST_UPSAMPLE(o)   o(srcTexture) o(filterRadius)
#define UNIFORMLIST_DOWNSAMPLE(o) o(srcTexture) o(srcResolution)

#define UNIFORMLIST(o)                                                  \
  o(u_envMap) o(u_diffuseTexture) o(u_specularTexture) o(u_bumpTexture) \
    o(u_kd) o(u_ka) o(u_ks) o(u_shinnness) o(u_ro) o(u_rd) o(u_isBack)  \
      o(u_shadingMode) o(u_useTextures) o(u_ViewMat) o(u_ProjMat)       \
        o(u_WorldMat) o(u_flatUV)

struct Renderer : public IRenderer {

  Window*      window;
  RendererDesc desc;

  /* GL Objects */
  GLuint              vao;
  std::vector<GLuint> textures;
  std::vector<GLuint> vbos;
  std::vector<GLuint> ebos;
  std::vector<GLuint> fbos;
  std::vector<GLuint> rbos;

  /* Default programs */
  GLuint programPbr;
  GLuint programGaussFilter;
  GLuint programUpsample;
  GLuint programDownsample;
  GLuint programPostHDR;

#define UNIFORM_DECL(u) GLuint pbr_##u;
  UNIFORMLIST(UNIFORM_DECL)
#undef UNIFORM_DECL

#define UNIFORM_DECL(u) GLuint hdr_##u;
  UNIFORMLIST_HDR(UNIFORM_DECL)
#undef UNIFORM_DECL

#define UNIFORM_DECL(u) GLuint filter_gauss_##u;
  UNIFORMLIST_GAUSS(UNIFORM_DECL)
#undef UNIFORM_DECL

#define UNIFORM_DECL(u) GLuint filter_upsample_##u;
  UNIFORMLIST_UPSAMPLE(UNIFORM_DECL)
#undef UNIFORM_DECL

#define UNIFORM_DECL(u) GLuint filter_downsample_##u;
  UNIFORMLIST_DOWNSAMPLE(UNIFORM_DECL)
#undef UNIFORM_DECL


  /* Debug checks */

  bool checkScene(Scene* scene) {
    for (Material& mat : scene->materials) {
      VERIFY(mat.albedoTexture < scene->textures.size(), "Invalid texture index\n");
      VERIFY(mat.normalTexture < scene->textures.size(), "Invalid texture index\n");
      VERIFY(mat.emissionTexture < scene->textures.size(), "Invalid texture index\n");
      VERIFY(mat.roughnessTexture < scene->textures.size(), "Invalid texture index\n");
    }

    return true;
  }

  int bindTexture(int textureSlot) {
    glActiveTexture(GL_TEXTURE0 + textureSlot);
    glBindTexture(GL_TEXTURE_2D, textures[textureSlot]);
    return textureSlot;
  }

  const inline static unsigned int attachments[] = {
    GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
    GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5,
    GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7};

  ENGINE_API void glUtilAttachScreenTexture(Renderer* renderer, int textureSlot, int attachment, GLenum type, GLenum format) {
    Window* window = renderer->window;
    if (window->needsUpdate) {

      int width  = window->width;
      int height = window->height;
      VERIFY(width > 0, "Width not valid!\n");
      VERIFY(width > 0, "Height not valid!\n");
      glActiveTexture(GL_TEXTURE0 + textureSlot);
      glBindTexture(GL_TEXTURE_2D, renderer->textures[textureSlot]);
      glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, type, GL_UNSIGNED_BYTE, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, renderer->textures[textureSlot], 0);
      LOG("[RENDERER] Generated screen texture for %d to attachment %d -> %d\n", textureSlot, attachment, renderer->textures[textureSlot]);
      glActiveTexture(GL_TEXTURE0);
    }
  }

  ENGINE_API void glUtilAttachRenderBuffer(Renderer* renderer, int rbo, int attachment, GLenum type) {
    Window* window = renderer->window;
    if (window->needsUpdate) {

      int width  = window->width;
      int height = window->height;
      VERIFY(width > 0, "Width not valid!\n");
      VERIFY(width > 0, "Height not valid!\n");
      glBindRenderbuffer(GL_RENDERBUFFER, renderer->rbos[rbo]);
      glRenderbufferStorage(GL_RENDERBUFFER, type, width, height);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, renderer->rbos[rbo]);
      LOG("[RENDERER] Generated render buffer for %d attachment %d\n", rbo, attachment);
    }
  }

  ENGINE_API void upload(Scene* scene) override {
    //Texture loading
    {
      const Texture* textureTable = scene->textures.data();
      for (int i = 0; i < scene->textures.size(); i++) {
        glActiveTexture(GL_TEXTURE0 + i + TEXT_START_USER);
        glBindTexture(GL_TEXTURE_2D, textures[i + TEXT_START_USER]);
        if (textureTable[i].useNearest) {
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        } else {
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        if (textureTable[i].useMipmapping) {
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        } else {
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }


        LOG("[RENDERER] Uploading texture [%d] width %d height %d channels %d\n", i, textureTable[i].width, textureTable[i].height, textureTable[i].channels);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureTable[i].width, textureTable[i].height, 0, GL_RGB, GL_UNSIGNED_BYTE, textureTable[i].data);
        if (textureTable[i].useMipmapping)
          glGenerateMipmap(GL_TEXTURE_2D);
      }
      glActiveTexture(GL_TEXTURE0);
    }

    //Buffer loading
    {
      for (int i = 0; i < scene->meshes.size(); i++) {
        Mesh* mesh = &scene->meshes[i];
        if (mesh->type == CUSTOM) {
          float*        vbo = mesh->tCustom.vertexBuffer;
          unsigned int* ebo = mesh->tCustom.indexBuffer;

          LOG("[RENDERER] Uploading mesh [%d] %p %p with %d %d to %d %d\n", BUFF_START_USER + i, mesh->tCustom.vertexBuffer, mesh->tCustom.indexBuffer, mesh->tCustom.numVertices, mesh->tCustom.numIndices, vbos[BUFF_START_USER + i], ebos[BUFF_START_USER + i]);

          glBindBuffer(GL_ARRAY_BUFFER, vbos[BUFF_START_USER + i]);
          glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebos[BUFF_START_USER + i]);

          VERIFY(mesh->tCustom.meshFormat >= MESH_FORMAT_DEFAULT && mesh->tCustom.meshFormat < MESH_FORMAT_LAST, "Invalid format %d", mesh->tCustom.meshFormat);
          glBufferData(GL_ARRAY_BUFFER, mesh->tCustom.numVertices * MESH_FORMAT_SIZE[mesh->tCustom.meshFormat] * sizeof(float), vbo, GL_STATIC_DRAW);
          glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->tCustom.numIndices * sizeof(unsigned int), ebo, GL_STATIC_DRAW);
        }
      }
    }
    LOG("[Renderer] Render upload completed.\n");
  }

  ENGINE_API ~Renderer() {

    Renderer* renderer = this;
    glDeleteTextures(renderer->textures.size(), renderer->textures.data());
    glDeleteBuffers(renderer->vbos.size(), renderer->vbos.data());
    glDeleteBuffers(renderer->ebos.size(), renderer->ebos.data());
    glDeleteFramebuffers(renderer->fbos.size(), renderer->fbos.data());
    glDeleteRenderbuffers(renderer->rbos.size(), renderer->rbos.data());

    windowDestroy(renderer->window);
    glfwTerminate();

    LOG("[Renderer] Render destroy completed.\n");
  }

  ENGINE_API void bindMaterial(Renderer* renderer, Material* mat) {

    glUniform1i(renderer->pbr_u_useTextures, mat->albedoTexture >= 0);
    if (mat->albedoTexture >= 0) {
      glUniform1i(renderer->pbr_u_diffuseTexture, mat->albedoTexture + TEXT_START_USER);
    } else {
      glUniform3f(renderer->pbr_u_kd, mat->albedo.x, mat->albedo.y, mat->albedo.z);
      glUniform3f(renderer->pbr_u_ks, mat->metallic, mat->roughness, mat->roughness);
    }
  }

  ENGINE_API int bindMesh(Renderer* renderer, Mesh* mesh, int meshIdx) {
    int vertexCount = mesh->tCustom.numVertices;
    glUniform1i(renderer->pbr_u_flatUV, 0);
    switch (mesh->type) {
      case CUSTOM:
        glBindBuffer(GL_ARRAY_BUFFER, renderer->vbos[meshIdx + BUFF_START_USER]);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebos[meshIdx + BUFF_START_USER]);
        vertexCount = mesh->tCustom.numIndices;
        break;
    }
    return vertexCount;
  }

  ENGINE_API void rendererBeginHDR(Renderer* renderer) {
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbos[FBO_HDR_PASS]);
    glUtilAttachRenderBuffer(renderer, RBO_HDR_PASS_DEPTH, GL_DEPTH_STENCIL_ATTACHMENT, GL_DEPTH24_STENCIL8);
    glUtilAttachScreenTexture(renderer, TEXT_ATTACHMENT_COLOR, GL_COLOR_ATTACHMENT0, GL_RGB, GL_RGB16F);
    glUtilAttachScreenTexture(renderer, TEXT_ATTACHMENT_BLOOM, GL_COLOR_ATTACHMENT1, GL_RGB, GL_RGB16F);
    glDrawBuffers(2, attachments);
    VERIFY_FRAMEBUFFER;
  }

  ENGINE_API int rendererFilterGauss(Renderer* renderer, int src, int pingTexture, int pongTexture, int passes) {
    int fbo = FBO_GAUSS_PASS_PING;
    int out = pingTexture;
    int in  = src;

    glUseProgram(renderer->programGaussFilter);
    for (int i = 0; i < 2 * passes; i++) {
      bool horizontal = i % 2;
      glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbos[FBO_GAUSS_PASS_PING + horizontal]);
      glUtilAttachScreenTexture(renderer, out, GL_COLOR_ATTACHMENT0, GL_RGB, GL_RGB16F);
      glDrawBuffers(1, attachments);
      VERIFY_FRAMEBUFFER;
      glUniform1i(renderer->filter_gauss_u_input, in);
      glUniform1i(renderer->filter_gauss_u_horizontal, 1);
      glUtilRenderScreenQuad();
      in  = out;
      out = horizontal ? pingTexture : pongTexture;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return out;
  }

  ENGINE_API int rendererFilterBloom(int src, float width, float height, float filterRadius) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbos[FBO_BLOOM]);

    glm::vec2 srcSize(width, height);

    if (window->needsUpdate) {
      LOG("[RENDERER] Bloom update texture and framebuffer");
      glm::vec2 size(width, height);
      for (int i = 0; i < PARAM_BLOOM_CHAIN_LENGTH; i++) {
        size /= 2;
        bindTexture(i + TEXT_BLOOM_START);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, (int)size.x, (int)size.y, 0, GL_RGB, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        SAFETY(glActiveTexture(GL_TEXTURE0));
      }

      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[TEXT_BLOOM_START], 0);
      glDrawBuffers(1, attachments);
      VERIFY_FRAMEBUFFER;
    }


    glm::vec2 size(width, height);
    //Downsample
    {
      glUseProgram(programDownsample);
      bindTexture(src);
      glUniform2f(filter_downsample_srcResolution, size.x, size.y);
      glUniform1i(filter_downsample_srcTexture, src);
      for (int i = 0; i < PARAM_BLOOM_CHAIN_LENGTH; i++) {
        glViewport(0, 0, size.x / 2, size.y / 2);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[i + TEXT_BLOOM_START], 0);
        VERIFY_FRAMEBUFFER;
        glUtilRenderScreenQuad();
        size = size / 2.0f;
        glUniform2f(filter_downsample_srcResolution, size.x, size.y);
        glUniform1i(filter_downsample_srcTexture, i + TEXT_BLOOM_START);
      }
    }
    //Upsample
    {
      glUseProgram(programUpsample);
      glUniform1i(filter_upsample_filterRadius, filterRadius);
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE);
      glBlendEquation(GL_FUNC_ADD);

      for (int i = PARAM_BLOOM_CHAIN_LENGTH - 1; i > 0; i--) {
        int texture        = TEXT_BLOOM_START + i;
        int nextMipTexture = texture - 1;

        glUniform1i(filter_upsample_srcTexture, texture);
        glViewport(0, 0, size.x * 2, size.y * 2);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[nextMipTexture], 0);
        VERIFY_FRAMEBUFFER;
        glUtilRenderScreenQuad();

        size *= 2.0f;
      }
      glDisable(GL_BLEND);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window->width, window->height);
    return TEXT_BLOOM_START;
  }

  ENGINE_API void renderScene(Renderer* renderer, Scene* scene, Stage* stage, float* viewMat, float* projMat) {

    glUniformMatrix4fv(renderer->pbr_u_ViewMat, 1, 0, viewMat);
    glUniformMatrix4fv(renderer->pbr_u_ProjMat, 1, 0, projMat);

    for (int d = 0; d < stage->instances.size(); d++) {
      ObjectInstanceGroup* g    = &stage->instances[d];
      Object*              obj  = &stage->objects[g->object];
      Mesh*                mesh = &scene->meshes[obj->mesh];
      Material*            mat  = &scene->materials[obj->material];


      VERIFY(valid(stage->objects, g->object), "Invalid object index %d\n", g->object);
      VERIFY(valid(scene->materials, obj->material), "Invalid material index %d\n", obj->material);
      VERIFY(valid(scene->meshes, obj->mesh), "Invalid mesh index %d\n", obj->mesh);

      //Bind mesh and materials
      int vertexCount = bindMesh(renderer, mesh, obj->mesh);
      if (mesh->type == CUSTOM) glUtilsSetVertexAttribs(mesh->tCustom.meshFormat);
      bindMaterial(renderer, mat);

      for (int i = 0; i < g->transforms.size(); i++) {
        glUniformMatrix4fv(renderer->pbr_u_WorldMat, 1, 0, (float*)&g->transforms[i][0][0]);
        glDrawElements(GL_TRIANGLES, vertexCount, GL_UNSIGNED_INT, 0);
      }
    }
  }

  ENGINE_API void rendererPass(Renderer* renderer, Scene* scene) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(renderer->programPbr);

    Stage* stage = scene->currentStage();

    float* viewMat = lin::viewMatrix(stage->camPos, stage->camDir);
    float* projMat = lin::projMatrix(renderer->window->ra);

    VERIFY(stage->skyTexture >= 0 && stage->skyTexture < scene->textures.size(), "Invalid sky texture\n");
    glUniform1i(renderer->pbr_u_envMap, stage->skyTexture + TEXT_START_USER);
    glUniform3f(renderer->pbr_u_ro, stage->camPos.x, stage->camPos.y, stage->camPos.z);
    glUniform3f(renderer->pbr_u_rd, stage->camDir.x, stage->camDir.y, stage->camDir.z);

    glUniform1i(renderer->pbr_u_isBack, 1);
    glUtilRenderQuad(renderer->vbos[BUFF_PLAIN], renderer->ebos[BUFF_PLAIN], renderer->pbr_u_WorldMat, renderer->pbr_u_ViewMat, renderer->pbr_u_ProjMat);
    glUniform1i(renderer->pbr_u_isBack, 0);
    renderScene(renderer, scene, stage, viewMat, projMat);
  }

  ENGINE_API void rendererEnd() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
  ENGINE_API void rendererHDR(Renderer* renderer, Scene* scene) {
    rendererBeginHDR(renderer);
    rendererPass(renderer, scene);
    rendererEnd();

    //int gaussBloomResult = rendererFilterGauss(renderer, TEXT_ATTACHMENT_BLOOM, TEXT_GAUSS_RESULT0, TEXT_GAUSS_RESULT02, desc.gauss_passes);
    int bloomResult = rendererFilterBloom(TEXT_ATTACHMENT_BLOOM, window->width, window->height, 3.0f);
    glUseProgram(renderer->programPostHDR);
    glUniform1i(renderer->hdr_u_bloom, bloomResult);
    glUniform1i(renderer->hdr_u_color, TEXT_ATTACHMENT_COLOR);

    VERIFY(renderer->hdr_u_bloom != renderer->hdr_u_color, "Invalid uniform values\n");
    glUtilRenderScreenQuad();
  }

  ENGINE_API void rendererRegular(Renderer* renderer, Scene* scene) {
    rendererPass(renderer, scene);
  }
  ENGINE_API void render(Scene* scene) override {

    if (window->width <= 0 || window->height <= 0) return;

    VERIFY(checkScene(scene), "Invalid scene graph\n");
    glBindVertexArray(vao);
    rendererHDR(this, scene);
    window->needsUpdate = 0;
    glBindVertexArray(0);
  }

  ENGINE_API int pollEvents() override {
    glfwSwapBuffers((GLFWwindow*)window->windowPtr);
    glfwPollEvents();
    return !glfwWindowShouldClose((GLFWwindow*)window->windowPtr);
  }

  RendererInput input() override {
    RendererInput in;
    in.keyboard = window->keyboard;
    in.x        = window->x;
    in.y        = window->y;
    return in;
  }
};

ENGINE_API IRenderer* rendererCreate(RendererDesc desc) {
  Renderer* renderer = new Renderer;
  renderer->desc     = desc;
  renderer->window   = windowCreate(renderer->desc.width, renderer->desc.height);
  if (renderer->window == NULL) {
    ERROR("Failed creating window.\n");
    exit(1);
  }
  renderer->textures.resize(MAX_OBJECTS);
  renderer->vbos.resize(MAX_OBJECTS);
  renderer->ebos.resize(MAX_OBJECTS);
  renderer->fbos.resize(MAX_OBJECTS);
  renderer->rbos.resize(MAX_OBJECTS);

  // GL configuration
  {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glEnable(GL_MULTISAMPLE);

#ifdef DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);
#endif
  }
  // GL gen
  {
    glGenVertexArrays(1, &renderer->vao);
    glGenBuffers(renderer->ebos.size(), renderer->ebos.data());
    glGenBuffers(renderer->vbos.size(), renderer->vbos.data());
    glGenTextures(renderer->textures.size(), renderer->textures.data());
    glGenFramebuffers(renderer->fbos.size(), renderer->fbos.data());
    glGenRenderbuffers(renderer->rbos.size(), renderer->rbos.data());
  }

  {
    glBindVertexArray(renderer->vao);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
  }

  renderer->programPbr         = glUtilLoadProgram("assets/pbr.vs", "assets/pbr.fs");
  renderer->programGaussFilter = glUtilLoadProgram("assets/filter.vs", "assets/gauss.fs");
  renderer->programPostHDR     = glUtilLoadProgram("assets/filter.vs", "assets/hdr.fs");
  renderer->programUpsample    = glUtilLoadProgram("assets/filter.vs", "assets/upsample.fs");
  renderer->programDownsample  = glUtilLoadProgram("assets/filter.vs", "assets/downsample.fs");

#define UNIFORM_ASSIGN(u)                                             \
  renderer->pbr_##u = glGetUniformLocation(renderer->programPbr, #u); \
  //LOG(#u " -> %d\n", renderer->pbr_##u);                              \
    //VERIFY(renderer->pbr_##u != -1, "Invalid uniform\n");
  UNIFORMLIST(UNIFORM_ASSIGN)
#undef UNIFORM_ASSIGN

#define UNIFORM_ASSIGN(u)                                                 \
  renderer->hdr_##u = glGetUniformLocation(renderer->programPostHDR, #u); \
  //LOG(#u " -> %d\n", renderer->hdr_##u);                                  \
    //VERIFY(renderer->hdr_##u != -1, "Invalid uniform\n");
  UNIFORMLIST_HDR(UNIFORM_ASSIGN)
#undef UNIFORM_ASSIGN

#define UNIFORM_ASSIGN(u)                                   \
  renderer->filter_gauss_##u =                              \
    glGetUniformLocation(renderer->programGaussFilter, #u); \
  //LOG(#u " -> %d\n", renderer->filter_gauss_##u);           \
    VERIFY(renderer->filter_gauss_##u != -1, "Invalid uniform\n");
  UNIFORMLIST_GAUSS(UNIFORM_ASSIGN)
#undef UNIFORM_ASSIGN

#define UNIFORM_ASSIGN(u)                                \
  renderer->filter_upsample_##u =                        \
    glGetUniformLocation(renderer->programUpsample, #u); \
  LOG(#u " -> %d\n", renderer->filter_upsample_##u);     \
  VERIFY(renderer->filter_upsample_##u != -1, "Invalid uniform\n");
  UNIFORMLIST_UPSAMPLE(UNIFORM_ASSIGN)
#undef UNIFORM_ASSIGN

#define UNIFORM_ASSIGN(u)                                  \
  renderer->filter_downsample_##u =                        \
    glGetUniformLocation(renderer->programDownsample, #u); \
  LOG(#u " -> %d\n", renderer->filter_downsample_##u);     \
  VERIFY(renderer->filter_downsample_##u != -1, "Invalid uniform\n");
  UNIFORMLIST_DOWNSAMPLE(UNIFORM_ASSIGN)
#undef UNIFORM_ASSIGN

  LOG("[Renderer] Render create completed.\n");
  return renderer;
}


} // namespace GLEngine
