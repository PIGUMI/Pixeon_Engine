#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <Windows.h>
#include <d3d11.h>

// memo
// WriteBufferを反射機能で実装

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

    // 入力レイアウト用にVSバイトコード取得
    bool GetVSBytecode(const std::string& name, const void** ppData, size_t* pSize) const;

    // ここから追加: 反射/可変CB管理
    struct VariableDesc {
        std::string name;
        UINT        offset = 0;
        UINT        size = 0;
    };
    struct CBufferRuntime {
        std::string                     name;
        UINT                            bindPoint = 0;  // register(bX)
        UINT                            size = 0;  // 16の倍数
        ID3D11Buffer* gpuBuffer = nullptr;
        std::vector<uint8_t>            cpuData;
        bool                            dirty = false;
        std::unordered_map<std::string, VariableDesc> varsByName;
    };
    struct ShaderReflectionData {
        std::vector<CBufferRuntime> cbuffers; // 複数存在しうる
        // 名前 -> index の検索支援
        std::unordered_map<std::string, size_t> cbufIndexByName;
    };

    // 反射情報取得
    const ShaderReflectionData* GetReflection(ShaderStage stage, const std::string& shaderName) const;

    // 変数名でセット（存在した場合のみ書き込む）
    bool SetCBufferVariable(ShaderStage stage,
        const std::string& shaderName,
        const std::string& cbName,
        const std::string& varName,
        const void* data, UINT size);

    // cbuffer全体を書き込み（サイズ一致時に有効）
    bool SetCBufferRaw(ShaderStage stage,
        const std::string& shaderName,
        const std::string& cbName,
        const void* data, UINT size);

    // 変更の入ったcbufferをすべてUpdateSubresourceし、適切なBindPointにバインド
    bool CommitAndBind(ShaderStage stage, const std::string& shaderName);

private:
    ShaderManager() = default;
    ~ShaderManager() {};

    bool CompileHLSL(const std::string& hlslPath, const std::string& entry, const std::string& target, const std::string& csoPath);
    bool LoadCSO(const std::string& csoPath, const std::string& type, const std::string& name);

    // 反射の実体
    bool ReflectShader(ShaderStage stage, const std::string& shaderName, const void* bytecode, size_t size);

    ID3D11Device* m_device = nullptr;

    std::unordered_map<std::string, ID3D11VertexShader*> m_vsShaders;
    std::unordered_map<std::string, ID3D11PixelShader*>  m_psShaders;

    // 定数バッファ（従来の固定キー版）
    std::unordered_map<std::string, std::vector<ID3D11Buffer*>> m_constantBuffers;

    // VS/PSバイトコード保持（入力レイアウト・反射に使用）
    std::unordered_map<std::string, std::vector<char>> m_vsBytecodes;
    std::unordered_map<std::string, std::vector<char>> m_psBytecodes;

    // 反射データ
    std::unordered_map<std::string, ShaderReflectionData> m_vsRef;
    std::unordered_map<std::string, ShaderReflectionData> m_psRef;

    std::unordered_map<std::string, FILETIME> m_hlslUpdateTimes;

    static ShaderManager* instance;
};