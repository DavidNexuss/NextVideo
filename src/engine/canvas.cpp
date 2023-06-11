#include <video.hpp>
#include <vector>

using namespace NextVideo;

struct Canvas : public ICanvas{

  struct Batch {
    int baseIndex = 0;
    int currentIndex = 0;
  };

  Batch batch;

  Canvas(ICanvasContext* ctx) : ICanvas(ctx) {
    this->ctx = ctx;
  }

  void setColor(glm::vec4 color) override { 
    this->currentColor = color; 
  };

  void setLineWidth(float lineWidth) override { 
    this->currentLineWidth = lineWidth;
  };

  void move(glm::vec3 position) override { 
    if(batch.baseIndex != 0) { 
      ctx->pushIndex(batch.baseIndex);
    }
    batch.baseIndex = batch.currentIndex;
    push(position);
  };

  void push(glm::vec3 position) override { 
    ctx->pushVertex(getVertex(position));
    ctx->pushIndex(batch.currentIndex);
    batch.currentIndex++;
  };


  NextVideo::ICanvasContext::CanvasContextVertex getVertex(glm::vec3 pos) { 
    NextVideo::ICanvasContext::CanvasContextVertex vtx;
    vtx.position = pos;
    vtx.color = currentColor;
    vtx.uv = currentUV;
    return vtx;
  }

  void flush() override { 
    ctx->pushIndex(batch.baseIndex);
  }

  private:
  glm::vec4 currentColor = glm::vec4(1,1,1,1);
  glm::vec2 currentUV = glm::vec2(0,0);
  float     currentLineWidth = 5.0;
  ICanvasContext* ctx;
};

ICanvas* NextVideo::createCanvas(ICanvasContext* ctx) { return new Canvas(ctx); }

