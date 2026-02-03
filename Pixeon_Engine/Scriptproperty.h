
#ifndef _SCRIPT_PROPERTY_H_
#define _SCRIPT_PROPERTY_H_

#include <string>
#include <vector>
#include <sstream>

enum class PropertyType
{
    FLOAT,
    INT,
    BOOL,
    STRING
};

struct PropertyMetadata
{
    std::string name;
    PropertyType type;
    void* dataPtr;
    float minValue = 0.0f;
    float maxValue = 100.0f;
    bool hasRange = false;

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

#define IMPLEMENT_GET_PROPERTIES() \
    std::vector<PropertyMetadata> GetProperties() override { \
        std::vector<PropertyMetadata> props; \
        PROPERTY_LIST(REGISTER_PROP) \
        return props; \
    }

#define IMPLEMENT_SERIALIZE() \
    std::string SerializeProperty(const std::string& name) override { \
        std::ostringstream ss; \
        PROPERTY_LIST(SERIALIZE_PROP) \
        return ""; \
    }

#define IMPLEMENT_DESERIALIZE() \
    void DeserializeProperty(const std::string& name, const std::string& value) override { \
        std::istringstream ss(value); \
        PROPERTY_LIST(DESERIALIZE_PROP) \
    }

#define REGISTER_PROP(type, varName) \
    props.push_back(PropertyMetadata(#varName, PropertyType::type, &varName));

#define SERIALIZE_PROP(type, varName) \
    if (name == #varName) { ss << varName; return ss.str(); }

#define DESERIALIZE_PROP(type, varName) \
    if (name == #varName) { ss >> varName; return; }

#define REGISTER_PROP_RANGE(type, varName, minVal, maxVal) \
    props.push_back(PropertyMetadata(#varName, PropertyType::type, &varName, minVal, maxVal));

#define SERIALIZE_PROP_RANGE(type, varName, minVal, maxVal) \
    if (name == #varName) { ss << varName; return ss.str(); }

#define DESERIALIZE_PROP_RANGE(type, varName, minVal, maxVal) \
    if (name == #varName) { ss >> varName; return; }

#define DECLARE_SCRIPT_PROPERTIES() \
    IMPLEMENT_GET_PROPERTIES() \
    IMPLEMENT_SERIALIZE() \
    IMPLEMENT_DESERIALIZE()

#endif // _SCRIPT_PROPERTY_H_