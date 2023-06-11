#include <video.hpp>
#include <vector>

using namespace NextVideo;

struct Canvas : public ICanvas{

  struct Batch {
    int currentIndex = 0;
    std::vector<int> indexStack;
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

  void resetIndex() { 
    batch.indexStack.clear();
  }

  void pushIndex() { 
    if(batch.indexStack.size() == 2) { 
      this->ctx->pushIndex(batch.indexStack[0]);
      this->ctx->pushIndex(batch.indexStack[1]);
      this->ctx->pushIndex(batch.currentIndex);

      batch.indexStack[0] = batch.indexStack[1];
      batch.indexStack[1] = batch.currentIndex;
    }
    else {
      batch.indexStack.push_back(batch.currentIndex);
    }
    batch.currentIndex++;
  }

  void move(glm::vec3 position) override { 
    resetIndex();
    push(position);
  };

  void push(glm::vec3 position) override { 
    ctx->pushVertex(getVertex(position));
    pushIndex();
  };


  NextVideo::ICanvasContext::CanvasContextVertex getVertex(glm::vec3 pos) { 
    NextVideo::ICanvasContext::CanvasContextVertex vtx;
    vtx.position = pos;
    vtx.color = currentColor;
    vtx.uv = currentUV;
    return vtx;
  }

  void flush() override { 
    resetIndex();
  }

  private:
  glm::vec4 currentColor = glm::vec4(1,1,1,1);
  glm::vec2 currentUV = glm::vec2(0,0);
  float     currentLineWidth = 5.0;
  ICanvasContext* ctx;
};

ICanvas* NextVideo::createCanvas(ICanvasContext* ctx) { return new Canvas(ctx); }

