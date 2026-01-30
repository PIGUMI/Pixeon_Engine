#ifndef _SCRIPT_PROPERTY_H_
#define _SCRIPT_PROPERTY_H_

#include <string>
#include <vector>
#include <functional>
#include <sstream>

enum class PropertyType
{
    FLOAT,
    INT,
    BOOL,
    STRING,
};

// プロパティのメタデータ
struct PropertyMetadata
{
    std::string name;           // 変数名
    PropertyType type;          // 型
    void* dataPtr;             // データへのポインタ
    float minValue = 0.0f;     // 数値型の最小値
    float maxValue = 100.0f;   // 数値型の最大値
    bool hasRange = false;     // 範囲制限があるか

    PropertyMetadata(const std::string& n, PropertyType t, void* ptr)
        : name(n), type(t), dataPtr(ptr) {
    }

    PropertyMetadata(const std::string& n, PropertyType t, void* ptr, float min, float max)
        : name(n), type(t), dataPtr(ptr), minValue(min), maxValue(max), hasRange(true) {
    }
};

class ScriptPropertyBase
{
public:
    virtual ~ScriptPropertyBase() = default;
    virtual std::vector<PropertyMetadata> GetProperties() = 0;
    virtual std::string SerializeProperty(const std::string& name) = 0;
    virtual void DeserializeProperty(const std::string& name, const std::string& value) = 0;
};

#define BEGIN_SCRIPT_PROPERTIES() \
    public: \
    std::vector<PropertyMetadata> GetProperties() override { \
        std::vector<PropertyMetadata> props;

#define SCRIPT_PROPERTY(type, varName) \
        props.push_back(PropertyMetadata(#varName, type, &varName));

#define SCRIPT_PROPERTY_RANGE(type, varName, minVal, maxVal) \
        props.push_back(PropertyMetadata(#varName, type, &varName, minVal, maxVal));

#define END_SCRIPT_PROPERTIES() \
        return props; \
    }

#define BEGIN_SERIALIZE_PROPERTIES() \
    std::string SerializeProperty(const std::string& name) override {

#define SERIALIZE_PROPERTY(varName) \
        if (name == #varName) { \
            std::ostringstream ss; \
            ss << varName; \
            return ss.str(); \
        }

#define END_SERIALIZE_PROPERTIES() \
        return ""; \
    }

#define BEGIN_DESERIALIZE_PROPERTIES() \
    void DeserializeProperty(const std::string& name, const std::string& value) override {

#define DESERIALIZE_PROPERTY(varName) \
        if (name == #varName) { \
            std::istringstream ss(value); \
            ss >> varName; \
            return; \
        }

#define END_DESERIALIZE_PROPERTIES() \
    }

#endif // _SCRIPT_PROPERTY_H_