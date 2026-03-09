#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <Windows.h>
#include <d3d11.h>

enum class ShaderStage { VS, PS };

class ShaderManager {
public:
    static ShaderManager* GetInstance();
    static void DestroyInstance();

    void Initialize(ID3D11Device* device);
    void Finalize();

    bool CreateHLSLTemplate(const std::string& shaderName, const std::string& type);

    void UpdateAndCompileShaders();

    ID3D11VertexShader* GetVertexShader(const std::string& name);
    ID3D11PixelShader* GetPixelShader(const std::string& name);

    bool CreateConstantBuffer(const std::string& key, UINT bufferSize);
    bool WriteBuffer(const std::string& key, UINT slot, void* pData, UINT dataSize);
    ID3D11Buffer* GetConstantBuffer(const std::string& key, UINT slot);

    std::vector<std::string> GetShaderList(const std::string& type) const;

    bool GetVSBytecode(const std::string& name, const void** ppData, size_t* pSize) const;

    struct VariableDesc {
        std::string name;
        UINT        offset = 0;
        UINT        size = 0;
    };

    struct CBufferRuntime {
        std::string                               name;
        UINT                                      bindPoint = 0;
        UINT                                      size = 0;
        ID3D11Buffer* gpuBuffer = nullptr;
        std::vector<uint8_t>                      cpuData;
        bool                                      dirty = false;
        std::unordered_map<std::string, VariableDesc> varsByName;
    };

    struct TextureBindDesc {
        std::string name;
        UINT        bindPoint = 0;
    };

    struct ShaderReflectionData {
        std::vector<CBufferRuntime>  cbuffers;
        std::unordered_map<std::string, size_t> cbufIndexByName;

        std::vector<TextureBindDesc> textures;
        std::unordered_map<std::string, UINT> texBindPointByName;
    };

    const ShaderReflectionData* GetReflection(ShaderStage stage, const std::string& shaderName) const;

    bool SetCBufferVariable(ShaderStage stage,
        const std::string& shaderName,
        const std::string& cbName,
        const std::string& varName,
        const void* data, UINT size);

    bool SetCBufferRaw(ShaderStage stage,
        const std::string& shaderName,
        const std::string& cbName,
        const void* data, UINT size);

    bool CommitAndBind(ShaderStage stage, const std::string& shaderName);

    bool BindSRV(ShaderStage stage,
        const std::string& shaderName,
        const std::string& texVarName,
        ID3D11ShaderResourceView* srv);

private:
    ShaderManager() = default;
    ~ShaderManager() {}

    bool CompileHLSL(const std::string& hlslPath, const std::string& entry,
        const std::string& target, const std::string& csoPath);
    bool LoadCSO(const std::string& csoPath, const std::string& type, const std::string& name);
    bool ReflectShader(ShaderStage stage, const std::string& shaderName,
        const void* bytecode, size_t size);

    ID3D11Device* m_device = nullptr;

    std::unordered_map<std::string, ID3D11VertexShader*> m_vsShaders;
    std::unordered_map<std::string, ID3D11PixelShader*>  m_psShaders;

    std::unordered_map<std::string, std::vector<ID3D11Buffer*>> m_constantBuffers;

    std::unordered_map<std::string, std::vector<char>> m_vsBytecodes;
    std::unordered_map<std::string, std::vector<char>> m_psBytecodes;

    std::unordered_map<std::string, ShaderReflectionData> m_vsRef;
    std::unordered_map<std::string, ShaderReflectionData> m_psRef;

    std::unordered_map<std::string, FILETIME> m_hlslUpdateTimes;

    static ShaderManager* instance;
};