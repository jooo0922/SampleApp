#pragma once
#include "IDrawable.h"
#include "../video/IVideoPlayer.h"
#include <memory>

class VideoRect : public IDrawable
{
public:
  VideoRect(std::shared_ptr<IVideoPlayer> player);

public:
  void update(float dt) override;
  void draw(SkCanvas* canvas) override;

private:
  std::shared_ptr<IVideoPlayer> m_player;
};
