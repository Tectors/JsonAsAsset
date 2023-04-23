﻿#pragma once

class FRemoteAssetDownloader {
public:
	template <class T = UObject>
	static bool DownloadAsset(const FString& Path, const FString& Type, TObjectPtr<T>& OutObject, bool& bSuccess);
	static bool MakeTexture(const FString& Path, UTexture*& OutTexture);

	static const TSharedPtr<FJsonObject> RequestExports(const FString& Path);
};
