#pragma once
// directly taken from my other repo: https://github.com/GuilBlack/ECS/tree/master/ECS
struct vec3
{
    union
    {
        struct
        {
            float x, y, z;
        };
        struct
        {
            float r, s, t;
        };
        struct
        {
            float r, g, b;
        };
    };

    bool operator==(const vec3& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct Transform
{
    vec3 Position = { 0, 0, 0 };
    vec3 Rotation = { 0, 0, 0 };
    vec3 Scale = { 1, 1, 1 };

    Transform() = default;

    Transform(const vec3& position, const vec3& rotation, const vec3& scale)
        : Position(position)
        , Rotation(rotation)
        , Scale(scale)
    {}

    bool operator==(const Transform& other) const
    {
        return Position == other.Position && Rotation == other.Rotation && Scale == other.Scale;
    }
};

struct A
{
    int Hello = 0;

    A() = default;

    A(int hello)
        : Hello(hello)
    {}

    bool operator==(const A& other) const
    {
        return Hello == other.Hello;
    }
};

struct B
{
    std::string s;

    bool operator==(const B& other) const
    {
        return s == other.s;
    }
};

struct ComplexStruct
{
    int num = 0;
    char* characters;

    ComplexStruct()
        : num(0)
        , characters(nullptr)
    {}

    ComplexStruct(int i, char* c)
        : num(i)
        , characters(c)
    {}

    ComplexStruct(const ComplexStruct& other)
        : num(other.num)
        , characters(new char[num])
    {
        std::copy(other.characters, other.characters + num, characters);
    }

    ComplexStruct(ComplexStruct&& other) noexcept
        : num(other.num)
        , characters(other.characters)
    {
        other.characters = nullptr;
    }

    ~ComplexStruct()
    {
        delete[] characters;
    }

    ComplexStruct& operator=(const ComplexStruct& other)
    {
        if (this != &other)
        {
            num = other.num;
            characters = new char[num];
            std::copy(other.characters, other.characters + num, characters);
        }
        return *this;
    }

    ComplexStruct& operator=(ComplexStruct&& other) noexcept
    {
        if (this != &other)
        {
            num = other.num;
            characters = other.characters;
            other.characters = nullptr;
        }
        return *this;
    }
};

class ScopeTimer
{
public:
    ScopeTimer(const std::string& name)
        : name_(name), start_(std::chrono::high_resolution_clock::now())
    {}

    ~ScopeTimer()
    {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(end - start_).count();
        std::cout << "[TIMER] " << name_ << " took " << duration << " ms\n";
    }

private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
};
