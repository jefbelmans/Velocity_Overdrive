#pragma once
class DiffuseMaterial_Shadow final : public Material<DiffuseMaterial_Shadow>
{
public:
	DiffuseMaterial_Shadow();
	~DiffuseMaterial_Shadow() override = default;

	DiffuseMaterial_Shadow(const DiffuseMaterial_Shadow& other) = delete;
	DiffuseMaterial_Shadow(DiffuseMaterial_Shadow&& other) noexcept = delete;
	DiffuseMaterial_Shadow& operator=(const DiffuseMaterial_Shadow& other) = delete;
	DiffuseMaterial_Shadow& operator=(DiffuseMaterial_Shadow&& other) noexcept = delete;

	void SetDiffuseTexture(const std::wstring& assetFile);
	void SetDiffuseColor(const XMFLOAT4& color);
	void UseDiffuseMap(bool useMap);

protected:
	void InitializeEffectVariables() override;
	void OnUpdateModelVariables(const SceneContext&, const ModelComponent*) const override;

private:
	TextureData* m_pDiffuseTexture{};

	mutable bool m_IsBakedLightWVPDirty{ true };
	mutable XMFLOAT4X4 m_BakedLightWVP;
};

