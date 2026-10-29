/*
* ファイル名　SpirvReflect
* 説　　　明　SPIR-V の最小限の反射と書き換え（外部ライブラリ不要の単純なパーサー）
*             glslangValidator が HLSL から生成した SPIR-V を前提にしている
*/
#include "SpirvReflect.h"
#include "../../../Common/ShaderBindings.h"
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace RHI {
	namespace {
		constexpr uint32_t SPIRV_MAGIC = 0x07230203u;

		// 使う命令とデコレーションだけ定義する
		enum Op : uint32_t {
			OpName = 5, OpMemberName = 6, OpExtension = 10, OpEntryPoint = 15,
			OpTypeInt = 21, OpTypeFloat = 22, OpTypeVector = 23, OpTypeMatrix = 24, OpTypeImage = 25,
			OpTypeSampler = 26, OpTypeSampledImage = 27, OpTypeArray = 28, OpTypeStruct = 30, OpTypePointer = 32,
			OpConstant = 43, OpVariable = 59, OpDecorate = 71, OpMemberDecorate = 72, OpDecorateString = 5632, OpMemberDecorateString = 5633,
		};
		enum Decoration : uint32_t {
			DecBlock = 2, DecRowMajor = 4, DecArrayStride = 6, DecMatrixStride = 7, DecBuiltIn = 11,
			DecLocation = 30, DecBinding = 33, DecDescriptorSet = 34, DecOffset = 35, DecUserSemantic = 5635,
		};
		enum StorageClass : uint32_t { UniformConstant = 0, Input = 1, Uniform = 2, Output = 3 };

		std::string LiteralString(const uint32_t* words, size_t count)
		{
			std::string s;
			for (size_t i = 0; i < count; ++i) {
				for (int b = 0; b < 4; ++b) {
					const char c = (char)((words[i] >> (b * 8)) & 0xFF);
					if (c == '\0') return s;
					s.push_back(c);
				}
			}
			return s;
		}

		struct TypeInfo {
			uint32_t i_Op = 0;
			std::vector<uint32_t> operands;   // 結果 ID を除いたオペランド
		};
		struct MemberInfo {
			std::string name;
			uint32_t i_Offset = 0;
			uint32_t i_MatrixStride = 0;
			bool b_RowMajor = false;
		};
		struct IdInfo {
			std::string name;
			bool b_HasBinding = false;
			uint32_t i_Binding = 0;
			bool b_HasLocation = false;
			uint32_t i_Location = 0;
			size_t i_LocationWord = 0;        // Location のリテラルがあるワード位置（書き換え用）
			bool b_BuiltIn = false;
			std::string semantic;
			uint32_t i_ArrayStride = 0;
			std::vector<MemberInfo> members;
		};

		// SPIR-V 全体を 1 回走査して集めた情報
		struct Module {
			std::unordered_map<uint32_t, IdInfo> ids;
			std::unordered_map<uint32_t, TypeInfo> types;
			std::unordered_map<uint32_t, uint32_t> constants;
			struct Variable { uint32_t i_Id, i_PointerType, i_Storage; };
			std::vector<Variable> variables;
			std::vector<uint32_t> interfaceIds;   // OpEntryPoint の入出力

			bool Parse(const uint32_t* code, size_t count)
			{
				if (count < 5 || code[0] != SPIRV_MAGIC) return false;
				size_t i = 5;
				while (i < count) {
					const uint32_t op = code[i] & 0xFFFF;
					const uint32_t wc = code[i] >> 16;
					if (wc == 0 || i + wc > count) return false;
					const uint32_t* w = code + i;
					switch (op) {
					case OpName:
						ids[w[1]].name = LiteralString(w + 2, wc - 2);
						break;
					case OpMemberName: {
						auto& m = ids[w[1]].members;
						if (m.size() <= w[2]) m.resize(w[2] + 1);
						m[w[2]].name = LiteralString(w + 3, wc - 3);
						break;
					}
					case OpEntryPoint: {
						// [モデル][関数 ID][名前…][インターフェース ID…]
						size_t p = 3;
						while (p < wc) { const uint32_t v = w[p++]; if (((v >> 24) & 0xFF) == 0) break; }
						for (; p < wc; ++p) interfaceIds.push_back(w[p]);
						break;
					}
					case OpDecorate: {
						IdInfo& info = ids[w[1]];
						switch (w[2]) {
						case DecBinding: info.b_HasBinding = true; info.i_Binding = w[3]; break;
						case DecLocation: info.b_HasLocation = true; info.i_Location = w[3]; info.i_LocationWord = i + 3; break;
						case DecBuiltIn: info.b_BuiltIn = true; break;
						case DecArrayStride: info.i_ArrayStride = w[3]; break;
						default: break;
						}
						break;
					}
					case OpDecorateString:
						if (w[2] == DecUserSemantic) ids[w[1]].semantic = LiteralString(w + 3, wc - 3);
						break;
					case OpMemberDecorate: {
						auto& m = ids[w[1]].members;
						if (m.size() <= w[2]) m.resize(w[2] + 1);
						switch (w[3]) {
						case DecOffset: m[w[2]].i_Offset = w[4]; break;
						case DecMatrixStride: m[w[2]].i_MatrixStride = w[4]; break;
						case DecRowMajor: m[w[2]].b_RowMajor = true; break;
						default: break;
						}
						break;
					}
					case OpTypeInt: case OpTypeFloat: case OpTypeVector: case OpTypeMatrix: case OpTypeImage:
					case OpTypeSampler: case OpTypeSampledImage: case OpTypeArray: case OpTypeStruct: case OpTypePointer: {
						TypeInfo t;
						t.i_Op = op;
						t.operands.assign(w + 2, w + wc);
						types[w[1]] = std::move(t);
						break;
					}
					case OpConstant:
						if (wc >= 4) constants[w[2]] = w[3];
						break;
					case OpVariable:
						variables.push_back({ w[2], w[1], w[3] });
						break;
					default:
						break;
					}
					i += wc;
				}
				return true;
			}

			const TypeInfo* TypeGet(uint32_t id) const
			{
				auto it = types.find(id);
				return it != types.end() ? &it->second : nullptr;
			}

			// ポインタ型の指す先（配列なら要素）をたどる
			uint32_t PointeeGet(uint32_t pointer_type, bool* out_array = nullptr) const
			{
				const TypeInfo* p = TypeGet(pointer_type);
				if (!p || p->i_Op != OpTypePointer || p->operands.size() < 2) return 0;
				uint32_t t = p->operands[1];
				const TypeInfo* ti = TypeGet(t);
				if (ti && ti->i_Op == OpTypeArray) {
					if (out_array) *out_array = true;
					t = ti->operands[0];
				}
				return t;
			}

			/*
			* 関数名　SizeGet
			* 引　数　type：型 ID　matrix_stride / row_major：構造体メンバーのデコレーション
			* 戻り値　定数バッファ内でのバイト数
			*/
			uint32_t SizeGet(uint32_t type, uint32_t matrix_stride = 16, bool row_major = false) const
			{
				const TypeInfo* t = TypeGet(type);
				if (!t) return 0;
				switch (t->i_Op) {
				case OpTypeInt:
				case OpTypeFloat:  return t->operands[0] / 8;
				case OpTypeVector: return SizeGet(t->operands[0]) * t->operands[1];
				case OpTypeMatrix: {
					// HLSL の行列は最後の行 (列) が詰められる（float3x3 = 16*2 + 12）
					const uint32_t columns = t->operands[1];
					const TypeInfo* col = TypeGet(t->operands[0]);
					const uint32_t rows = col ? col->operands[1] : 4;
					const uint32_t scalar = col ? SizeGet(col->operands[0]) : 4;
					const uint32_t lines = row_major ? rows : columns;
					const uint32_t per_line = (row_major ? columns : rows) * scalar;
					return matrix_stride * (lines - 1) + per_line;
				}
				case OpTypeArray: {
					auto it = constants.find(t->operands[1]);
					const uint32_t len = it != constants.end() ? it->second : 1;
					auto info = ids.find(type);
					const uint32_t stride = (info != ids.end() && info->second.i_ArrayStride) ? info->second.i_ArrayStride : SizeGet(t->operands[0]);
					return stride * (len - 1) + SizeGet(t->operands[0], matrix_stride, row_major);
				}
				case OpTypeStruct: {
					uint32_t size = 0;
					auto info = ids.find(type);
					for (size_t m = 0; m < t->operands.size(); ++m) {
						const MemberInfo* mi = (info != ids.end() && m < info->second.members.size()) ? &info->second.members[m] : nullptr;
						const uint32_t off = mi ? mi->i_Offset : 0;
						const uint32_t ms = (mi && mi->i_MatrixStride) ? mi->i_MatrixStride : 16;
						size = std::max(size, off + SizeGet(t->operands[m], ms, mi && mi->b_RowMajor));
					}
					return size;
				}
				default: return 0;
				}
			}
		};
	}

	void SemanticSplit(const std::string& semantic, std::string& out_name, uint32_t& out_index)
	{
		size_t end = semantic.size();
		while (end > 0 && std::isdigit((unsigned char)semantic[end - 1])) --end;
		out_name = semantic.substr(0, end);
		std::transform(out_name.begin(), out_name.end(), out_name.begin(), [](unsigned char c) { return (char)std::toupper(c); });
		out_index = (end < semantic.size()) ? (uint32_t)std::stoul(semantic.substr(end)) : 0;
	}

	/*
	* 関数名　SpirvReflect
	* 引　数　code / word_count：SPIR-V　stage：ステージ　out_reflection：出力
	* 戻り値　解析できれば true
	* 説　明　定数バッファ・テクスチャ・サンプラー・入出力変数を取り出す
	*         binding 番号は ShaderBindings の配置からステージ内のスロット番号へ戻す
	*/
	bool SpirvReflect(const uint32_t* code, size_t word_count, ShaderStage stage, ShaderReflection& out)
	{
		Module m;
		if (!m.Parse(code, word_count)) return false;
		out = ShaderReflection{};
		const uint32_t base = ShaderBindings::StageBase((ShaderBindings::Stage)stage);
		using namespace ShaderBindings;

		auto is_interface = [&](uint32_t id) {
			return std::find(m.interfaceIds.begin(), m.interfaceIds.end(), id) != m.interfaceIds.end();
		};

		for (const auto& v : m.variables) {
			const IdInfo& info = m.ids[v.i_Id];
			if (v.i_Storage == Uniform || v.i_Storage == UniformConstant) {
				if (!info.b_HasBinding || info.i_Binding < base) continue;
				const uint32_t local = info.i_Binding - base;
				const uint32_t pointee = m.PointeeGet(v.i_PointerType);
				const TypeInfo* t = m.TypeGet(pointee);
				if (!t) continue;
				if (t->i_Op == OpTypeStruct && local >= CB_OFFSET && local < CB_OFFSET + CB_SLOT_COUNT) {
					ShaderConstantBuffer cb;
					const IdInfo& type_info = m.ids[pointee];
					cb.name = !type_info.name.empty() ? type_info.name : info.name;
					cb.i_Slot = local - CB_OFFSET;
					cb.i_Size = (m.SizeGet(pointee) + 15) & ~15u;
					for (size_t i = 0; i < t->operands.size() && i < type_info.members.size(); ++i) {
						const MemberInfo& mi = type_info.members[i];
						ShaderVariable var;
						var.name = mi.name;
						var.i_Offset = mi.i_Offset;
						var.i_Size = m.SizeGet(t->operands[i], mi.i_MatrixStride ? mi.i_MatrixStride : 16, mi.b_RowMajor);
						cb.variables.push_back(var);
					}
					out.constantBuffers.push_back(std::move(cb));
				}
				else if (t->i_Op == OpTypeImage && local >= SRV_OFFSET && local < SRV_OFFSET + SRV_SLOT_COUNT) {
					ShaderResourceBinding b;
					b.name = info.name;
					b.i_Slot = local - SRV_OFFSET;
					// OpTypeImage: [sampled type][dim][depth][arrayed][ms][sampled][format]
					b.b_Array = t->operands.size() > 3 && t->operands[3] != 0;
					out.textures.push_back(std::move(b));
				}
				else if (t->i_Op == OpTypeSampler && local >= SAMPLER_OFFSET && local < SAMPLER_OFFSET + SAMPLER_SLOT_COUNT) {
					ShaderResourceBinding b;
					b.name = info.name;
					b.i_Slot = local - SAMPLER_OFFSET;
					out.samplers.push_back(std::move(b));
				}
			}
			else if ((v.i_Storage == Input || v.i_Storage == Output) && !info.b_BuiltIn && info.b_HasLocation && is_interface(v.i_Id)) {
				ShaderInputAttribute a;
				SemanticSplit(info.semantic, a.semantic, a.i_SemanticIndex);
				a.i_Location = info.i_Location;
				(v.i_Storage == Input ? out.inputs : out.outputs).push_back(std::move(a));
			}
		}
		auto by_slot = [](const auto& a, const auto& b) { return a.i_Slot < b.i_Slot; };
		std::sort(out.constantBuffers.begin(), out.constantBuffers.end(), by_slot);
		std::sort(out.textures.begin(), out.textures.end(), by_slot);
		std::sort(out.samplers.begin(), out.samplers.end(), by_slot);
		return true;
	}

	/*
	* 関数名　SpirvInputLocationsRemap
	* 引　数　pixel_code：PS の SPIR-V　vertex_outputs：VS 出力の反射情報
	* 戻り値　書き換えた SPIR-V
	* 説　明　PS 入力の location を、同じセマンティクスを持つ VS 出力の location に合わせる
	*/
	std::vector<uint32_t> SpirvInputLocationsRemap(const std::vector<uint32_t>& pixel_code,
		const std::vector<ShaderInputAttribute>& vertex_outputs)
	{
		std::vector<uint32_t> code = pixel_code;
		Module m;
		if (!m.Parse(code.data(), code.size())) return code;
		for (const auto& v : m.variables) {
			if (v.i_Storage != Input) continue;
			auto it = m.ids.find(v.i_Id);
			if (it == m.ids.end() || it->second.b_BuiltIn || !it->second.b_HasLocation || it->second.i_LocationWord == 0) continue;
			std::string name;
			uint32_t index = 0;
			SemanticSplit(it->second.semantic, name, index);
			for (const auto& out : vertex_outputs) {
				if (out.semantic == name && out.i_SemanticIndex == index) {
					code[it->second.i_LocationWord] = out.i_Location;
					break;
				}
			}
		}
		return code;
	}

	/*
	* 関数名　SpirvSemanticsStrip
	* 引　数　code：SPIR-V
	* 戻り値　拡張命令を除いた SPIR-V
	* 説　明　OpExtension "SPV_GOOGLE_*" と UserSemantic の OpDecorateString を取り除く
	*/
	std::vector<uint32_t> SpirvSemanticsStrip(const std::vector<uint32_t>& code)
	{
		if (code.size() < 5 || code[0] != SPIRV_MAGIC) return code;
		std::vector<uint32_t> out(code.begin(), code.begin() + 5);
		out.reserve(code.size());
		size_t i = 5;
		while (i < code.size()) {
			const uint32_t op = code[i] & 0xFFFF;
			const uint32_t wc = code[i] >> 16;
			if (wc == 0 || i + wc > code.size()) return code;
			bool skip = false;
			if (op == OpExtension) {
				const std::string name = LiteralString(&code[i + 1], wc - 1);
				skip = (name == "SPV_GOOGLE_hlsl_functionality1" || name == "SPV_GOOGLE_decorate_string");
			}
			else if (op == OpDecorateString) {
				skip = (wc >= 3 && code[i + 2] == DecUserSemantic);
			}
			else if (op == OpMemberDecorateString) {
				skip = (wc >= 4 && code[i + 3] == DecUserSemantic);
			}
			if (!skip) out.insert(out.end(), code.begin() + i, code.begin() + i + wc);
			i += wc;
		}
		return out;
	}
}
