#include <LNEInclude.h>

class Application : public lne::ApplicationBase
{
public:
    Application(lne::ApplicationSettings settings)
        : lne::ApplicationBase(settings)
    {
        int* a = new int[100];
    }
};

lne::ApplicationBase* lne::CreateApplication()
{
    return new Application({
        "LNApp",
        1280, 720
    });
}
