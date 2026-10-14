// esp.hpp — GUI-ESP поверх EspCore: только отрисовка ImGui/D3D.
#pragma once
#include "esp_core.hpp"

#include "imgui/imgui.h"

class Esp : public EspCore {
public:
  void Draw(class Overlay* ov, const EspStyle& st);
};

