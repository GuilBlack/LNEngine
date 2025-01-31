#include "AppLayer.h"
#include "Engine/Main.h"

class Application : public lne::ApplicationBase
{
public:
    Application(lne::ApplicationSettings&& settings)
        : lne::ApplicationBase(std::move(settings))
    {
        PushLayer(lnnew AppLayer());
    }
};

lne::ApplicationBase* lne::CreateApplication()
{
    return lnnew Application({
        "LNApp",
        1920, 1080,
        true
    });
}
