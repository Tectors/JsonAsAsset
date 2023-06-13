﻿#include "Importers/TextureImporter.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureCube.h"
#include "Factories/TextureFactory.h"
#include "Factories/TextureRenderTargetFactoryNew.h"
#include "Utilities/MathUtilities.h"
#include <IImageWrapper.h>

#include "Factories.h"
#include "IImageWrapperModule.h"

bool UTextureImporter::ImportTexture2D(UTexture*& OutTexture2D, const TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const {
	const TSharedPtr<FJsonObject> SubObjectProperties = Properties->GetObjectField("Properties");

	// NEW: .bin support
	UTexture2D* Texture2D =
		NewObject<UTexture2D>(OutermostPkg, UTexture2D::StaticClass(), *FileName, RF_Standalone | RF_Public); 
	Texture2D->SetPlatformData(new FTexturePlatformData());

	ImportTexture2D_Data(Texture2D, SubObjectProperties);
	FTexturePlatformData* PlatformData = Texture2D->GetPlatformData();

	if (FString PixelFormat; Properties->TryGetStringField("PixelFormat", PixelFormat)) PlatformData->PixelFormat = static_cast<EPixelFormat>(Texture2D->GetPixelFormatEnum()->GetValueByNameString(PixelFormat));

	float SizeX = Properties->GetNumberField("SizeX");
	float SizeY = Properties->GetNumberField("SizeY");

	Texture2D->Source.Init(SizeX, SizeY, 1, 1, 
		TSF_BGRA8 // Maybe HDR here?
	);

	uint8_t* dest = Texture2D->Source.LockMip(0);
	FMemory::Memcpy(dest, Data.GetData(), Data.Num());

	Texture2D->Source.UnlockMip(0);
	Texture2D->UpdateResource();

	if (Texture2D) {
		OutTexture2D = Texture2D;
		return true;
	}

	return false;
}

bool UTextureImporter::ImportTextureCube(UTexture*& OutTextureCube, const TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const {
	UTextureCube* TextureCube = NewObject<UTextureCube>(Package, UTextureCube::StaticClass(), *FileName, RF_Public | RF_Standalone);

	float InSizeX = Properties->GetNumberField("SizeX");
	float InSizeY = Properties->GetNumberField("SizeY");

	TextureCube->Source.Init(InSizeX, InSizeY, 6, 1, ETextureSourceFormat::TSF_BGRA8);

	int32 MipSize = CalculateImageBytes(InSizeX, InSizeY, 0, ETextureSourceFormat::TSF_BGRA8);
	uint8* SliceData = TextureCube->Source.LockMip(0);
	/*switch (ETextureSourceFormat::TSF_BGRA8)
	{
		case TSF_BGRA8:
		{
			TArray<FColor> OutputBuffer;
			for (int32 SliceIndex = 0; SliceIndex < 6; SliceIndex++)
			{
				if (CubeResource->ReadPixels(OutputBuffer, FReadSurfaceDataFlags(RCM_UNorm, (ECubeFace)SliceIndex)))
				{
					FMemory::Memcpy((FColor*)(SliceData + SliceIndex * MipSize), OutputBuffer.GetData(), MipSize);
				}
			}
		}
		break;
		case TSF_RGBA16F:
		{
			TArray<FFloat16Color> OutputBuffer;
			for (int32 SliceIndex = 0; SliceIndex < 6; SliceIndex++)
			{
				if (CubeResource->ReadPixels(OutputBuffer, FReadSurfaceDataFlags(RCM_UNorm, (ECubeFace)SliceIndex)))
				{
					FMemory::Memcpy((FFloat16Color*)(SliceData + SliceIndex * MipSize), OutputBuffer.GetData(), MipSize);
				}
			}
		}
		break;
	}*/

	TextureCube->Source.UnlockMip(0);
	TextureCube->SRGB = false;
	// If HDR source image then choose HDR compression settings..
	TextureCube->CompressionSettings = ETextureSourceFormat::TSF_BGRA8 == TSF_RGBA16F ? TextureCompressionSettings::TC_HDR : TextureCompressionSettings::TC_Default;
	// Default to no mip generation for cube render target captures.
	TextureCube->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
	TextureCube->PostEditChange();

	if (TextureCube) {
		OutTextureCube = TextureCube;
		return true;
	}

	return false;
}

bool UTextureImporter::ImportVolumeTexture(UTexture*& OutTexture2D, const TArray<uint8>& Data, const TSharedPtr<FJsonObject>& Properties) const
{
	return false;
}

bool UTextureImporter::ImportRenderTarget2D(UTexture*& OutRenderTarget2D, const TSharedPtr<FJsonObject>& Properties) const {
	UTextureRenderTargetFactoryNew* TextureFactory = NewObject<UTextureRenderTargetFactoryNew>();
	TextureFactory->AddToRoot();
	UTextureRenderTarget2D* RenderTarget2D = Cast<UTextureRenderTarget2D>(TextureFactory->FactoryCreateNew(UTextureRenderTarget2D::StaticClass(), OutermostPkg, *FileName, RF_Standalone | RF_Public, nullptr, GWarn));

	ImportTexture_Data(RenderTarget2D, Properties);

	int SizeX;
	if (Properties->TryGetNumberField("SizeX", SizeX)) RenderTarget2D->SizeX = SizeX;
	int SizeY;
	if (Properties->TryGetNumberField("SizeY", SizeY)) RenderTarget2D->SizeY = SizeY;

	FString AddressX;
	if (Properties->TryGetStringField("AddressX", AddressX)) RenderTarget2D->AddressX = static_cast<TextureAddress>(StaticEnum<TextureAddress>()->GetValueByNameString(AddressX));
	FString AddressY;
	if (Properties->TryGetStringField("AddressY", AddressY)) RenderTarget2D->AddressY = static_cast<TextureAddress>(StaticEnum<TextureAddress>()->GetValueByNameString(AddressY));
	FString RenderTargetFormat;
	if (Properties->TryGetStringField("RenderTargetFormat", RenderTargetFormat)) RenderTarget2D->RenderTargetFormat = static_cast<ETextureRenderTargetFormat>(StaticEnum<ETextureRenderTargetFormat>()->GetValueByNameString(RenderTargetFormat));

	bool bAutoGenerateMips;
	if (Properties->TryGetBoolField("bAutoGenerateMips", bAutoGenerateMips)) RenderTarget2D->bAutoGenerateMips = bAutoGenerateMips;
	if (bAutoGenerateMips) {
		if (FString MipsSamplerFilter; Properties->TryGetStringField("MipsSamplerFilter", MipsSamplerFilter))
			RenderTarget2D->MipsSamplerFilter = static_cast<TextureFilter>(StaticEnum<TextureFilter>()->GetValueByNameString(MipsSamplerFilter));
	}

	const TSharedPtr<FJsonObject>* ClearColor;
	if (Properties->TryGetObjectField("ClearColor", ClearColor)) RenderTarget2D->ClearColor = FMathUtilities::ObjectToLinearColor(ClearColor->Get());

	if (RenderTarget2D) {
		OutRenderTarget2D = RenderTarget2D;
		return true;
	}

	return false;
}

// Handle UTexture2D
bool UTextureImporter::ImportTexture2D_Data(UTexture2D* InTexture2D, const TSharedPtr<FJsonObject>& Properties) const {
	if (InTexture2D == nullptr) return false;

	ImportTexture_Data(InTexture2D, Properties);

	if (FString AddressX; Properties->TryGetStringField("AddressX", AddressX)) InTexture2D->AddressX = static_cast<TextureAddress>(StaticEnum<TextureAddress>()->GetValueByNameString(AddressX));
	if (FString AddressY; Properties->TryGetStringField("AddressY", AddressY)) InTexture2D->AddressY = static_cast<TextureAddress>(StaticEnum<TextureAddress>()->GetValueByNameString(AddressY));
	if (bool bHasBeenPaintedInEditor; Properties->TryGetBoolField("bHasBeenPaintedInEditor", bHasBeenPaintedInEditor)) InTexture2D->bHasBeenPaintedInEditor = bHasBeenPaintedInEditor;

	// --------- Platform Data --------- //
	FTexturePlatformData* PlatformData = InTexture2D->GetPlatformData();

	if (int SizeX; Properties->TryGetNumberField("SizeX", SizeX)) PlatformData->SizeX = SizeX;
	if (int SizeY; Properties->TryGetNumberField("SizeY", SizeY)) PlatformData->SizeY = SizeY;
	if (uint32 PackedData; Properties->TryGetNumberField("PackedData", PackedData)) PlatformData->PackedData = PackedData;
	if (FString PixelFormat; Properties->TryGetStringField("PixelFormat", PixelFormat)) PlatformData->PixelFormat = static_cast<EPixelFormat>(InTexture2D->GetPixelFormatEnum()->GetValueByNameString(PixelFormat));

	if (int FirstResourceMemMip; Properties->TryGetNumberField("FirstResourceMemMip", FirstResourceMemMip)) InTexture2D->FirstResourceMemMip = FirstResourceMemMip;
	if (int LevelIndex; Properties->TryGetNumberField("LevelIndex", LevelIndex)) InTexture2D->LevelIndex = LevelIndex;

	return false;
}

// Handle UTexture
bool UTextureImporter::ImportTexture_Data(UTexture* InTexture, const TSharedPtr<FJsonObject>& Properties) const {
	if (InTexture == nullptr) return false;

	// Adjust parameters
	if (float AdjustBrightness; Properties->TryGetNumberField("AdjustBrightness", AdjustBrightness)) InTexture->AdjustBrightness = AdjustBrightness;
	if (float AdjustBrightnessCurve; Properties->TryGetNumberField("AdjustBrightnessCurve", AdjustBrightnessCurve)) InTexture->AdjustBrightnessCurve = AdjustBrightnessCurve;
	if (float AdjustHue; Properties->TryGetNumberField("AdjustHue", AdjustHue)) InTexture->AdjustHue = AdjustHue;
	if (float AdjustMaxAlpha; Properties->TryGetNumberField("AdjustMaxAlpha", AdjustMaxAlpha)) InTexture->AdjustMaxAlpha = AdjustMaxAlpha;
	if (float AdjustMinAlpha; Properties->TryGetNumberField("AdjustMinAlpha", AdjustMinAlpha)) InTexture->AdjustMinAlpha = AdjustMinAlpha;
	if (float AdjustRGBCurve; Properties->TryGetNumberField("AdjustRGBCurve", AdjustRGBCurve)) InTexture->AdjustRGBCurve = AdjustRGBCurve;
	if (float AdjustSaturation; Properties->TryGetNumberField("AdjustSaturation", AdjustSaturation)) InTexture->AdjustSaturation = AdjustSaturation;
	if (float AdjustVibrance; Properties->TryGetNumberField("AdjustVibrance", AdjustVibrance)) InTexture->AdjustVibrance = AdjustVibrance;

	if (const TSharedPtr<FJsonObject>* AlphaCoverageThresholds; Properties->TryGetObjectField("AlphaCoverageThresholds", AlphaCoverageThresholds))
		InTexture->AlphaCoverageThresholds = FMathUtilities::ObjectToVector(AlphaCoverageThresholds->Get());

	if (bool bChromaKeyTexture; Properties->TryGetBoolField("bChromaKeyTexture", bChromaKeyTexture)) InTexture->bChromaKeyTexture = bChromaKeyTexture;
	if (bool bFlipGreenChannel; Properties->TryGetBoolField("bFlipGreenChannel", bFlipGreenChannel)) InTexture->bFlipGreenChannel = bFlipGreenChannel;
	if (bool bNoTiling; Properties->TryGetBoolField("bNoTiling", bNoTiling)) InTexture->bNoTiling = bNoTiling;
	if (bool bPreserveBorder; Properties->TryGetBoolField("bPreserveBorder", bPreserveBorder)) InTexture->bPreserveBorder = bPreserveBorder;
	if (bool bUseLegacyGamma; Properties->TryGetBoolField("bUseLegacyGamma", bUseLegacyGamma)) InTexture->bUseLegacyGamma = bUseLegacyGamma;

	if (const TSharedPtr<FJsonObject>* ChromaKeyColor; Properties->TryGetObjectField("ChromaKeyColor", ChromaKeyColor)) InTexture->ChromaKeyColor = FMathUtilities::ObjectToColor(ChromaKeyColor->Get());
	if (float ChromaKeyThreshold; Properties->TryGetNumberField("ChromaKeyThreshold", ChromaKeyThreshold)) InTexture->ChromaKeyThreshold = ChromaKeyThreshold;

	if (float CompositePower; Properties->TryGetNumberField("CompositePower", CompositePower)) InTexture->CompositePower = CompositePower;
	// if (const TSharedPtr<FJsonObject>* CompositeTexture; Properties->TryGetObjectField("CompositeTexture", CompositeTexture));
	if (FString CompositeTextureMode; Properties->TryGetStringField("CompositeTextureMode", CompositeTextureMode)) InTexture->CompositeTextureMode = static_cast<ECompositeTextureMode>(StaticEnum<ECompositeTextureMode>()->GetValueByNameString(CompositeTextureMode));

	if (bool CompressionNoAlpha; Properties->TryGetBoolField("CompressionNoAlpha", CompressionNoAlpha)) InTexture->CompressionNoAlpha = CompressionNoAlpha;
	if (bool CompressionNone; Properties->TryGetBoolField("CompressionNone", CompressionNone)) InTexture->CompressionNone = CompressionNone;
	if (FString CompressionQuality; Properties->TryGetStringField("CompressionQuality", CompressionQuality)) InTexture->CompressionQuality = static_cast<ETextureCompressionQuality>(StaticEnum<ETextureCompressionQuality>()->GetValueByNameString(CompressionQuality));
	if (FString CompressionSettings; Properties->TryGetStringField("CompressionSettings", CompressionSettings)) InTexture->CompressionSettings = static_cast<TextureCompressionSettings>(StaticEnum<TextureCompressionSettings>()->GetValueByNameString(CompressionSettings));
	if (bool CompressionYCoCg; Properties->TryGetBoolField("CompressionYCoCg", CompressionYCoCg)) InTexture->CompressionYCoCg = CompressionYCoCg;
	if (bool DeferCompression; Properties->TryGetBoolField("DeferCompression", DeferCompression)) InTexture->DeferCompression = DeferCompression;
	if (FString Filter; Properties->TryGetStringField("Filter", Filter)) InTexture->Filter = static_cast<TextureFilter>(StaticEnum<TextureFilter>()->GetValueByNameString(Filter));

	// TODO: Add LayerFormatSettings

	if (FString LODGroup; Properties->TryGetStringField("LODGroup", LODGroup)) InTexture->LODGroup = static_cast<TextureGroup>(StaticEnum<TextureGroup>()->GetValueByNameString(LODGroup));
	if (FString LossyCompressionAmount; Properties->TryGetStringField("LossyCompressionAmount", LossyCompressionAmount)) InTexture->LossyCompressionAmount = static_cast<ETextureLossyCompressionAmount>(StaticEnum<ETextureLossyCompressionAmount>()->GetValueByNameString(LossyCompressionAmount));

	if (int MaxTextureSize; Properties->TryGetNumberField("MaxTextureSize", MaxTextureSize)) InTexture->MaxTextureSize = MaxTextureSize;
	if (FString MipGenSettings; Properties->TryGetStringField("MipGenSettings", MipGenSettings)) InTexture->MipGenSettings = static_cast<TextureMipGenSettings>(StaticEnum<TextureMipGenSettings>()->GetValueByNameString(MipGenSettings));
	if (FString MipLoadOptions; Properties->TryGetStringField("MipLoadOptions", MipLoadOptions)) InTexture->MipLoadOptions = static_cast<ETextureMipLoadOptions>(StaticEnum<ETextureMipLoadOptions>()->GetValueByNameString(MipLoadOptions));

	if (const TSharedPtr<FJsonObject>* PaddingColor; Properties->TryGetObjectField("PaddingColor", PaddingColor)) InTexture->PaddingColor = FMathUtilities::ObjectToColor(PaddingColor->Get());
	if (FString PowerOfTwoMode; Properties->TryGetStringField("PowerOfTwoMode", PowerOfTwoMode)) InTexture->PowerOfTwoMode = static_cast<ETexturePowerOfTwoSetting::Type>(StaticEnum<ETexturePowerOfTwoSetting::Type>()->GetValueByNameString(PowerOfTwoMode));

	if (bool SRGB; Properties->TryGetBoolField("SRGB", SRGB)) InTexture->SRGB = SRGB;
	if (bool VirtualTextureStreaming; Properties->TryGetBoolField("VirtualTextureStreaming", VirtualTextureStreaming)) InTexture->VirtualTextureStreaming = VirtualTextureStreaming;

	if (FString LightingGuid; Properties->TryGetStringField("LightingGuid", LightingGuid)) InTexture->SetLightingGuid(FGuid(LightingGuid));

	return false;
}
